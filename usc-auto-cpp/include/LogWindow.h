// LogWindow.h — 日志窗口（纯渲染层：订阅 Logger 信号 + 转发用户意图）
// 对齐 Python LogWindow：自动刷新可暂停、重新认证、保存 TXT、清空、退出
#pragma once

#include <QWidget>
#include <cstdint>

class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace usc {

class LogWindow : public QWidget {
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);

    // 显示并置前（关闭=隐藏，不销毁，对齐原版 WM_DELETE_WINDOW → withdraw）
    void showAndActivate();

signals:
    // 用户意图信号（GUI 不做业务，转发给接线方）
    void reauthRequested();   // "重新认证"按钮
    void pauseRequested(int minutes);  // "暂停检测"按钮（0 = 无限期）
    void resumeRequested();   // 暂停中点"恢复检测"
    void exitRequested();     // "退出程序"按钮

public:
    // 同步暂停状态显示（由接线方在状态变化时调用；线程安全经由信号槽）
    void setPauseState(bool paused, std::int64_t remainSec);

private slots:
    void onRefresh();            // 定时刷新日志内容
    void onToggleAutoRefresh();  // 暂停/继续自动刷新
    void onPauseClicked();       // 暂停检测（选时长）/ 恢复检测
    void onSaveLog();            // 保存日志为 TXT
    void onClearLog();           // 清空日志（带确认）
    void onExitClicked();        // 退出（带确认）

signals:
    void refreshTick();          // 每次刷新发出（供同步暂停倒计时）

private:
    void rebuildUiText();

    QPlainTextEdit* m_text = nullptr;
    QPushButton* m_toggleBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QTimer* m_timer = nullptr;
    bool m_autoRefresh = true;
    bool m_lastContentDirty = true;  // 有新日志未渲染标志（避免每 tick 全量重绘）
    bool m_paused = false;           // 检测暂停状态（仅显示用，真值在 CoreWorker）
};

}  // namespace usc
