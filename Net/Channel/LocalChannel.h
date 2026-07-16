#ifndef LOCALCHANNEL_H
#define LOCALCHANNEL_H

#include "AbstractChannel.h"
#include <QLocalSocket>

namespace Net
{
// 本机进程IPC通道，支持主动连接与服务端接管两种模式
class NET_EXPORT LocalChannel : public AbstractChannel
{
    Q_OBJECT
public:
    // 主动连接模式：内部创建新socket
    explicit LocalChannel(QObject* parent = nullptr);
    // 服务端接管模式：接管已accept的socket，LocalChannel接管所有权
    explicit LocalChannel(QLocalSocket* existingSocket, QObject* parent = nullptr);

    bool ConnectTo(const QString& addr, quint16 port) override;
    void CloseChannel() override;
    qint64 SendRawFrame(const QByteArray& frame) override;
    bool IsChannelConnected() const override;

private slots:
    void OnReadyRead();
    void OnSocketError(QLocalSocket::LocalSocketError socketError);

private:
    // 初始化信号槽连接（两个构造函数共用）
    void InitConnections();

    QLocalSocket* mSocket = nullptr;
};
}

#endif
