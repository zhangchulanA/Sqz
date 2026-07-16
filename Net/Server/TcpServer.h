#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "NetGlobal.h"
#include "TcpChannel.h"
#include <QTcpServer>

namespace Net
{
// TCP服务端，管理多个Tcp客户端通道
class NET_EXPORT TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject* parent = nullptr);
    bool Listen(quint16 port);
    void Close();
    QList<TcpChannel*> AllClients() const;

signals:
    void SignalNewClient(TcpChannel* cli);
private slots:
    void OnNewConn();
private:
    QTcpServer mServer;
    QList<TcpChannel*> mClients;
};
}

#endif
