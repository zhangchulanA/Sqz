#ifndef REQUESTBUILDER_H
#define REQUESTBUILDER_H

#include "NetGlobal.h"
#include "NetDef.h"
#include <functional>

namespace Net
{
class DataTransporter;

// 链式请求临时构造器，流式配置参数与回调
class NET_EXPORT RequestBuilder
{
public:
    RequestBuilder(DataTransporter& trans, QByteArray data);

    // 链式配置接口
    RequestBuilder& Timeout(int ms);
    RequestBuilder& Cycle(int intervalMs, int maxCount = -1);
    RequestBuilder& Retry(int cnt);
    RequestBuilder& OnSuccess(std::function<void(const QByteArray&)> cb);
    RequestBuilder& OnFailed(std::function<void(const RequestError&)> cb);

    // 执行发送
    quint32 Call();
    RequestResult Sync();

private:
    DataTransporter& mTrans;
    QByteArray mReqData;
    RequestOption mOpt;
    std::function<void(const QByteArray&)> mSuccCb;
    std::function<void(const RequestError&)> mFailCb;
};
}

#endif
