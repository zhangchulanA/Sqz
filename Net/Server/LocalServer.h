#ifndef LOCALSERVER_H
#define LOCALSERVER_H

#include "NetGlobal.h"
#include "LocalChannel.h"
#include <QLocalServer>

namespace Net
{
// IPC本地服务端，管理本机进程客户端
class NET_EXPORT LocalServer : public QObject
{
    Q_OBJECT
public:
    explicit LocalServer(QObject* parent = nullptr);
    bool Listen(const QString& name);
    void Close();
    QList<LocalChannel*> AllClients() const;

signals:
    void SignalNewClient(LocalChannel* cli);
private slots:
    void OnNewConn();
private:
    QLocalServer mServer;
    QList<LocalChannel*> mClients;
};
}

#endif
