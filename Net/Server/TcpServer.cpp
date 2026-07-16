#include "TcpServer.h"

namespace Net
{
// 构造函数：连接newConnection信号到处理槽
TcpServer::TcpServer(QObject* parent)
    : QObject(parent)
{
    connect(&mServer, &QTcpServer::newConnection, this, &TcpServer::OnNewConn);
}

// 开始监听指定端口
bool TcpServer::Listen(quint16 port)
{
    return mServer.listen(QHostAddress::Any, port);
}

// 关闭服务端，释放所有客户端通道
void TcpServer::Close()
{
    mServer.close();
    for (auto c : mClients) c->deleteLater();
    mClients.clear();
}

// 获取所有已连接的客户端通道列表
QList<TcpChannel*> TcpServer::AllClients() const
{
    return mClients;
}

// 新连接回调：接管accept到的socket，创建TcpChannel并通知上层
void TcpServer::OnNewConn()
{
    while (mServer.hasPendingConnections())
    {
        QTcpSocket* sock = mServer.nextPendingConnection();
        // 将已accept的socket传递给TcpChannel，由TcpChannel接管所有权
        TcpChannel* cli = new TcpChannel(sock, this);
        mClients.append(cli);
        emit SignalNewClient(cli);
    }
}
}