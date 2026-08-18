// TrayController.cpp — 托盘实现
#include "TrayController.h"

#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QPainter>
#include <QPen>

namespace usc {

TrayController::TrayController(QObject* parent) : QSystemTrayIcon(parent) {
    setIcon(loadIcon());
    setToolTip("校园网守护程序");

    auto* menu = new QMenu();
    // default=true：双击图标触发（对齐原版 pystray default=True）
    auto* showAction = menu->addAction("显示日志");
    menu->setDefaultAction(showAction);  // 粗体默认项
    menu->addSeparator();
    menu->addAction("退出", this, &TrayController::exitRequested);

    setContextMenu(menu);
    connect(showAction, &QAction::triggered, this, &TrayController::showLogRequested);
    // 激活（双击/单击按系统约定）也显示日志
    connect(this, &QSystemTrayIcon::activated, this, [this](ActivationReason reason) {
        if (reason == DoubleClick || reason == Trigger) {
            emit showLogRequested();
        }
    });
    show();
}

QIcon TrayController::loadIcon() {
    // 外部 icon.png 优先（对齐原版）
    if (QFile::exists("icon.png")) {
        return QIcon("icon.png");
    }
    // 占位图：粉色圆底 + 白色三圆（对齐原版 PIL 绘制样式）
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 182, 193));
    p.drawEllipse(8, 8, 48, 48);
    p.setBrush(Qt::white);
    p.drawEllipse(24, 20, 16, 16);
    p.drawEllipse(18, 34, 12, 12);
    p.drawEllipse(34, 34, 12, 12);
    p.end();
    return QIcon(pm);
}

}  // namespace usc
