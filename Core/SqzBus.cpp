#include "SqzBus.h"

// 静态成员初始化：回调 ID 自增生成器（线程安全）
namespace Sqz {
std::atomic<quint64> SqzBus::s_nextId{0};


// 单例实现
SqzBus *SqzBus::instance()
{
    static SqzBus s_bus;
    return &s_bus;
}


// 发送无参数消息（转发为空 QVariant）
void SqzBus::Send(const QString &msgName)
{
    Send(msgName, QVariant());
}

// 发送 QVariant 参数消息
void SqzBus::Send(const QString &msgName, const QVariant &args)
{
    instance()->sendImpl(msgName, args);
}

// 发送 QString 参数消息
void SqzBus::Send(const QString &msgName, const QString &str)
{
    Send(msgName, QVariant(str));
}

// 发送 int 参数消息
void SqzBus::Send(const QString &msgName, const int &value)
{
    Send(msgName, QVariant(value));
}

// 发送 double 参数消息
void SqzBus::Send(const QString &msgName, const double &value)
{
    Send(msgName, QVariant(value));
}

// 发送 bool 参数消息
void SqzBus::Send(const QString &msgName, const bool &value)
{
    Send(msgName, QVariant(value));
}

// 发送 QByteArray 参数消息
void SqzBus::Send(const QString &msgName, const QByteArray &value)
{
    Send(msgName, QVariant(value));
}

// 发送 qint64 参数消息
void SqzBus::Send(const QString &msgName, const qint64 &value)
{
    Send(msgName, QVariant(value));
}

// 发送 QVariantList 参数消息
void SqzBus::Send(const QString &msgName, const QVariantList &list)
{
    Send(msgName, QVariant(list));
}

// 发送 QVariantMap 参数消息
void SqzBus::Send(const QString &msgName, const QVariantMap &map)
{
    Send(msgName, QVariant(map));
}

// 发送 QJsonObject 参数消息
void SqzBus::Send(const QString &msgName, const QJsonObject &map)
{
    Send(msgName, QVariant(map));
}

// ==============================
// 注册监听
// ==============================

// 注册带参数回调：调用 receiveImpl
quint64 SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void (const QVariant &)> callback)
{
    return instance()->receiveImpl(receiver, msgName, std::move(callback));
}

// 注册无参回调：包装为带参回调后转发
quint64 SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void ()> callback)
{
    return Receive(receiver, msgName, [callback](const QVariant&) {
        callback();
    });
}


// 清空指定消息的所有回调
void SqzBus::Clear(const QString &msgName)
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.remove(msgName);
}

// 清空所有消息的所有回调
void SqzBus::ClearAll()
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.clear();
    instance()->_blockedReceivers.clear();
    // 注意：_connectedReceivers 不清空，因为 destroyed 信号连接仍需保留
    // 对象销毁时会自动触发 onReceiverDestroyed 清理
}

// 批量清理：删除指定对象的所有回调
void SqzBus::Reset(QObject *obj)
{
    if (!obj) return;
    QMutexLocker lock(&instance()->_mutex);
    for (auto msgIt = instance()->_callbacks.begin(); msgIt != instance()->_callbacks.end(); )
    {
        auto& callbackList = msgIt.value();
        // 倒序遍历删除，避免索引失效
        for (int i = callbackList.size() - 1; i >= 0; --i)
        {
            if (callbackList[i].receiver == obj)
                callbackList.removeAt(i);
        }
        if (callbackList.isEmpty())
            msgIt = instance()->_callbacks.erase(msgIt);
        else
            ++msgIt;
    }
    instance()->_blockedReceivers.remove(obj);
}

// 静态 Off：调用实例的 offImpl（批量删除该对象在该消息上的所有回调）
void SqzBus::Off(QObject* obj, const QString& msgName)
{
    instance()->offImpl(obj, msgName);
}

// 非静态 offImpl 实现（按对象+消息名批量删除）
void SqzBus::offImpl(QObject *obj, const QString &msgName)
{
    if (!obj) return;
    QMutexLocker lock(&_mutex);
    if (!_callbacks.contains(msgName)) return;
    auto& list = _callbacks[msgName];
    for (int i = list.size() - 1; i >= 0; --i)
    {
        if (list[i].receiver == obj)
            list.removeAt(i);
    }
    if (list.isEmpty())
        _callbacks.remove(msgName);
}

// ==============================
// 临时屏蔽接口
// ==============================

void SqzBus::BlockReceiver(QObject *receiver)
{
    if (!receiver) return;
    QMutexLocker lock(&instance()->_mutex);
    instance()->_blockedReceivers.insert(receiver);
}

void SqzBus::UnblockReceiver(QObject *receiver)
{
    if (!receiver) return;
    QMutexLocker lock(&instance()->_mutex);
    instance()->_blockedReceivers.remove(receiver);
}

bool SqzBus::IsReceiverBlocked(QObject *receiver)
{
    if (!receiver) return false;
    QMutexLocker lock(&instance()->_mutex);
    return instance()->_blockedReceivers.contains(receiver);
}

// 调试接口：查询某消息的回调条目数
int SqzBus::CallbackCount(const QString &msgName)
{
    QMutexLocker lock(&instance()->_mutex);
    auto it = instance()->_callbacks.find(msgName);
    return it != instance()->_callbacks.end() ? it.value().size() : 0;
}

// ==============================
// 内部实现
// ==============================

// 注册回调的内部实现
quint64 SqzBus::receiveImpl(QObject *receiver, const QString &msgName,
                         std::function<void (const QVariant &)> callback)
{
    if (!receiver || msgName.isEmpty() || !callback)
        return 0;

    QMutexLocker lock(&_mutex);

    // 避免重复连接 destroyed 信号：同一对象只连接一次
    if (!_connectedReceivers.contains(receiver))
    {
        connect(receiver, &QObject::destroyed,
                this, &SqzBus::onReceiverDestroyed,
                Qt::DirectConnection);
        _connectedReceivers.insert(receiver);
    }

    CallbackItem item;
    item.id           = ++s_nextId;   // 生成全局唯一 ID
    item.receiver     = receiver;     // QPointer：sendImpl 中判空用
    item.receiverRaw  = receiver;     // 裸指针：onReceiverDestroyed 中比较用
    item.func         = std::move(callback);

    _callbacks[msgName].append(item);

    return item.id;
}

// 发送消息的内部实现
void SqzBus::sendImpl(const QString &msgName, const QVariant &args)
{
    if (msgName.isEmpty())
        return;

    QList<CallbackItem> list;
    QSet<QObject*> blockedSnapshot;
    {
        QMutexLocker lock(&_mutex);
        if (!_callbacks.contains(msgName))
            return;
        list = _callbacks[msgName];
        blockedSnapshot = _blockedReceivers;
    }

    for (const auto& item : list)
    {
        QObject* recvPtr = item.receiver.data();
        if (!recvPtr) continue;
        if (blockedSnapshot.contains(recvPtr)) continue;

        const bool isCrossThread = (recvPtr->thread() != QThread::currentThread());

        if (isCrossThread)
        {
            // 投递：lambda 只负责"执行一次"，不再负责移除
            QMetaObject::invokeMethod(recvPtr, [item, args]() {
                if (!item.receiver) return;
                if (IsReceiverBlocked(item.receiver.data())) return;
                try {
                    item.func(args);
                } catch (...) {
                    qWarning("SqzBus: callback exception suppressed (cross-thread)");
                }
                // 注意：这里不再调 OffById
            }, Qt::QueuedConnection);
        }
        else
        {
            // 同线程：直接执行，然后标记移除
            try {
                item.func(args);
            } catch (...) {
                qWarning("SqzBus: callback exception suppressed (same-thread)");
            }
        }
    }
}

// 对象销毁时的清理槽函数
void SqzBus::onReceiverDestroyed(QObject *obj)
{
    QMutexLocker lock(&_mutex);

    // 从已连接集合中移除
    _connectedReceivers.remove(obj);

    // 从屏蔽集合中移除（如果存在）
    _blockedReceivers.remove(obj);

    // 清理所有回调
    for (auto it = _callbacks.begin(); it != _callbacks.end(); )
    {
        auto& items = it.value();
        for (int i = items.size() - 1; i >= 0; --i)
        {
            if (items[i].receiverRaw == obj)
                items.removeAt(i);
        }
        if (items.isEmpty())
            it = _callbacks.erase(it);
        else
            ++it;
    }
}

} // namespace Sqz
