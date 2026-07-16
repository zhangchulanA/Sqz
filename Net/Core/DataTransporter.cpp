#include "DataTransporter.h"
#include <QJsonDocument>
#include <QDataStream>
#include <QEventLoop>

namespace Net
{
UdpDiscover DataTransporter::sDiscover;

// 构造函数：初始化内部定时器及请求管理器的周期重发入口
DataTransporter::DataTransporter(QObject* parent)
    : QObject(parent)
{
    connect(&mReconnectTimer, &QTimer::timeout, this, &DataTransporter::OnReconnectTimer);
    connect(&mHbTimer, &QTimer::timeout, this, &DataTransporter::OnHeartBeatTimer);
    connect(&mHbTimeoutTimer, &QTimer::timeout, this, &DataTransporter::OnHeartBeatTimeout);
    connect(&mReqMgr, &RequestManager::SignalCycleResend, this, [this](quint32 seq){
        QByteArray data = mReqMgr.GetRequestData(seq);
        if (!data.isEmpty() && mSingleChannel && mSingleChannel->IsChannelConnected())
        {
            QByteArray frame = mCodec.EncodeRequest(seq, data);
            mSingleChannel->SendRawFrame(frame);
        }
    });
}

// 析构函数：释放所有请求、断开连接、停止监听
DataTransporter::~DataTransporter()
{
    mReqMgr.ReleaseAll();
    Disconnect();
    StopListen();
}

// 释放当前客户端通道，避免内存泄漏
void DataTransporter::ReleaseCurrentChannel()
{
    if (mSingleChannel)
    {
        mSingleChannel->CloseChannel();
        mSingleChannel->deleteLater();
        mSingleChannel = nullptr;
    }
}

// 单向推送：编码Push帧后通过单通道或广播发送
// 客户端模式：通过mSingleChannel发送；服务端模式：通过mClientChannels发送
qint64 DataTransporter::SendPush(const QByteArray& data)
{
    QByteArray frame = mCodec.EncodePush(data);
    if (mSingleChannel && mSingleChannel->IsChannelConnected())
    {
        return mSingleChannel->SendRawFrame(frame);
    }
    // 服务端模式：广播模式发全部，否则发第一个已连接的客户端
    if (!mClientChannels.isEmpty())
    {
        if (mBroadcastAll)
        {
            qint64 total = 0;
            for (auto ch : mClientChannels)
            {
                if (ch->IsChannelConnected()) total += ch->SendRawFrame(frame);
            }
            return total;
        }
        // 默认发第一个已连接的客户端
        for (auto ch : mClientChannels)
        {
            if (ch->IsChannelConnected()) { ch->SendRawFrame(frame); return frame.size(); }
        }
    }
    return -1;
}


// 单向推送：JSON格式便捷接口
qint64 DataTransporter::SendPush(const QJsonDocument& json)
{
    return SendPush(json.toJson(QJsonDocument::Compact));
}

qint64 DataTransporter::SendPush(const QJsonObject &jsonobject)
{
    return SendPush(QJsonDocument(jsonobject).toJson(QJsonDocument::Compact));
}


// 创建链式请求构造器
RequestBuilder DataTransporter::Request(const QByteArray& reqData)
{
    return RequestBuilder(*this, reqData);
}

// 创建链式请求构造器（JSON便捷接口）
RequestBuilder DataTransporter::Request(const QJsonDocument& json)
{
    return Request(json.toJson(QJsonDocument::Compact));
}

// 服务端回复成功：编码Response帧并发送
// 客户端模式：通过mSingleChannel回复；服务端模式：通过第一个已连接的客户端通道回复
DataTransporter& DataTransporter::ReplySuccess(quint32 seq, const QByteArray& rspData)
{
    QByteArray frame = mCodec.EncodeResponse(seq, RspCode::Success, rspData);
    if (mSingleChannel)
    {
        mSingleChannel->SendRawFrame(frame);
    }
    else
    {
        for (auto ch : mClientChannels)
        {
            if (ch->IsChannelConnected()) { ch->SendRawFrame(frame); break; }
        }
    }
    return *this;
}


// 服务端回复成功：JSON便捷接口
DataTransporter& DataTransporter::ReplySuccess(quint32 seq, const QJsonDocument& rspJson)
{
    return ReplySuccess(seq, rspJson.toJson());
}

DataTransporter &DataTransporter::ReplySuccess(quint32 seq, const QJsonObject &rspJson)
{
        QByteArray jsonData = QJsonDocument(rspJson).toJson(QJsonDocument::Compact);
        return ReplySuccess(seq,jsonData);
}

// 服务端回复失败：编码错误Response帧并发送
// 客户端模式：通过mSingleChannel回复；服务端模式：通过第一个已连接的客户端通道回复
DataTransporter& DataTransporter::ReplyFail(quint32 seq, const QByteArray& errInfo)
{
    QByteArray frame = mCodec.EncodeResponse(seq, RspCode::BusinessFail, errInfo);
    if (mSingleChannel)
    {
        mSingleChannel->SendRawFrame(frame);
    }
    else
    {
        for (auto ch : mClientChannels)
        {
            if (ch->IsChannelConnected()) { ch->SendRawFrame(frame); break; }
        }
    }
    return *this;
}

// 服务端回复失败：文本便捷接口
DataTransporter &DataTransporter::ReplyFail(quint32 seq, const QJsonObject &rspJson)
{
    QByteArray jsonData = QJsonDocument(rspJson).toJson(QJsonDocument::Compact);
    return ReplyFail(seq,jsonData);
}

// 服务端监听：根据TransMode自动选择TcpServer或LocalServer
DataTransporter &DataTransporter::Listen(const QString &addr, quint16 port, TransMode mode)
{
    StopListen();
    if (mode == TransMode::LocalIpc || (mode == TransMode::Auto && port == 0))
    {
        mLocalServer = new LocalServer(this);
        mLocalServer->Listen(addr);
        connect(mLocalServer, &LocalServer::SignalNewClient, this, [this](LocalChannel* cli)
        {
            mClientChannels.append(cli);
            connect(cli, &AbstractChannel::SignalRecvFullFrame, this, &DataTransporter::DispatchFullFrame);
        });
    }
    else
    {
        mTcpServer = new TcpServer(this);
        mTcpServer->Listen(port);
        connect(mTcpServer, &TcpServer::SignalNewClient, this, [this](TcpChannel* cli)
        {
            mClientChannels.append(cli);
            connect(cli, &AbstractChannel::SignalRecvFullFrame, this, &DataTransporter::DispatchFullFrame);
        });
    }
    return *this;
}

// 停止服务端监听，释放所有服务端资源
void DataTransporter::StopListen()
{
    if (mTcpServer != nullptr)
    {
        mTcpServer->Close();
        delete mTcpServer;
        mTcpServer = nullptr;
    }
    if (mLocalServer != nullptr)
    {
        mLocalServer->Close();
        delete mLocalServer;
        mLocalServer = nullptr;
    }
    mClientChannels.clear();
}

// 客户端连接：释放旧通道后创建新通道，连接信号槽
DataTransporter& DataTransporter::ConnectTo(const QString& targetAddr, quint16 port, TransMode mode)
{
    mLastTargetAddr = targetAddr;
    mLastPort = port;
    mLastMode = mode;

    // 释放旧通道，避免内存泄漏
    ReleaseCurrentChannel();

    mSingleChannel = CreateChannel(targetAddr, port, mode);
    connect(mSingleChannel, &AbstractChannel::SignalRecvFullFrame, this, &DataTransporter::DispatchFullFrame);
    connect(mSingleChannel, &AbstractChannel::SignalConnected, this, [this](){
        emit SignalConnected();
        mReconnectTimer.stop();
        if (mHeartBeatEnable)
        {
            mHbTimer.start(mHbSendInterval);
            mHbTimeoutTimer.start(mHbTimeout);
            mHbElapsed.start();
        }
    });
    connect(mSingleChannel, &AbstractChannel::SignalDisconnected, this, [this](){
        emit SignalDisconnected();
        mHbTimer.stop();
        mHbTimeoutTimer.stop();
        mReqMgr.ClearAllDisconnect();
        if (mAutoReconnect) mReconnectTimer.start(mReconnectInterval);
    });
    // 连接通道错误信号到上层
    connect(mSingleChannel, &AbstractChannel::SignalError, this, &DataTransporter::OnChannelError);

    mSingleChannel->ConnectTo(targetAddr, port);
    return *this;
}

// 配置自动重连
DataTransporter& DataTransporter::AutoReconnect(bool enable, int intervalMs)
{
    mAutoReconnect = enable;
    mReconnectInterval = intervalMs;
    return *this;
}

// 配置心跳：启动/停止发送和超时检测定时器
DataTransporter& DataTransporter::HeartBeat(bool enable, int sendInterval, int timeoutMs)
{
    mHeartBeatEnable = enable;
    mHbSendInterval = sendInterval;
    mHbTimeout = timeoutMs;
    if (enable && mSingleChannel && mSingleChannel->IsChannelConnected())
    {
        mHbTimer.start(mHbSendInterval);
        mHbTimeoutTimer.start(mHbTimeout);
        mHbElapsed.start();
    }
    else
    {
        mHbTimer.stop();
        mHbTimeoutTimer.stop();
    }
    return *this;
}

// 配置广播模式：推送时是否向所有客户端发送
DataTransporter& DataTransporter::BroadcastAll(bool enable)
{
    mBroadcastAll = enable;
    return *this;
}

// 重连定时回调：使用上次记录的参数重新连接
void DataTransporter::OnReconnectTimer()
{
    ConnectTo(mLastTargetAddr, mLastPort, mLastMode);
}

// 心跳发送定时回调：编码并发送心跳帧
void DataTransporter::OnHeartBeatTimer()
{
    QByteArray hb = mCodec.EncodeHeartBeat();
    if (mSingleChannel) mSingleChannel->SendRawFrame(hb);
}

// 心跳超时回调：心跳超时未收到任何响应，触发断连并上报错误
void DataTransporter::OnHeartBeatTimeout()
{
    if (mHbElapsed.elapsed() > mHbTimeout)
    {
        emit SignalNetError("Heartbeat timeout");
        Disconnect();
        // 如果开启了自动重连，由Disconnected信号中的lambda触发重连
    }
    else
    {
        // 收到过响应，重置计时器
        mHbElapsed.start();
    }
}

// 通道错误转发：将底层通道错误转发到上层SignalNetError
void DataTransporter::OnChannelError(const QString& errMsg)
{
    emit SignalNetError(errMsg);
}

// 完整帧分发：根据消息类型路由到不同的处理逻辑
void DataTransporter::DispatchFullFrame(MsgType type, quint32 seq, RspCode code, const QByteArray& payload)
{
    switch (type)
    {
    case MsgType::RawPush:
        emit SignalRecvPush(payload);
        break;
    case MsgType::Request:
        emit SignalRecvRequest(seq, payload);
        break;
    case MsgType::Response:
        mReqMgr.OnRecvResponse(seq, code, payload);
        break;
    case MsgType::HeartBeat:
        // 收到对端心跳响应，重置超时计时器
        if (mHeartBeatEnable) mHbElapsed.start();
        break;
    default: break;
    }
}

// 局域网设备扫描：委托静态UdpDiscover实例执行
QList<DeviceInfo> DataTransporter::ScanLanDevices(int waitMs, quint16 broadcastPort)
{
    return sDiscover.ScanLan(waitMs, broadcastPort);
}

// 发送链式请求：创建请求上下文，编码并发送帧
quint32 DataTransporter::SendChainRequest(const QByteArray& data, const RequestOption& opt,
                                         std::function<void(const QByteArray&)> onSucc,
                                         std::function<void(const RequestError&)> onFail)
{
    quint32 seq = mReqMgr.CreateRequest(data, opt, onSucc, onFail);
    QByteArray frame = mCodec.EncodeRequest(seq, data);
    if (mSingleChannel) mSingleChannel->SendRawFrame(frame);
    return seq;
}

// 同步发送请求：使用QEventLoop阻塞等待响应或超时
RequestResult DataTransporter::SendChainRequestSync(const QByteArray& data, const RequestOption& opt)
{
    RequestResult res;
    res.code = RspCode::Timeout;

    if (!mSingleChannel || !mSingleChannel->IsChannelConnected())
    {
        res.code = RspCode::Disconnect;
        return res;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    bool finished = false;

    // 发送请求，在回调中设置结果并退出事件循环
    quint32 seq = mReqMgr.CreateRequest(data, opt,
        [&](const QByteArray& payload) {
            if (!finished)
            {
                res.code = RspCode::Success;
                res.payload = payload;
                finished = true;
                loop.quit();
            }
        },
        [&](const RequestError& err) {
            if (!finished)
            {
                res.code = err.code;
                finished = true;
                loop.quit();
            }
        });

    // 超时定时器：超时后退出事件循环
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&](){
        if (!finished)
        {
            finished = true;
            loop.quit();
        }
    });
    timeoutTimer.start(opt.timeoutMs);

    // 编码并发送请求帧
    QByteArray frame = mCodec.EncodeRequest(seq, data);
    mSingleChannel->SendRawFrame(frame);

    // 阻塞等待结果
    loop.exec();

    return res;
}

// 停止指定序列号的周期请求
void DataTransporter::StopCycleRequest(quint32 seq)
{
    mReqMgr.CancelRequest(seq);
}

// 停止所有周期请求
void DataTransporter::StopAllCycleRequest()
{
    mReqMgr.ReleaseAll();
}

// 断开连接：关闭重连、心跳、释放通道
void DataTransporter::Disconnect()
{
    mAutoReconnect = false;
    mReconnectTimer.stop();
    mHbTimer.stop();
    mHbTimeoutTimer.stop();
    ReleaseCurrentChannel();
    mReqMgr.ClearAllDisconnect();
}

// 通道工厂：根据TransMode创建对应的通道实例
AbstractChannel* DataTransporter::CreateChannel(const QString& addr, quint16 port, TransMode mode)
{
    if (mode == TransMode::LocalIpc || (mode == TransMode::Auto && port == 0))
    {
        return new LocalChannel(this);
    }
    return new TcpChannel(this);
}
}
