// LogWindow.h — 日志子页面（纯渲染层：订阅 Logger 信号渲染历史日志）
// v3：控制按钮（重新认证/暂停检测/退出）移交 StatusPage；本页只留日志查看能力
#pragma once

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace usc {

class LogWindow : public QWidget {
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);

    // 切到本页时立即刷新一次（供 MainWindow 标签切换时调用）
    void refreshNow();

private slots:
    void onRefresh();            // 定时刷新日志内容
    void onToggleAutoRefresh();  // 暂停/继续自动刷新
    void onSaveLog();            // 保存日志为 TXT
    void onClearLog();           // 清空日志（带确认）

private:
    void rebuildUiText();

    QPlainTextEdit* m_text = nullptr;
    QPushButton* m_toggleBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QTimer* m_timer = nullptr;
    bool m_autoRefresh = true;
    bool m_lastContentDirty = true;  // 有新日志未渲染标志（避免每 tick 全量重绘）
};

}  // namespace usc
