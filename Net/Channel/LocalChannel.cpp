#include "LocalChannel.h"

namespace Net
{
// 主动连接模式构造函数：内部创建新socket并初始化信号槽
LocalChannel::LocalChannel(QObject* parent)
    : AbstractChannel(parent)
{
    mSocket = new QLocalSocket(this);
    InitConnections();
}

// 服务端接管模式构造函数：接管已accept的socket，重设父对象为通道自身
LocalChannel::LocalChannel(QLocalSocket* existingSocket, QObject* parent)
    : AbstractChannel(parent)
{
    mSocket = existingSocket;
    mSocket->setParent(this);
    InitConnections();
}

// 初始化信号槽连接，统一处理readyRead/connected/disconnected/error
void LocalChannel::InitConnections()
{
    connect(mSocket, &QLocalSocket::readyRead, this, &LocalChannel::OnReadyRead);
    connect(mSocket, &QLocalSocket::connected, this, &AbstractChannel::SignalConnected);
    connect(mSocket, &QLocalSocket::disconnected, this, &AbstractChannel::SignalDisconnected);
    connect(mSocket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &LocalChannel::OnSocketError);
}

// 主动连接到本地服务端名称
bool LocalChannel::ConnectTo(const QString& addr, quint16 /*port*/)
{
    mSocket->connectToServer(addr);
    return true;
}

// 关闭通道连接
void LocalChannel::CloseChannel()
{
    mSocket->disconnectFromServer();
}

// 发送编码后的完整帧
qint64 LocalChannel::SendRawFrame(const QByteArray& frame)
{
    return mSocket->write(frame);
}

// 查询通道是否处于已连接状态
bool LocalChannel::IsChannelConnected() const
{
    return mSocket->state() == QLocalSocket::ConnectedState;
}

// readyRead回调：委托基类统一处理粘包解析
void LocalChannel::OnReadyRead()
{
    AbstractChannel::OnReadyRead(mSocket);
}

// socket错误回调：将底层错误信息向上传递
void LocalChannel::OnSocketError(QLocalSocket::LocalSocketError socketError)
{
    emit SignalError(mSocket->errorString());
}
}