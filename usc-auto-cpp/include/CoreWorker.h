// CoreWorker.h — 唯一后台工作线程（v2 分层：全部业务在此，GUI 纯渲染）
// 取代 Python core_module.core_task + guardian 的自捕获自愈部分
#pragma once

#include <QObject>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "AppConfig.h"
#include "AuthClient.h"  // AccountStatus（状态信号载荷）

namespace usc {

// 校园网连接状态（用户视角四态；GUI 展示用，业务时序不变，仅埋点上报）
// 检测中/重认证中/暂停等过程态不单独成态，也不进「状态变化」历史
enum class NetState {
    Unknown = 0,             // 尚未获得首次检测结果
    Connected = 1,           // 已连接（互联网可达）
    ConnectedNoInternet = 2, // 已连接但无互联网（在校园网但检测不过/认证失效，重认证中）
    Disconnected = 3,        // 未连接（网络不可达）
    NotOnCampus = 4,         // 未连接到校园网（WLAN 不在目标 SSID）
};

// 状态对应的 UI 文案
const char* netStateText(NetState s);

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

    // === 检测暂停（免打扰窗口）===
    // 请求暂停 minutes 分钟（0 = 无限期，直到手动恢复）；覆盖旧暂停；线程安全
    void requestPause(int minutes);
    // 提前恢复检测
    void resume();
    // 当前是否处于暂停窗口
    bool isPaused() const;
    // 暂停剩余秒数（未暂停返回 0；无限期返回 -1）
    std::int64_t pauseRemainSec() const;

    // 距下次检测剩余秒数（0 = 检测中/不适用，如暂停、WLAN 等待）
    std::int64_t nextCheckRemainSec() const;

signals:
    // 工作线程正常退出
    void stopped();
    // 一次（手动触发的）重新认证完成
    void reauthFinished();
    // 校园网状态上报（int 取 NetState，避免跨线程 metatype 注册；跨线程发射自动队列投递）
    // 每次埋点均发射（副标题 detail 可能刷新）；「只在状态值变化时记历史」由 GUI 侧判断
    void netStateChanged(int newState, const QString& detail);
    // 账号状态详情（检测通过后上报，供状态页展示）
    void accountStatusUpdated(const usc::AccountStatus& status);

private:
    void run(std::stop_token st);        // 线程主函数（jthread 用）
    void mainLoop(std::stop_token st);   // 对齐 core_task 的主循环
    bool ensureCredentials();            // cookie/token 缺失或超7天 → 获取并回写
    bool sleepInterruptible(std::stop_token st, int seconds);  // false=被停止打断
    void setNetState(NetState s, const QString& detail = {});  // 埋点：上报校园网状态
    void publishAccountStatus();        // 查询账号状态并 emit（失败仅置无效态，不影响业务）

    // 认证动作（保证同一线程内串行执行，天然免锁）
    void doLogin();
    void doRelogin();  // logout → 10s → login（对齐 core_task"认证有效，重新认证"分支）

    AppConfig m_cfg;          // 启动时校验通过的配置快照
    std::string m_cfgPath;    // 回写 cookie/token 用
    std::string m_cookie;     // 运行期凭证（仅工作线程读写）
    std::string m_csrfToken;

    std::jthread m_thread;
    std::atomic<bool> m_reauthRequested{false};

    // 检测暂停：Unix 秒截止时间；0 = 未暂停；-1 = 无限期暂停
    std::atomic<std::int64_t> m_pauseUntil{0};

    // 距下次检测的 Unix 秒截止时间；0 = 检测中/不适用（供 GUI 倒计时）
    std::atomic<std::int64_t> m_nextCheckAt{0};

    // 当前校园网状态（GUI 展示用）
    std::atomic<NetState> m_netState{NetState::Unknown};

    // 可中断 sleep 支持
    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCv;
    std::atomic<bool> m_stopping{false};
};

}  // namespace usc
