// MainWindow.h — 主窗口（QTabWidget：「状态」主页 + 「日志」子页面）
// 纯渲染层聚合：转发子页用户意图、1s 轮询回推暂停/倒计时状态、统一显示入口
#pragma once

#include <QMainWindow>
#include <cstdint>

#include "AuthClient.h"  // AccountStatus

class QTabWidget;

namespace usc {

class StatusPage;
class LogWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // 显示并置前（关闭=隐藏，不销毁；托盘双击/单实例激活时调用，默认切到状态页）
    void showAndActivate();

signals:
    // 用户意图（聚合自 StatusPage，供接线方连到 CoreWorker）
    void reauthRequested();
    void pauseRequested(int minutes);  // 0 = 无限期
    void resumeRequested();
    void exitRequested();
    // 1s 轮询节拍（供接线方回推 setPauseAndTimers：拉取 CoreWorker 真值刷新 UI）
    void pollTick();

public slots:
    // 状态推送（CoreWorker stateChanged，跨线程队列投递）
    void onStateChanged(int state, const QString& detail);
    // 账号状态推送（CoreWorker accountStatusUpdated）
    void onAccountStatus(const usc::AccountStatus& st);
    // 轮询回推（pollTick 的应答：暂停态 + 距下次检测秒数）
    void setPauseAndTimers(bool paused, std::int64_t pauseRemainSec,
                           std::int64_t nextCheckRemainSec);

private:
    QTabWidget* m_tabs = nullptr;
    StatusPage* m_statusPage = nullptr;
    LogWindow* m_logPage = nullptr;
};

}  // namespace usc
