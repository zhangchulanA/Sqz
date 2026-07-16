#include "RequestManager.h"

namespace Net
{
// 构造函数：初始化QObject基类
RequestManager::RequestManager(QObject* parent)
    : QObject(parent)
{}

// 析构函数：释放所有未完成的请求上下文
RequestManager::~RequestManager()
{
    ReleaseAll();
}

// 销毁单个请求上下文：先移除、再断连、最后删除，防止QTimer析构时触发父对象回调
void RequestManager::DestroyContext(quint32 seq)
{
    if (!mPool.contains(seq))
        return;
    ReqContext* ctx = mPool[seq];
    mPool.remove(seq);
    ctx->timeoutTimer.stop();
    ctx->cycleTimer.stop();
    disconnect(&ctx->timeoutTimer, nullptr, this, nullptr);
    disconnect(&ctx->cycleTimer, nullptr, this, nullptr);
    delete ctx;
}

// 创建请求上下文：分配序列号，堆上创建ReqContext，启动超时/周期定时器，加入请求池
quint32 RequestManager::CreateRequest(const QByteArray& data, const RequestOption& opt,
                                     std::function<void(const QByteArray&)> succ,
                                     std::function<void(const RequestError&)> fail)
{
    quint32 seq = mSeqAuto++;
    // 在堆上创建上下文，父对象绑定管理器，利用Qt父子树管理生命周期
    ReqContext* ctx = new ReqContext(this);
    ctx->seq = seq;
    ctx->opt = opt;
    ctx->reqData = data;
    ctx->succCb = std::move(succ);
    ctx->failCb = std::move(fail);
    ctx->curCount = 0;

    // 配置并启动超时定时器（单次触发）
    ctx->timeoutTimer.setInterval(opt.timeoutMs);
    ctx->timeoutTimer.setSingleShot(true);
    connect(&ctx->timeoutTimer, &QTimer::timeout, this, [this, seq](){
        OnTimeoutTrigger(seq);
    });
    ctx->timeoutTimer.start();

    // 如果启用周期模式，配置并启动周期定时器
    if (opt.cycleEnable)
    {
        ctx->cycleTimer.setInterval(opt.cycleIntervalMs);
        connect(&ctx->cycleTimer, &QTimer::timeout, this, [this, seq](){
            OnCycleTrigger(seq);
        });
        ctx->cycleTimer.start();
    }
    mPool[seq] = ctx;
    return seq;
}

// 取消指定序列号的请求：立即销毁上下文
void RequestManager::CancelRequest(quint32 seq)
{
    DestroyContext(seq);
}

// 获取指定序列号的原始请求数据（供周期重发使用）
QByteArray RequestManager::GetRequestData(quint32 seq) const
{
    if (mPool.contains(seq))
        return mPool[seq]->reqData;
    return QByteArray();
}

// 断连清理：通知所有未完成请求失败（Disconnect错误码），并销毁上下文
void RequestManager::ClearAllDisconnect()
{
    QList<quint32> seqList = mPool.keys();
    for (quint32 seq : seqList)
    {
        ReqContext* ctx = mPool[seq];
        RequestError err;
        err.code = RspCode::Disconnect;
        err.msg = "Channel disconnected, request discard";
        if (ctx->failCb) ctx->failCb(err);
        DestroyContext(seq);
    }
}

// 释放所有请求：调用ClearAllDisconnect
void RequestManager::ReleaseAll()
{
    ClearAllDisconnect();
}

// 响应到达处理：匹配序列号，触发成功/失败回调，管理周期计数
void RequestManager::OnRecvResponse(quint32 seq, RspCode code, const QByteArray& payload)
{
    if (!mPool.contains(seq)) return;
    ReqContext* ctx = mPool[seq];
    // 收到响应后停止超时定时器
    ctx->timeoutTimer.stop();

    if (code == RspCode::Success)
    {
        auto succCb = ctx->succCb;
        if (succCb) succCb(payload);
    }
    else
    {
        auto failCb = ctx->failCb;
        if (failCb)
        {
            RequestError err;
            err.code = code;
            err.rawData = payload;
            switch (code)
            {
            case RspCode::BusinessFail: err.msg = "Business logic failed"; break;
            case RspCode::Timeout: err.msg = "Request timeout"; break;
            default: err.msg = "Unknown error";
            }
            failCb(err);
        }
    }

    // 回调中可能已销毁上下文（如StopCycleRequest），重新检查
    if (!mPool.contains(seq)) return;

    // 非周期模式：响应后立即销毁上下文
    if (!ctx->opt.cycleEnable)
    {
        DestroyContext(seq);
    }
    else
    {
        // 周期模式：递增计数，达到最大次数时销毁；cycleMaxCount==-1时永不销毁
        ctx->curCount++;
        if (ctx->opt.cycleMaxCount > 0 && ctx->curCount >= ctx->opt.cycleMaxCount)
        {
            DestroyContext(seq);
        }
    }
}

// 超时触发：通知调用方超时失败，非周期模式销毁上下文
void RequestManager::OnTimeoutTrigger(quint32 seq)
{
    if (!mPool.contains(seq)) return;
    ReqContext* ctx = mPool[seq];
    RequestError err;
    err.code = RspCode::Timeout;
    err.msg = "Request timeout";
    if (ctx->failCb) ctx->failCb(err);

    if (!ctx->opt.cycleEnable)
    {
        DestroyContext(seq);
    }
}

// 周期触发：发射SignalCycleResend通知上层重新发送
void RequestManager::OnCycleTrigger(quint32 seq)
{
    emit SignalCycleResend(seq);
}
}
