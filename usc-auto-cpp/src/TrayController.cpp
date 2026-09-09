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
    auto* showAction = menu->addAction("打开主界面");
    menu->setDefaultAction(showAction);  // 粗体默认项
    menu->addSeparator();
    menu->addAction("退出", this, &TrayController::exitRequested);

    setContextMenu(menu);
    connect(showAction, &QAction::triggered, this, &TrayController::showMainRequested);
    // 激活（双击/单击按系统约定）也打开主界面
    connect(this, &QSystemTrayIcon::activated, this, [this](ActivationReason reason) {
        if (reason == DoubleClick || reason == Trigger) {
            emit showMainRequested();
        }
    });
    show();
}

QIcon TrayController::loadIcon() {
    // 优先用 exe 内嵌资源图标（windowIcon 在 main 里从资源加载），
    // 与应用程序图标天然一致；无资源时回退 icon.png / 占位图
    if (!QApplication::windowIcon().isNull()) {
        return QApplication::windowIcon();
    }
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
