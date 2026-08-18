// CoreWorker.h — 唯一后台工作线程（v2 分层：全部业务在此，GUI 纯渲染）
// 取代 Python core_module.core_task + guardian 的自捕获自愈部分
#pragma once

#include <QObject>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "AppConfig.h"

namespace usc {

class CoreWorker : public QObject {
    Q_OBJECT
public:
    explicit CoreWorker(AppConfig cfg, std::string configPath, QObject* parent = nullptr);
    ~CoreWorker() override;

    // 启动工作线程（幂等）
    void start();

    // 请求停止：唤醒 sleep 并等待线程退出（幂等，供退出流程调用）
    void requestStop();

    // 请求重新认证（GUI 按钮触发；线程安全）
    // 返回 false = 已有一次在执行中（对齐 Python"别急!"）
    bool requestReauth();

signals:
    // 工作线程正常退出
    void stopped();
    // 一次（手动触发的）重新认证完成
    void reauthFinished();

private:
    void run(std::stop_token st);        // 线程主函数（jthread 用）
    void mainLoop(std::stop_token st);   // 对齐 core_task 的主循环
    bool ensureCredentials();            // cookie/token 缺失或超7天 → 获取并回写
    bool sleepInterruptible(std::stop_token st, int seconds);  // false=被停止打断

    // 认证动作（保证同一线程内串行执行，天然免锁）
    void doLogin();
    void doRelogin();  // logout → 10s → login（对齐 core_task"认证有效，重新认证"分支）

    AppConfig m_cfg;          // 启动时校验通过的配置快照
    std::string m_cfgPath;    // 回写 cookie/token 用
    std::string m_cookie;     // 运行期凭证（仅工作线程读写）
    std::string m_csrfToken;

    std::jthread m_thread;
    std::atomic<bool> m_reauthRequested{false};

    // 可中断 sleep 支持
    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCv;
    std::atomic<bool> m_stopping{false};
};

}  // namespace usc
