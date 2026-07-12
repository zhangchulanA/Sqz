#include "SqzBus.h"

// ==============================
// 单例实现
// ==============================
namespace Sqz {
SqzBus *SqzBus::instance()
{
    static SqzBus s_bus;
    return &s_bus;
}

// ==============================
// 发送消息（无注释，保持简洁）
// ==============================
void SqzBus::Send(const QString &msgName)
{
    Send(msgName, QVariant());
}

void SqzBus::Send(const QString &msgName, const QVariant &args)
{
    instance()->sendImpl(msgName, args);
}

void SqzBus::Send(const QString &msgName, const QString &str)
{
    Send(msgName, QVariant(str));
}

void SqzBus::Send(const QString &msgName, const int &value)
{
    Send(msgName, QVariant(value));
}

void SqzBus::Send(const QString &msgName, const double &value)
{
    Send(msgName, QVariant(value));
}

void SqzBus::Send(const QString &msgName, const bool &value)
{
    Send(msgName, QVariant(value));
}

void SqzBus::Send(const QString &msgName, const QByteArray &value)
{
    Send(msgName, QVariant(value));
}

void SqzBus::Send(const QString &msgName, const qint64 &value)
{
    Send(msgName, QVariant(value));
}

void SqzBus::Send(const QString &msgName, const QVariantList &list)
{
    Send(msgName, QVariant(list));
}

void SqzBus::Send(const QString &msgName, const QVariantMap &map)
{
    Send(msgName, QVariant(map));
}

// ==============================
// 注册监听（无注释，保持简洁）
// ==============================
void SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void (const QVariant &)> callback)
{
    instance()->receiveImpl(receiver, msgName, std::move(callback));
}

void SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void ()> callback)
{
    Receive(receiver, msgName, [callback](const QVariant&) {
        callback();
    });
}

// ==============================
// 一次性监听
// ==============================
void SqzBus::ReceiveOnce(QObject *receiver, const QString &msgName,
                          std::function<void(const QVariant&)> callback)
{
    if (!receiver || !callback) return;

    // 构造一个包装函数：执行原回调后自动注销
    std::function<void(const QVariant&)> wrapper =
        [receiver, msgName, callback](const QVariant& args) {
            callback(args);
            // 执行完毕后，注销当前 (receiver, msgName) 的回调
            SqzBus::Off(receiver, msgName);
        };

    Receive(receiver, msgName, wrapper);
}

void SqzBus::ReceiveOnce(QObject *receiver, const QString &msgName,
                          std::function<void()> callback)
{
    ReceiveOnce(receiver, msgName, [callback](const QVariant&) {
        callback();
    });
}

// ==============================
// 清理接口
// ==============================
void SqzBus::Clear(const QString &msgName)
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.remove(msgName);
}

void SqzBus::ClearAll()
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.clear();
}

void SqzBus::Reset(QObject *obj)
{
    if (!obj) return;
    QMutexLocker lock(&instance()->_mutex);
    for (auto msgIt = instance()->_callbacks.begin(); msgIt != instance()->_callbacks.end(); )
    {
        auto& callbackList = msgIt.value();
        // 倒序遍历删除，避免索引失效（已修正：i>=0）
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
}

// 静态 Off：调用实例的 offImpl
void SqzBus::Off(QObject* obj, const QString& msgName)
{
    instance()->offImpl(obj, msgName);
}

// 非静态 offImpl 实现（原 off 的逻辑，只是改名避免冲突）
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

// ==============================
// 内部实现
// ==============================
void SqzBus::receiveImpl(QObject *receiver, const QString &msgName,
                         std::function<void (const QVariant &)> callback)
{
    if (!receiver || msgName.isEmpty() || !callback)
        return;

    QMutexLocker lock(&_mutex);

    // 避免重复连接 destroyed 信号：同一对象只连接一次
    if (!_connectedReceivers.contains(receiver))
    {
        connect(receiver, &QObject::destroyed,
                this, &SqzBus::onReceiverDestroyed);
        _connectedReceivers.insert(receiver);
    }

    CallbackItem item;
    item.receiver = receiver;
    item.func     = std::move(callback);

    _callbacks[msgName].append(item);
}

void SqzBus::sendImpl(const QString &msgName, const QVariant &args)
{
    if (msgName.isEmpty())
        return;

    QMutexLocker lock(&_mutex);
    if (!_callbacks.contains(msgName))
        return;

    QList<CallbackItem> list = _callbacks[msgName];
    lock.unlock();

    for (const auto& item : list)
    {
        // QPointer 自动判空，检测对象是否已销毁
        if (!item.receiver)
            continue;

        // 检查该接收者是否被临时屏蔽
        {
            QMutexLocker lock(&_mutex);
            if (_blockedReceivers.contains(item.receiver))
                continue;
        }

        // 跨线程 → 安全队列投递
        if (item.receiver->thread() != QThread::currentThread())
        {
            QMetaObject::invokeMethod(item.receiver, [=]() {
                // 再次检查，防止投递期间对象被销毁或屏蔽状态变化
                if (!item.receiver)
                    return;
                {
                    QMutexLocker lock(&instance()->_mutex);
                    if (instance()->_blockedReceivers.contains(item.receiver))
                        return;
                }
                item.func(args);
            }, Qt::QueuedConnection);
        }
        // 同线程 → 直接执行（高效）
        else
        {
            item.func(args);
        }
    }
}

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
            if (items[i].receiver == obj)
                items.removeAt(i);
        }
        if (items.isEmpty())
            it = _callbacks.erase(it);
        else
            ++it;
    }
}
}
