#ifndef DATATRANSPORTER_H
#define DATATRANSPORTER_H

#include "NetGlobal.h"
#include "NetDef.h"
#include "AbstractChannel.h"
#include "TcpChannel.h"
#include "LocalChannel.h"
#include "TcpServer.h"
#include "LocalServer.h"
#include "RequestBuilder.h"
#include "RequestManager.h"
#include "FrameCodec.h"
#include "UdpDiscover.h"
#include <QObject>
#include <QTimer>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace Net
{
// 对外唯一顶层入口，统一TCP/IPC、请求应答、心跳重连
class NET_EXPORT DataTransporter : public QObject
{
    Q_OBJECT
public:
    explicit DataTransporter(QObject* parent = nullptr);
    ~DataTransporter();

    // 单向推送，无应答
    qint64 SendPush(const QByteArray& data);
    qint64 SendPush(const QJsonDocument& json);
    qint64 SendPush(const QJsonObject& json);
    template<typename T> qint64 SendPushStruct(const T& obj);

    // 链式请求入口
    RequestBuilder Request(const QByteArray& reqData);
    RequestBuilder Request(const QJsonDocument& json);
    template<typename T> RequestBuilder Request(const T& obj);

    // 服务端回复接口（链式返回自身）
    DataTransporter& ReplySuccess(quint32 seq, const QByteArray& rspData);
    DataTransporter& ReplySuccess(quint32 seq, const QJsonDocument& rspJson);
    DataTransporter& ReplySuccess(quint32 seq, const QJsonObject& rspJson);

    DataTransporter& ReplyFail(quint32 seq, const QByteArray& errInfo);
    DataTransporter& ReplyFail(quint32 seq, const QJsonObject& rspJson);

    // 服务端监听 链式配置
    DataTransporter& Listen(const QString& addr, quint16 port = 0, TransMode mode = TransMode::Auto);
    void StopListen();

    // 客户端连接 链式配置
    DataTransporter& ConnectTo(const QString& targetAddr, quint16 port = 0, TransMode mode = TransMode::Auto);
    void Disconnect();

    // 配套网络配置 链式
    DataTransporter& AutoReconnect(bool enable, int intervalMs = 3000);
    DataTransporter& HeartBeat(bool enable, int sendInterval = 10000, int timeoutMs = 15000);
    DataTransporter& BroadcastAll(bool enable);

    // 周期请求管控
    void StopCycleRequest(quint32 seq);
    void StopAllCycleRequest();

    // 局域网扫描设备
    QList<DeviceInfo> ScanLanDevices(int waitMs = 1000, quint16 broadcastPort = 9999);

signals:
    void SignalRecvPush(const QByteArray& data);
    void SignalRecvRequest(quint32 seq, const QByteArray& reqData);
    void SignalConnected();
    void SignalDisconnected();
    void SignalNetError(const QString& errMsg);

public:
    // RequestBuilder底层调用发送
    quint32 SendChainRequest(const QByteArray& data, const RequestOption& opt,
                             std::function<void(const QByteArray&)> onSucc,
                             std::function<void(const RequestError&)> onFail);
    RequestResult SendChainRequestSync(const QByteArray& data, const RequestOption& opt);

    // 通道工厂，自动创建Tcp/Local通道
    AbstractChannel* CreateChannel(const QString& addr, quint16 port, TransMode mode);
    // 完整帧分发逻辑
    void DispatchFullFrame(MsgType type, quint32 seq, RspCode code, const QByteArray& payload);
    // 心跳定时回调
    void OnHeartBeatTimer();
    // 心跳超时回调
    void OnHeartBeatTimeout();
    // 重连定时回调
    void OnReconnectTimer();
    // 通道错误转发
    void OnChannelError(const QString& errMsg);

private:
    // 释放当前客户端通道
    void ReleaseCurrentChannel();

    TcpServer* mTcpServer = nullptr;
    LocalServer* mLocalServer = nullptr;
    QList<AbstractChannel*> mClientChannels;

    AbstractChannel* mSingleChannel = nullptr;
    RequestManager mReqMgr;

    bool mAutoReconnect = false;
    int mReconnectInterval = 3000;
    QTimer mReconnectTimer;
    QString mLastTargetAddr;
    quint16 mLastPort = 0;
    TransMode mLastMode = TransMode::Auto;

    bool mHeartBeatEnable = false;
    int mHbSendInterval = 10000;
    int mHbTimeout = 15000;
    QTimer mHbTimer;
    QTimer mHbTimeoutTimer;
    QElapsedTimer mHbElapsed;

    bool mBroadcastAll = false;
    FrameCodec mCodec;
    static UdpDiscover sDiscover;
};

// 模板实现放头文件
template<typename T>
RequestBuilder DataTransporter::Request(const T& obj)
{
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << obj;
    return Request(buf);
}

template<typename T>
qint64 DataTransporter::SendPushStruct(const T& obj)
{
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << obj;
    return SendPush(buf);
}
}

#endif
