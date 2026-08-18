// LogWindow.cpp — 日志窗口实现
#include "LogWindow.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "Logger.h"

namespace usc {

LogWindow::LogWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("校园网守护日志");
    resize(700, 500);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    m_text->setFont(mono);

    auto* btnFrame = new QWidget(this);
    auto* btns = new QHBoxLayout(btnFrame);
    btns->setContentsMargins(0, 0, 0, 0);

    m_toggleBtn = new QPushButton("暂停自动刷新", btnFrame);
    auto* reauthBtn = new QPushButton("重新认证", btnFrame);
    auto* pauseBtn = new QPushButton("暂停检测", btnFrame);
    auto* saveBtn = new QPushButton("保存日志为 TXT", btnFrame);
    auto* clearBtn = new QPushButton("清空日志", btnFrame);
    auto* exitBtn = new QPushButton("退出程序", btnFrame);
    m_pauseBtn = pauseBtn;
    exitBtn->setStyleSheet("background-color:#ff6b6b; color:white;");

    btns->addWidget(m_toggleBtn);
    btns->addWidget(reauthBtn);
    btns->addWidget(pauseBtn);
    btns->addWidget(saveBtn);
    btns->addWidget(clearBtn);
    btns->addStretch();
    btns->addWidget(exitBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_text);
    layout->addWidget(btnFrame);

    // 50ms 自动刷新（对齐原版 after(50, tick)）
    m_timer = new QTimer(this);
    m_timer->setInterval(50);
    connect(m_timer, &QTimer::timeout, this, &LogWindow::onRefresh);
    m_timer->start();

    connect(m_toggleBtn, &QPushButton::clicked, this, &LogWindow::onToggleAutoRefresh);
    connect(reauthBtn, &QPushButton::clicked, this, [this]() { emit reauthRequested(); });
    connect(pauseBtn, &QPushButton::clicked, this, &LogWindow::onPauseClicked);
    connect(saveBtn, &QPushButton::clicked, this, &LogWindow::onSaveLog);
    connect(clearBtn, &QPushButton::clicked, this, &LogWindow::onClearLog);
    connect(exitBtn, &QPushButton::clicked, this, &LogWindow::onExitClicked);

    // 新日志到达 → 标脏（信号从后台线程来，Qt 自动队列到 GUI 线程）
    connect(&Logger::instance(), &Logger::logAppended, this, [this]() { m_lastContentDirty = true; },
            Qt::QueuedConnection);

    rebuildUiText();
    onRefresh();
}

void LogWindow::showAndActivate() {
    show();
    raise();
    activateWindow();
    onRefresh();
}

void LogWindow::rebuildUiText() {
    m_toggleBtn->setText(m_autoRefresh ? "暂停自动刷新" : "继续自动刷新");
}

void LogWindow::onRefresh() {
    if (!m_autoRefresh || !m_lastContentDirty) return;
    m_lastContentDirty = false;
    m_text->setPlainText(Logger::instance().content());
    m_text->verticalScrollBar()->setValue(m_text->verticalScrollBar()->maximum());
    emit refreshTick();  // 供接线方同步暂停倒计时显示
}

void LogWindow::onToggleAutoRefresh() {
    m_autoRefresh = !m_autoRefresh;
    rebuildUiText();
    if (m_autoRefresh) {
        m_lastContentDirty = true;
        onRefresh();
    }
}

void LogWindow::onSaveLog() {
    const QString defName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";
    const QString path = QFileDialog::getSaveFileName(this, "保存日志", defName, "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(Logger::instance().content().toUtf8());
        QMessageBox::information(this, "成功", "日志已保存至:\n" + path);
    } else {
        QMessageBox::critical(this, "错误", "保存失败:\n" + f.errorString());
    }
}

void LogWindow::onClearLog() {
    if (QMessageBox::question(this, "确认清空", "确定要清空当前日志吗？") == QMessageBox::Yes) {
        Logger::instance().clear();
        m_text->clear();
        m_lastContentDirty = false;
    }
}

void LogWindow::onExitClicked() {
    if (QMessageBox::question(this, "确认退出", "确定要退出校园网守护程序吗？") == QMessageBox::Yes) {
        emit exitRequested();
    }
}

void LogWindow::onPauseClicked() {
    if (m_paused) {
        emit resumeRequested();
        return;
    }
    // 时长选择：预设 + 自定义 + 无限期
    const QStringList items{"30 分钟", "60 分钟", "120 分钟", "自定义...", "直到手动恢复"};
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, "暂停检测",
                                                 "选择暂停时长（期间不检测网络状态）：", items, 0, false, &ok);
    if (!ok) return;  // 取消

    int minutes = 0;
    if (choice == "30 分钟") minutes = 30;
    else if (choice == "60 分钟") minutes = 60;
    else if (choice == "120 分钟") minutes = 120;
    else if (choice == "直到手动恢复") minutes = 0;
    else {
        bool numOk = false;
        const int custom = QInputDialog::getInt(this, "自定义时长", "暂停分钟数：", 60, 1, 24 * 60, 1, &numOk);
        if (!numOk) return;
        minutes = custom;
    }
    emit pauseRequested(minutes);
}

void LogWindow::setPauseState(bool paused, std::int64_t remainSec) {
    m_paused = paused;
    if (!paused) {
        m_pauseBtn->setText("暂停检测");
        return;
    }
    if (remainSec < 0) {
        m_pauseBtn->setText("恢复检测（无限期）");
    } else {
        const int mm = static_cast<int>(remainSec / 60);
        const int ss = static_cast<int>(remainSec % 60);
        m_pauseBtn->setText(QString("恢复检测（剩 %1:%2）")
                                .arg(mm, 2, 10, QChar('0'))
                                .arg(ss, 2, 10, QChar('0')));
    }
}

}  // namespace usc
