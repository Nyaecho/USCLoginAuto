// SingleInstance.h — 单实例守护
// 首个实例创建本地命名服务端；后续实例连接成功即为"重复"，发激活消息后退出。
// QLocalServer::removeServer 清理崩溃残留（命名管道随进程销毁，双保险）
#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>

namespace usc {

class SingleInstance : public QObject {
    Q_OBJECT
public:
    // 尝试成为唯一实例。构造后用 isPrimary() 判断结果。
    explicit SingleInstance(QString key, QObject* parent = nullptr);

    bool isPrimary() const { return m_primary; }

    // 仅次实例调用：向主实例发送激活消息（触发其 activationRequested）
    void notifyExisting();

signals:
    // 主实例收到其他实例的激活请求（在 GUI 线程触发）
    void activationRequested();

private:
    QString m_key;
    bool m_primary = false;
    QLocalServer m_server;   // 主实例持有
    QLocalSocket m_probe;    // 次实例探测/通知用
};

}  // namespace usc
