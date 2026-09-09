// MainWindow.cpp — 主窗口实现
#include "MainWindow.h"

#include <QTabWidget>
#include <QTimer>

#include "LogWindow.h"
#include "StatusPage.h"

namespace usc {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("校园网守护");
    resize(720, 560);

    m_statusPage = new StatusPage(this);
    m_logPage = new LogWindow(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_statusPage, "状态");
    m_tabs->addTab(m_logPage, "日志");
    setCentralWidget(m_tabs);

    // 切到日志页时立即刷新一次内容（页签可能长时间未渲染）
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_tabs->widget(index) == m_logPage) m_logPage->refreshNow();
    });

    // 转发用户意图
    connect(m_statusPage, &StatusPage::reauthRequested, this, &MainWindow::reauthRequested);
    connect(m_statusPage, &StatusPage::pauseRequested, this, &MainWindow::pauseRequested);
    connect(m_statusPage, &StatusPage::resumeRequested, this, &MainWindow::resumeRequested);
    connect(m_statusPage, &StatusPage::exitRequested, this, &MainWindow::exitRequested);

    // 1s 轮询节拍：暂停倒计时/检测倒计时由接线方拉 CoreWorker 真值回推
    auto* poller = new QTimer(this);
    poller->setInterval(1000);
    connect(poller, &QTimer::timeout, this, &MainWindow::pollTick);
    poller->start();
}

void MainWindow::showAndActivate() {
    show();
    raise();
    activateWindow();
    m_tabs->setCurrentWidget(m_statusPage);  // 默认切到状态页（对齐预览图①）
}

void MainWindow::onStateChanged(int state, const QString& detail) {
    m_statusPage->onStateChanged(state, detail);
}

void MainWindow::onAccountStatus(const usc::AccountStatus& st) {
    m_statusPage->onAccountStatus(st);
}

void MainWindow::setPauseAndTimers(bool paused, std::int64_t pauseRemainSec,
                                    std::int64_t nextCheckRemainSec) {
    m_statusPage->setTimers(paused, pauseRemainSec, nextCheckRemainSec);
}

}  // namespace usc
