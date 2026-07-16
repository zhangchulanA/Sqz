#include "TcpChannel.h"

namespace Net
{
// 主动连接模式构造函数：内部创建新socket并初始化信号槽
TcpChannel::TcpChannel(QObject* parent)
    : AbstractChannel(parent)
{
    mSocket = new QTcpSocket(this);
    InitConnections();
}

// 服务端接管模式构造函数：接管已accept的socket，重设父对象为通道自身
TcpChannel::TcpChannel(QTcpSocket* existingSocket, QObject* parent)
    : AbstractChannel(parent)
{
    mSocket = existingSocket;
    mSocket->setParent(this);
    InitConnections();
}

// 初始化信号槽连接，统一处理readyRead/connected/disconnected/error
void TcpChannel::InitConnections()
{
    connect(mSocket, &QTcpSocket::readyRead, this, &TcpChannel::OnReadyRead);
    connect(mSocket, &QTcpSocket::connected, this, &AbstractChannel::SignalConnected);
    connect(mSocket, &QTcpSocket::disconnected, this, &AbstractChannel::SignalDisconnected);
    connect(mSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &TcpChannel::OnSocketError);
}

// 主动连接到目标地址和端口
bool TcpChannel::ConnectTo(const QString& addr, quint16 port)
{
    mSocket->connectToHost(addr, port);
    return true;
}

// 关闭通道连接
void TcpChannel::CloseChannel()
{
    mSocket->disconnectFromHost();
}

// 发送编码后的完整帧
qint64 TcpChannel::SendRawFrame(const QByteArray& frame)
{
    return mSocket->write(frame);
}

// 查询通道是否处于已连接状态
bool TcpChannel::IsChannelConnected() const
{
    return mSocket->state() == QTcpSocket::ConnectedState;
}

// readyRead回调：委托基类统一处理粘包解析
void TcpChannel::OnReadyRead()
{
    AbstractChannel::OnReadyRead(mSocket);
}

// socket错误回调：将底层错误信息向上传递
void TcpChannel::OnSocketError(QAbstractSocket::SocketError)
{
    emit SignalError(mSocket->errorString());
}
}