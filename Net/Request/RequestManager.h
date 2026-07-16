#ifndef REQUESTMANAGER_H
#define REQUESTMANAGER_H

#include "NetGlobal.h"
#include "NetDef.h"
#include <QObject>
#include <QTimer>
#include <QHash>
#include <functional>

namespace Net
{
// 单条请求上下文
struct ReqContext
{
    quint32 seq;
    RequestOption opt;
    QByteArray reqData;
    std::function<void(const QByteArray&)> succCb;
    std::function<void(const RequestError&)> failCb;
    QTimer timeoutTimer;
    QTimer cycleTimer;
    int curCount = 0;

    explicit ReqContext(QObject* parent = nullptr)
        : timeoutTimer(parent), cycleTimer(parent)
    {}
};

// 请求池管理器：维护序列号、超时、周期、回调分发
class NET_EXPORT RequestManager : public QObject
{
    Q_OBJECT
public:
    explicit RequestManager(QObject* parent = nullptr);
    ~RequestManager();

    quint32 CreateRequest(const QByteArray& data, const RequestOption& opt,
                          std::function<void(const QByteArray&)> succ,
                          std::function<void(const RequestError&)> fail);
    void CancelRequest(quint32 seq);
    QByteArray GetRequestData(quint32 seq) const;
    void ClearAllDisconnect();
    void ReleaseAll();
    void OnRecvResponse(quint32 seq, RspCode code, const QByteArray& payload);

signals:
    void SignalCycleResend(quint32 seq);

private slots:
    void OnTimeoutTrigger(quint32 seq);
    void OnCycleTrigger(quint32 seq);
private:
    // 改为存储堆指针，规避QTimer拷贝问题
    QHash<quint32, ReqContext*> mPool;
    quint32 mSeqAuto = 1;
    // 释放单条上下文
    void DestroyContext(quint32 seq);
};
}

#endif
