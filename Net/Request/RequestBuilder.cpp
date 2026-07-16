#include "RequestBuilder.h"
#include "Net/Core/DataTransporter.h"

namespace Net
{
// 构造函数：保存DataTransporter引用和请求数据
RequestBuilder::RequestBuilder(DataTransporter& trans, QByteArray data)
    : mTrans(trans), mReqData(std::move(data))
{}

// 链式配置：设置请求超时时间（毫秒）
RequestBuilder& RequestBuilder::Timeout(int ms)
{
    mOpt.timeoutMs = ms;
    return *this;
}

// 链式配置：启用周期请求模式，intervalMs为间隔毫秒，maxCount为最大次数（-1无限）
RequestBuilder& RequestBuilder::Cycle(int intervalMs, int maxCount)
{
    mOpt.cycleEnable = true;
    mOpt.cycleIntervalMs = intervalMs;
    mOpt.cycleMaxCount = maxCount;
    return *this;
}

// 链式配置：设置重试次数
RequestBuilder& RequestBuilder::Retry(int cnt)
{
    mOpt.retryCount = cnt;
    return *this;
}

// 链式配置：设置成功回调
RequestBuilder& RequestBuilder::OnSuccess(std::function<void(const QByteArray&)> cb)
{
    mSuccCb = std::move(cb);
    return *this;
}

// 链式配置：设置失败回调
RequestBuilder& RequestBuilder::OnFailed(std::function<void(const RequestError&)> cb)
{
    mFailCb = std::move(cb);
    return *this;
}

// 执行异步发送：将配置和回调传递给DataTransporter，返回序列号
quint32 RequestBuilder::Call()
{
    return mTrans.SendChainRequest(mReqData, mOpt, mSuccCb, mFailCb);
}

// 执行同步发送：阻塞等待结果后返回
RequestResult RequestBuilder::Sync()
{
    return mTrans.SendChainRequestSync(mReqData, mOpt);
}
}