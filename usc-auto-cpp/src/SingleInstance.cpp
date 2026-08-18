// SingleInstance.cpp — 单实例守护实现
#include "SingleInstance.h"

namespace usc {

SingleInstance::SingleInstance(QString key, QObject* parent)
    : QObject(parent), m_key(std::move(key)) {
    // 清理上次崩溃可能残留的服务端记录（正常退出时内核已回收）
    QLocalServer::removeServer(m_key);

    // 先探测：已有实例在运行则连接成功
    m_probe.connectToServer(m_key);
    if (m_probe.waitForConnected(200)) {
        m_primary = false;  // 我是重复实例
        return;
    }

    // 无人占用 → 创建服务端，成为主实例
    if (m_server.listen(m_key)) {
        m_primary = true;
        connect(&m_server, &QLocalServer::newConnection, this, [this]() {
            QLocalSocket* conn = m_server.nextPendingConnection();
            if (conn == nullptr) return;
            connect(conn, &QLocalSocket::disconnected, conn, &QObject::deleteLater);
            if (conn->waitForReadyRead(200) && conn->readAll() == "show") {
                emit activationRequested();
            }
            conn->disconnectFromServer();
        });
    } else {
        // listen 失败（极端竞态）：按次实例处理，静默退出
        m_primary = false;
    }
}

void SingleInstance::notifyExisting() {
    if (m_primary || m_probe.state() != QLocalSocket::ConnectedState) return;
    m_probe.write("show");
    m_probe.flush();
    m_probe.disconnectFromServer();
}

}  // namespace usc
