#ifndef TCPCHANNEL_H
#define TCPCHANNEL_H

#include "AbstractChannel.h"
#include <QTcpSocket>

namespace Net
{
// 局域网TCP点对点通道，支持主动连接与服务端接管两种模式
class NET_EXPORT TcpChannel : public AbstractChannel
{
    Q_OBJECT
public:
    // 主动连接模式：内部创建新socket
    explicit TcpChannel(QObject* parent = nullptr);
    // 服务端接管模式：接管已accept的socket，TcpChannel接管所有权
    explicit TcpChannel(QTcpSocket* existingSocket, QObject* parent = nullptr);

    bool ConnectTo(const QString& addr, quint16 port) override;
    void CloseChannel() override;
    qint64 SendRawFrame(const QByteArray& frame) override;
    bool IsChannelConnected() const override;

private slots:
    void OnReadyRead();
    void OnSocketError(QAbstractSocket::SocketError err);

private:
    // 初始化信号槽连接（两个构造函数共用）
    void InitConnections();

    QTcpSocket* mSocket = nullptr;
};
}

#endif
