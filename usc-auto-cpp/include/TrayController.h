// TrayController.h — 系统托盘（纯渲染层：显示日志/退出菜单 + 图标）
#pragma once

#include <QSystemTrayIcon>

class QMenu;

namespace usc {

class TrayController : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayController(QObject* parent = nullptr);

    // icon.png 存在则用之，否则 QPainter 画占位图（对齐原版 create_icon）
    static QIcon loadIcon();

signals:
    void showMainRequested();  // 菜单"打开主界面"或双击图标（默认切到状态页）
    void exitRequested();      // 菜单"退出"
};

}  // namespace usc
