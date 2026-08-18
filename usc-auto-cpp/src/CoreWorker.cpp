// CoreWorker.cpp — 后台工作线程实现
// 时序对齐 Python core_module.core_task：
//   检测稳定 → 60s；WLAN 断开 → 死等 WLAN；网络不可达 → 30s 重试；
//   认证失效 → login；认证有效 → logout + 10s + login；LoginException → 10s 重试
#include "CoreWorker.h"

#include <chrono>
#include <thread>

#include "AuthClient.h"
#include "Logger.h"
#include "NetworkMonitor.h"
#include "WlanChecker.h"
#include "exceptions.h"

namespace usc {

CoreWorker::CoreWorker(AppConfig cfg, std::string configPath, QObject* parent)
    : QObject(parent),
      m_cfg(std::move(cfg)),
      m_cfgPath(std::move(configPath)),
      m_cookie(m_cfg.cookie),        // 从配置快照初始化凭证；
      m_csrfToken(m_cfg.csrfToken) {}  // 缺失或超7天才由 ensureCredentials 重新获取
// 说明：头文件声明序 m_cfg → m_cfgPath → m_cookie → m_csrfToken，
//       初始化按声明序执行，m_cookie 读 m_cfg 时后者已构造（无 -Wreorder 问题）

CoreWorker::~CoreWorker() {
    requestStop();
}

void CoreWorker::start() {
    if (m_thread.joinable()) return;  // 幂等
    m_stopping = false;
    // jthread 析构自动 request_stop + join；线程函数第一参数收 stop_token
    m_thread = std::jthread([this](std::stop_token st) { run(st); });
}

void CoreWorker::requestStop() {
    if (!m_thread.joinable()) return;
    m_stopping = true;
    m_thread.request_stop();
    {
        // 唤醒可能正在 sleep 的循环
        std::lock_guard<std::mutex> lock(m_sleepMutex);
        m_sleepCv.notify_all();
    }
    try {
        m_thread.join();
    } catch (const std::system_error&) {
        // 未启动/已 join，忽略
    }
    emit stopped();
}

bool CoreWorker::requestReauth() {
    bool expected = false;
    if (!m_reauthRequested.compare_exchange_strong(expected, true)) {
        log("别急！");
        return false;  // 已有请求排队
    }
    // 唤醒可能正在 sleep 的主循环，让请求立即被处理（对齐 Python 独立线程的即时性）
    {
        std::lock_guard<std::mutex> lock(m_sleepMutex);
        m_sleepCv.notify_all();
    }
    return true;
}

bool CoreWorker::sleepInterruptible(std::stop_token st, int seconds) {
    if (st.stop_requested()) return false;
    std::unique_lock<std::mutex> lock(m_sleepMutex);
    // 停止信号或 reauth 请求都会提前唤醒；reauth 唤醒后返回 true 让循环顶部处理请求
    m_sleepCv.wait_for(lock, std::chrono::seconds(seconds), [&st, this] {
        return st.stop_requested() || m_stopping.load() || m_reauthRequested.load();
    });
    return !st.stop_requested();  // false = 被停止打断，调用方应退出
}

bool CoreWorker::ensureCredentials() {
    // 对齐 load_config_from_file：cookie/token 缺失 或 LastUpdate 距今 > 7 天 → 重新获取回写
    bool needFetch = m_cookie.empty() || m_csrfToken.empty();
    if (!needFetch && !m_cfg.lastUpdate.empty()) {
        auto last = AppConfig::parseTimestamp(m_cfg.lastUpdate);
        if (last) {
            const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count();
            if (nowSec - *last > 7LL * 24 * 3600) {
                log("凭证已超 7 天，自动更新");
                needFetch = true;
            }
        } else {
            needFetch = true;  // 时间戳解析失败按需更新（对齐原版）
        }
    }

    if (needFetch) {
        Credentials cred = AuthClient::getCookieAndCsrf(m_cfg.authServer);  // 失败抛出，外层捕获
        m_cookie = cred.yudearCookie;
        m_csrfToken = cred.csrfToken;
        std::string err;
        const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
        if (!AppConfig::saveKeys(m_cfgPath, m_cookie, m_csrfToken,
                                 AppConfig::formatTimestamp(nowSec), err)) {
            log("凭证回写失败: " + err);
        }
    }
    return true;
}

void CoreWorker::doLogin() {
    AuthClient::login(m_cfg.authServer, m_cookie, m_csrfToken, m_cfg.username, m_cfg.password);
}

void CoreWorker::doRelogin() {
    AuthClient::logout(m_cfg.authServer);
    std::this_thread::sleep_for(std::chrono::seconds(10));  // 对齐原版 logout 后 10s
    doLogin();
}

void CoreWorker::run(std::stop_token st) {
    log("自动认证服务启动");
    try {
        ensureCredentials();
        log("核心线程配置导入完成");
    } catch (const std::exception& e) {
        log(std::string("启动失败: ") + e.what());
        emit stopped();
        return;
    }

    mainLoop(st);
    log("核心线程收到停止信号，正在退出");
}

void CoreWorker::mainLoop(std::stop_token st) {
    int flag = 0;
    while (!st.stop_requested()) {
        try {
            // --- 手动重新认证请求（对齐 GUI trigger_reauth 分支） ---
            if (m_reauthRequested.load()) {
                log("已触发重新认证请求");
                AuthClient::reAuth(m_cfg.authServer, m_cookie, m_csrfToken,
                                   m_cfg.username, m_cfg.password);
                m_reauthRequested.store(false);
                emit reauthFinished();
                continue;  // 对齐原版 re_auth 独立线程、主循环不受影响
            }

            // --- 稳定提示（对齐 flag>=10 提示逻辑） ---
            if (flag >= 10) {
                log("长时间网络稳定，继续保持监测中...");
                flag = 0;
            }
            ++flag;

            // --- 网络连通性检测 ---
            bool stable;
            if (m_cfg.checkType == CheckType::Status) {
                stable = AuthClient::checkStatus(m_cfg.authServer);  // 对齐原版 else 分支
            } else {
                stable = NetworkMonitor::check(m_cfg);
            }
            if (stable) {
                if (!sleepInterruptible(st, 60)) return;
                continue;
            }

            // --- 网络不稳定 → 重认证流程 ---
            log("网络不稳定，尝试重新认证...");
            if (!WlanChecker::isConnected(m_cfg.targetSsid)) {
                throw CheckWlanException();
            }
            if (!AuthClient::checkStatus(m_cfg.authServer)) {
                log("认证失效，正在重新登录...");
                doLogin();
            } else {
                log("认证有效，重新认证");
                doRelogin();
            }
            if (!sleepInterruptible(st, 60)) return;

        } catch (const CheckWlanException&) {
            // 对齐原版：死等 WLAN 恢复（每 60s 探测一次，期间响应停止）
            log("未连接到无线局域网，等待连接...");
            while (!st.stop_requested()) {
                if (!sleepInterruptible(st, 60)) return;
                if (WlanChecker::isConnected(m_cfg.targetSsid)) break;
            }
        } catch (const NetworkUnreachableException& e) {
            log(std::string("网络出现不可达错误，30s后重试: ") + e.what());
            if (!sleepInterruptible(st, 30)) return;
        } catch (const LoginException& e) {
            log(std::string("登录失败: 认证服务器不可达，10s后重试: ") + e.what());
            if (!sleepInterruptible(st, 10)) return;
        } catch (const std::exception& e) {
            // 兜底：任何未知异常不逃逸出线程（自愈，取代 guardian）
            log(std::string("出现预料之外的错误: ") + e.what());
            if (!sleepInterruptible(st, 30)) return;
        }
    }
}

}  // namespace usc
