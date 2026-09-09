// StatusPage.h — 状态主页（纯渲染层：大图标状态 + 倒计时 + 账号信息 + 状态历史 + 控制）
// 数据来源：MainWindow 接线推送（WorkerState / AccountStatus / 暂停与倒计时轮询）
#pragma once

#include <QWidget>
#include <cstdint>

#include "AuthClient.h"  // AccountStatus

class QLabel;
class QListWidget;
class QPushButton;
class QProgressBar;

namespace usc {

class StatusPage : public QWidget {
    Q_OBJECT
public:
    explicit StatusPage(QWidget* parent = nullptr);

signals:
    // 用户意图信号（GUI 不做业务，转发给接线方）
    void reauthRequested();         // "重新认证"按钮
    void pauseRequested(int minutes);  // "暂停检测"按钮（0 = 无限期）
    void resumeRequested();         // 暂停中点"恢复检测"
    void exitRequested();           // "退出程序"按钮

public:
    // 状态机推送（state 取 WorkerState 枚举值）
    void onStateChanged(int state, const QString& detail);
    // 账号状态推送（检测通过后 CoreWorker 上报）
    void onAccountStatus(const usc::AccountStatus& st);
    // 轮询推送：检测暂停状态 + 距下次检测剩余秒（由 MainWindow pollTick 驱动）
    void setTimers(bool paused, std::int64_t pauseRemainSec, std::int64_t nextCheckRemainSec);

private slots:
    void onPauseClicked();  // 暂停检测（选时长）/ 恢复检测
    void onExitClicked();   // 退出（带确认）

private:
    void rebuildStatusLook();   // 按当前状态刷新大图标/颜色/文案
    void addHistory(const QString& text, const QColor& dot);
    static QString humanBytes(long long bytes);  // 字节 → "1.79 GB" 人性化显示

    // 状态大图标区
    QLabel* m_stateIcon = nullptr;     // 圆点（QLabel + 样式着色）
    QLabel* m_stateTitle = nullptr;    // 大字标题（如"网络正常"）
    QLabel* m_stateSub = nullptr;      // 副行（上次检测/方式/错误详情）
    QLabel* m_uptimeBadge = nullptr;   // 右侧胶囊（连续在线时长）

    // 倒计时条
    QLabel* m_timerLabel = nullptr;
    QProgressBar* m_timerBar = nullptr;

    // 账号信息卡片
    QLabel* m_accName = nullptr;
    QLabel* m_accUsername = nullptr;
    QLabel* m_accIp = nullptr;
    QLabel* m_accMac = nullptr;
    QLabel* m_accOnlineSince = nullptr;
    QLabel* m_accTraffic = nullptr;

    // 最近状态变化
    QListWidget* m_history = nullptr;

    // 控制按钮
    QPushButton* m_pauseBtn = nullptr;

    // 本页缓存状态
    int m_state = 0;               // WorkerState 枚举值
    bool m_paused = false;
    QString m_stateDetail;
    QString m_lastOnlineSince;     // 上次在线的 AddTime（算"连续在线"展示）
};

}  // namespace usc
