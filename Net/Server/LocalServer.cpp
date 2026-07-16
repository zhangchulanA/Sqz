#include "LocalServer.h"

namespace Net
{
// 构造函数：连接newConnection信号到处理槽
LocalServer::LocalServer(QObject* parent)
    : QObject(parent)
{
    connect(&mServer, &QLocalServer::newConnection, this, &LocalServer::OnNewConn);
}

// 开始监听指定名称的本地服务
bool LocalServer::Listen(const QString& name)
{
    QLocalServer::removeServer(name);
    return mServer.listen(name);
}

// 关闭服务端，释放所有客户端通道
void LocalServer::Close()
{
    mServer.close();
    for (auto c : mClients) c->deleteLater();
    mClients.clear();
}

// 获取所有已连接的客户端通道列表
QList<LocalChannel*> LocalServer::AllClients() const
{
    return mClients;
}

// 新连接回调：接管accept到的socket，创建LocalChannel并通知上层
void LocalServer::OnNewConn()
{
    while (mServer.hasPendingConnections())
    {
        QLocalSocket* sock = mServer.nextPendingConnection();
        // 将已accept的socket传递给LocalChannel，由LocalChannel接管所有权
        LocalChannel* cli = new LocalChannel(sock, this);
        mClients.append(cli);
        emit SignalNewClient(cli);
    }
}
}