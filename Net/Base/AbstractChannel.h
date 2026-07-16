#ifndef ABSTRACTCHANNEL_H
#define ABSTRACTCHANNEL_H

#include "NetGlobal.h"
#include "FrameCodec.h"
#include <QObject>
#include <QIODevice>
#include <QByteArray>

namespace Net
{
// 通道抽象基类，统一Tcp/Local行为
class NET_EXPORT AbstractChannel : public QObject
{
    Q_OBJECT
public:
    explicit AbstractChannel(QObject* parent = nullptr);
    virtual ~AbstractChannel() = default;

    // 连接目标（客户端接口）
    virtual bool ConnectTo(const QString& addr, quint16 port) = 0;
    // 关闭通道
    virtual void CloseChannel() = 0;
    // 发送完整编码帧
    virtual qint64 SendRawFrame(const QByteArray& frame) = 0;
    // 获取连接状态
    virtual bool IsChannelConnected() const = 0;

protected:
    // IO可读回调，统一分包解析
    void OnReadyRead(QIODevice* dev);

signals:
    // 解析完成一整条业务帧
    void SignalRecvFullFrame(MsgType type, quint32 seq, RspCode code, QByteArray payload);
    void SignalConnected();
    void SignalDisconnected();
    void SignalError(const QString& errMsg);

private:
    FrameCodec mCodec;
    QByteArray mRecvBuf;
};
}

#endif
