// StatusPage.cpp — 状态主页实现
#include "StatusPage.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "CoreWorker.h"  // NetState / netStateText

namespace usc {

namespace {

// 校园网状态 → 主色（对齐预览图色板：绿=已连接 黄=无互联网 红=未连接 灰=未知）
QColor stateColor(int s) {
    switch (static_cast<NetState>(s)) {
        case NetState::Connected:           return {0x2e, 0xcc, 0x71};  // 绿
        case NetState::ConnectedNoInternet: return {0xf3, 0x9c, 0x12};  // 黄
        case NetState::Disconnected:        return {0xe7, 0x4c, 0x3c};  // 红
        case NetState::NotOnCampus:         return {0xe7, 0x4c, 0x3c};  // 红
        case NetState::Unknown:             return {0x95, 0xa5, 0xa6};  // 灰
    }
    return Qt::gray;
}

// 校园网状态 → 圆点内符号
QString stateGlyph(int s) {
    switch (static_cast<NetState>(s)) {
        case NetState::Connected:           return "✓";
        case NetState::ConnectedNoInternet: return "!";
        case NetState::Disconnected:        return "✕";
        case NetState::NotOnCampus:         return "✕";
        case NetState::Unknown:             return "…";
    }
    return "?";
}

}  // namespace

StatusPage::StatusPage(QWidget* parent) : QWidget(parent) {
    // === 顶部：状态大图标区 ===
    m_stateIcon = new QLabel("…", this);
    m_stateIcon->setFixedSize(60, 60);
    m_stateIcon->setAlignment(Qt::AlignCenter);
    QFont iconFont = m_stateIcon->font();
    iconFont.setPointSize(22);
    iconFont.setBold(true);
    m_stateIcon->setFont(iconFont);

    m_stateTitle = new QLabel("启动中", this);
    QFont titleFont = m_stateTitle->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    m_stateTitle->setFont(titleFont);

    m_stateSub = new QLabel("正在初始化...", this);
    m_stateSub->setStyleSheet("color:#8a8f98;");

    m_uptimeBadge = new QLabel(this);
    m_uptimeBadge->setStyleSheet(
        "background-color:#e9f9f1; color:#1e8e4e; border:1px solid #bfe9d2;"
        "border-radius:14px; padding:4px 14px;");
    m_uptimeBadge->hide();

    auto* headLayout = new QHBoxLayout();
    headLayout->setContentsMargins(12, 12, 12, 4);
    auto* headText = new QVBoxLayout();
    headText->setSpacing(2);
    headText->addWidget(m_stateTitle);
    headText->addWidget(m_stateSub);
    headLayout->addWidget(m_stateIcon);
    headLayout->addLayout(headText);
    headLayout->addStretch();
    headLayout->addWidget(m_uptimeBadge);

    // === 倒计时条 ===
    auto* timerFrame = new QWidget(this);
    timerFrame->setStyleSheet(
        "QWidget { background-color:#eef4fd; border-radius:7px; }");
    m_timerLabel = new QLabel("距下次检测  --", timerFrame);
    m_timerBar = new QProgressBar(timerFrame);
    m_timerBar->setFixedWidth(216);
    m_timerBar->setFixedHeight(10);
    m_timerBar->setRange(0, 60);
    m_timerBar->setTextVisible(false);
    m_timerBar->setStyleSheet(
        "QProgressBar { background-color:#d8e3f6; border-radius:5px; }"
        "QProgressBar::chunk { background-color:#4a90e2; border-radius:5px; }");
    auto* timerLayout = new QHBoxLayout(timerFrame);
    timerLayout->setContentsMargins(16, 8, 16, 8);
    timerLayout->addWidget(m_timerLabel);
    timerLayout->addStretch();
    timerLayout->addWidget(m_timerBar);

    // === 账号信息卡片 ===
    auto* card = new QWidget(this);
    card->setStyleSheet(
        "QWidget { background-color:#fbfcfd; border:1px solid #e3e6eb; border-radius:8px; }");
    auto makeRow = [](QLabel*& value, const char* label) {
        auto* l = new QLabel(label);
        l->setStyleSheet("color:#98a0ab; background:transparent; border:none;");
        value = new QLabel("-");
        value->setStyleSheet("background:transparent; border:none;");
        return std::pair{l, value};
    };
    auto* cardTitle = new QLabel("认证信息");
    QFont cardFont = cardTitle->font();
    cardFont.setBold(true);
    cardTitle->setFont(cardFont);
    cardTitle->setStyleSheet("font-size:14px; background:transparent; border:none;");
    auto* cardGrid = new QGridLayout();
    cardGrid->setContentsMargins(0, 0, 0, 0);
    cardGrid->setHorizontalSpacing(24);
    cardGrid->setVerticalSpacing(10);
    auto [n1, v1] = makeRow(m_accName, "姓名");
    auto [n2, v2] = makeRow(m_accUsername, "用户名");
    auto [n3, v3] = makeRow(m_accIp, "IPv4");
    cardGrid->addWidget(n1, 0, 0); cardGrid->addWidget(v1, 0, 1);
    cardGrid->addWidget(n2, 0, 2); cardGrid->addWidget(v2, 0, 3);
    cardGrid->addWidget(n3, 0, 4); cardGrid->addWidget(v3, 0, 5);
    auto [m1, w1] = makeRow(m_accMac, "MAC");
    auto [m2, w2] = makeRow(m_accOnlineSince, "上线时间");
    auto [m3, w3] = makeRow(m_accTraffic, "流量");
    cardGrid->addWidget(m1, 1, 0); cardGrid->addWidget(w1, 1, 1);
    cardGrid->addWidget(m2, 1, 2); cardGrid->addWidget(w2, 1, 3);
    cardGrid->addWidget(m3, 1, 4); cardGrid->addWidget(w3, 1, 5);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);
    cardLayout->addWidget(cardTitle);
    cardLayout->addLayout(cardGrid);

    // === 最近状态变化 ===
    auto* histHeader = new QHBoxLayout();
    auto* histTitle = new QLabel("最近状态变化");
    QFont histFont = histTitle->font();
    histFont.setBold(true);
    histTitle->setFont(histFont);
    auto* histHint = new QLabel("保留最近 50 条");
    histHint->setStyleSheet("color:#a6adb8;");
    histHeader->addWidget(histTitle);
    histHeader->addStretch();
    histHeader->addWidget(histHint);

    m_history = new QListWidget(this);
    m_history->setAlternatingRowColors(false);
    m_history->setUniformItemSizes(true);
    m_history->setFocusPolicy(Qt::NoFocus);
    m_history->setStyleSheet(
        "QListWidget { background:transparent; border:none; }"
        "QListWidget::item { padding:1px 0; }");

    // === 控制按钮 ===
    auto* btnFrame = new QWidget(this);
    auto* btns = new QHBoxLayout(btnFrame);
    btns->setContentsMargins(0, 0, 0, 0);
    auto* reauthBtn = new QPushButton("重新认证", btnFrame);
    m_pauseBtn = new QPushButton("暂停检测", btnFrame);
    auto* exitBtn = new QPushButton("退出程序", btnFrame);
    exitBtn->setStyleSheet("background-color:#ff6b6b; color:white;");
    btns->addWidget(reauthBtn);
    btns->addWidget(m_pauseBtn);
    btns->addStretch();
    btns->addWidget(exitBtn);

    connect(reauthBtn, &QPushButton::clicked, this, [this]() { emit reauthRequested(); });
    connect(m_pauseBtn, &QPushButton::clicked, this, &StatusPage::onPauseClicked);
    connect(exitBtn, &QPushButton::clicked, this, &StatusPage::onExitClicked);

    // === 总装 ===
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 12, 20, 16);
    layout->addLayout(headLayout);
    layout->addWidget(timerFrame);
    layout->addWidget(card);
    layout->addLayout(histHeader);
    layout->addWidget(m_history, 1);
    layout->addWidget(btnFrame);

    addHistory("程序启动，开始监测", QColor(0x8a, 0x91, 0x9c));
    rebuildStatusLook();
}

void StatusPage::onStateChanged(int state, const QString& detail) {
    // 仅当校园网状态值变化时记入历史（过程刷新/重复上报不记）
    const bool changed = state != m_state;
    if (changed && m_state == static_cast<int>(NetState::Unknown)) {
        addHistory(QString("首次检测：%1").arg(netStateText(static_cast<NetState>(state))),
                   stateColor(state));
    } else if (changed) {
        addHistory(QString("状态变化：%1 → %2")
                       .arg(netStateText(static_cast<NetState>(m_state)))
                       .arg(netStateText(static_cast<NetState>(state))),
                   stateColor(state));
    }
    m_state = state;
    m_stateDetail = detail;
    rebuildStatusLook();
}

void StatusPage::onAccountStatus(const usc::AccountStatus& st) {
    if (!st.valid && !st.timedOut) {
        m_accName->setText(QString("无法获取（%1）").arg(QString::fromStdString(st.error)));
        m_accUsername->setText("-");
        m_accIp->setText("-");
        m_accMac->setText("-");
        m_accOnlineSince->setText("-");
        m_accTraffic->setText("-");
        return;
    }
    if (!st.online) {
        m_accName->setText("当前不在线");
        m_accUsername->setText("-");
        m_accIp->setText("-");
        m_accMac->setText("-");
        m_accOnlineSince->setText("-");
        m_accTraffic->setText("-");
        m_uptimeBadge->hide();
        return;
    }
    m_lastOnlineSince = QString::fromStdString(st.onlineSince);
    m_accName->setText(QString::fromStdString(st.name));
    m_accUsername->setText(QString::fromStdString(st.username));
    m_accIp->setText(QString::fromStdString(st.ipv4));
    m_accMac->setText(QString::fromStdString(st.mac));
    // AddTime 形如 "2026-09-09T14:13:56+08:00"，展示为 "09-09 14:13:56"
    QString since = m_lastOnlineSince;
    const auto t = QDateTime::fromString(since, Qt::ISODateWithMs);
    if (t.isValid()) since = t.toString("MM-dd hh:mm:ss");
    m_accOnlineSince->setText(since);
    m_accTraffic->setText(QString("↓ %1　↑ %2")
                              .arg(humanBytes(st.bytesDown), humanBytes(st.bytesUp)));
    // 连续在线胶囊：用 AddTime 与当前差值
    if (t.isValid()) {
        const auto secs = t.secsTo(QDateTime::currentDateTime());
        if (secs >= 0) {
            const auto h = secs / 3600, m = (secs % 3600) / 60;
            m_uptimeBadge->setText(QString("连续在线 %1h %2m").arg(h).arg(m, 2, 10, QChar('0')));
            m_uptimeBadge->show();
        }
    }
}

void StatusPage::setTimers(bool paused, std::int64_t pauseRemainSec,
                           std::int64_t nextCheckRemainSec) {
    m_paused = paused;
    if (paused) {
        if (pauseRemainSec < 0) {
            m_timerLabel->setText("检测已暂停（直到手动恢复）");
            m_pauseBtn->setText("恢复检测（无限期）");
        } else {
            const int mm = static_cast<int>(pauseRemainSec / 60);
            const int ss = static_cast<int>(pauseRemainSec % 60);
            m_timerLabel->setText(QString("检测已暂停　剩 %1:%2")
                                      .arg(mm, 2, 10, QChar('0'))
                                      .arg(ss, 2, 10, QChar('0')));
            m_pauseBtn->setText(QString("恢复检测（剩 %1:%2）")
                                    .arg(mm, 2, 10, QChar('0'))
                                    .arg(ss, 2, 10, QChar('0')));
        }
        m_timerBar->setRange(0, 1);
        m_timerBar->setValue(0);
        return;
    }
    m_pauseBtn->setText("暂停检测");
    if (nextCheckRemainSec <= 0) {
        m_timerLabel->setText("正在检测...");
        m_timerBar->setRange(0, 1);
        m_timerBar->setValue(1);
        return;
    }
    m_timerLabel->setText(QString("距下次检测  %1 秒").arg(nextCheckRemainSec));
    m_timerBar->setRange(0, 60);
    m_timerBar->setValue(static_cast<int>(std::min<std::int64_t>(nextCheckRemainSec, 60)));
}

void StatusPage::onPauseClicked() {
    if (m_paused) {
        emit resumeRequested();
        return;
    }
    // 时长选择：预设 + 自定义 + 无限期（沿用原 LogWindow 交互）
    const QStringList items{"30 分钟", "60 分钟", "120 分钟", "自定义...", "直到手动恢复"};
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, "暂停检测",
                                                 "选择暂停时长（期间不检测网络状态）：",
                                                 items, 0, false, &ok);
    if (!ok) return;  // 取消

    int minutes = 0;
    if (choice == "30 分钟") minutes = 30;
    else if (choice == "60 分钟") minutes = 60;
    else if (choice == "120 分钟") minutes = 120;
    else if (choice == "直到手动恢复") minutes = 0;
    else {
        bool numOk = false;
        const int custom = QInputDialog::getInt(this, "自定义时长", "暂停分钟数：",
                                                60, 1, 24 * 60, 1, &numOk);
        if (!numOk) return;
        minutes = custom;
    }
    emit pauseRequested(minutes);
}

void StatusPage::onExitClicked() {
    if (QMessageBox::question(this, "确认退出", "确定要退出校园网守护程序吗？") ==
        QMessageBox::Yes) {
        emit exitRequested();
    }
}

void StatusPage::rebuildStatusLook() {
    const QColor c = stateColor(m_state);
    m_stateIcon->setStyleSheet(QString(
        "background-color:%1; color:white; border-radius:30px;")
        .arg(c.name()));
    m_stateIcon->setText(stateGlyph(m_state));
    m_stateTitle->setText(netStateText(static_cast<NetState>(m_state)));
    m_stateTitle->setStyleSheet(QString("color:%1;").arg(c.name()));
    if (!m_stateDetail.isEmpty()) {
        m_stateSub->setText(m_stateDetail);
    } else {
        m_stateSub->setText(m_lastOnlineSince.isEmpty()
                                ? "等待首次检测..."
                                : QString("上次上线 %1").arg(m_lastOnlineSince));
    }
    if (static_cast<NetState>(m_state) != NetState::Connected) {
        m_uptimeBadge->hide();
    }
}

void StatusPage::addHistory(const QString& text, const QColor& dot) {
    auto* item = new QListWidgetItem(m_history);
    const QString ts = QDateTime::currentDateTime().toString("[hh:mm:ss]");
    // 圆点用彩色字符近似（QListWidget 自绘圆点需 delegate，字符方案足够轻量）
    item->setText(QString("%1  %2  %3").arg(ts).arg("●").arg(text));
    item->setForeground(QColor(0x33, 0x38, 0x3f));
    auto* label = new QLabel(QString("%1  <span style=\"color:%2\">●</span>  %3")
                                 .arg(ts, dot.name(), text.toHtmlEscaped()));
    label->setStyleSheet("color:#33383f; background:transparent;");
    m_history->setItemWidget(item, label);
    while (m_history->count() > 50) delete m_history->takeItem(0);
    m_history->scrollToBottom();
}

QString StatusPage::humanBytes(long long bytes) {
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) return QString::number(gb, 'f', 2) + " GB";
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1.0) return QString::number(mb, 'f', 1) + " MB";
    return QString::number(bytes / 1024.0, 'f', 1) + " KB";
}

}  // namespace usc
