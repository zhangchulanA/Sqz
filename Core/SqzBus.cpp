#include "SqzBus.h"

// ==============================
// 静态成员初始化：回调 ID 自增生成器（线程安全）
// ==============================
namespace Sqz {
std::atomic<quint64> SqzBus::s_nextId{0};

// ==============================
// 单例实现
// ==============================
SqzBus *SqzBus::instance()
{
    static SqzBus s_bus;
    return &s_bus;
}

// ==============================
// 发送消息（静态转发层）
// ==============================

/// 发送无参数消息（转发为空 QVariant）
void SqzBus::Send(const QString &msgName)
{
    Send(msgName, QVariant());
}

/// 发送 QVariant 参数消息
void SqzBus::Send(const QString &msgName, const QVariant &args)
{
    instance()->sendImpl(msgName, args);
}

/// 发送 QString 参数消息
void SqzBus::Send(const QString &msgName, const QString &str)
{
    Send(msgName, QVariant(str));
}

/// 发送 int 参数消息
void SqzBus::Send(const QString &msgName, const int &value)
{
    Send(msgName, QVariant(value));
}

/// 发送 double 参数消息
void SqzBus::Send(const QString &msgName, const double &value)
{
    Send(msgName, QVariant(value));
}

/// 发送 bool 参数消息
void SqzBus::Send(const QString &msgName, const bool &value)
{
    Send(msgName, QVariant(value));
}

/// 发送 QByteArray 参数消息
void SqzBus::Send(const QString &msgName, const QByteArray &value)
{
    Send(msgName, QVariant(value));
}

/// 发送 qint64 参数消息
void SqzBus::Send(const QString &msgName, const qint64 &value)
{
    Send(msgName, QVariant(value));
}

/// 发送 QVariantList 参数消息
void SqzBus::Send(const QString &msgName, const QVariantList &list)
{
    Send(msgName, QVariant(list));
}

/// 发送 QVariantMap 参数消息
void SqzBus::Send(const QString &msgName, const QVariantMap &map)
{
    Send(msgName, QVariant(map));
}

/// 发送 QJsonObject 参数消息
void SqzBus::Send(const QString &msgName, const QJsonObject &map)
{
    Send(msgName, QVariant(map));
}

// ==============================
// 注册监听
// ==============================

/// 注册带参数回调：调用 receiveImpl，once 默认 false
quint64 SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void (const QVariant &)> callback)
{
    return instance()->receiveImpl(receiver, msgName, std::move(callback), false);
}

/// 注册无参回调：包装为带参回调后转发
quint64 SqzBus::Receive(QObject *receiver, const QString &msgName,
                     std::function<void ()> callback)
{
    return Receive(receiver, msgName, [callback](const QVariant&) {
        callback();
    });
}

// ==============================
// 一次性监听
// ==============================

/// 一次性监听（带参数版本）
/// 修复：原实现在 wrapper 中调用 Off(receiver, msgName) 会删除该 (receiver, msgName)
///       下的所有回调（包括非 once 的）。新实现通过 once 标志，sendImpl 执行后
///       只按 id 精确删除该条回调，不影响其他回调。
quint64 SqzBus::ReceiveOnce(QObject *receiver, const QString &msgName,
                          std::function<void(const QVariant&)> callback)
{
    return instance()->receiveImpl(receiver, msgName, std::move(callback), true);
}

/// 一次性监听（无参数版本）
quint64 SqzBus::ReceiveOnce(QObject *receiver, const QString &msgName,
                          std::function<void()> callback)
{
    return ReceiveOnce(receiver, msgName, [callback](const QVariant&) {
        callback();
    });
}

// ==============================
// 清理接口
// ==============================

/// 清空指定消息的所有回调
void SqzBus::Clear(const QString &msgName)
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.remove(msgName);
}

/// 清空所有消息的所有回调
/// 修复：同时清理 _blockedReceivers
void SqzBus::ClearAll()
{
    QMutexLocker lock(&instance()->_mutex);
    instance()->_callbacks.clear();
    instance()->_blockedReceivers.clear();
    // 注意：_connectedReceivers 不清空，因为 destroyed 信号连接仍需保留
    // 对象销毁时会自动触发 onReceiverDestroyed 清理
}

/// 批量清理：删除指定对象的所有回调
/// 修复：同时清理 _blockedReceivers 中的记录
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
    // 修复：同步清理屏蔽集合
    instance()->_blockedReceivers.remove(obj);
}

/// 静态 Off：调用实例的 offImpl（批量删除该对象在该消息上的所有回调）
void SqzBus::Off(QObject* obj, const QString& msgName)
{
    instance()->offImpl(obj, msgName);
}

/// 静态 OffById：按回调 ID 精确删除单条回调
void SqzBus::OffById(quint64 id)
{
    instance()->offByIdImpl(id);
}

/// 非静态 offImpl 实现（按对象+消息名批量删除）
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

/// 按 id 精确删除单条回调（新增实现）
/// 遍历所有消息的回调列表，找到匹配 id 的条目删除
void SqzBus::offByIdImpl(quint64 id)
{
    if (id == 0) return;
    QMutexLocker lock(&_mutex);
    for (auto msgIt = _callbacks.begin(); msgIt != _callbacks.end(); )
    {
        auto& list = msgIt.value();
        for (int i = list.size() - 1; i >= 0; --i)
        {
            if (list[i].id == id)
            {
                list.removeAt(i);
                // id 全局唯一，找到即停止遍历当前列表
                if (list.isEmpty())
                    _callbacks.erase(msgIt);
                return;
            }
        }
        if (list.isEmpty())
            msgIt = _callbacks.erase(msgIt);
        else
            ++msgIt;
    }
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

/// 调试接口：查询某消息的回调条目数
int SqzBus::CallbackCount(const QString &msgName)
{
    QMutexLocker lock(&instance()->_mutex);
    auto it = instance()->_callbacks.find(msgName);
    return it != instance()->_callbacks.end() ? it.value().size() : 0;
}

// ==============================
// 内部实现
// ==============================

/// 注册回调的内部实现
/// 修复1：connect destroyed 改为 Qt::DirectConnection，跨线程时在析构线程同步执行
/// 修复2：生成唯一 id 并设置 once 标志
quint64 SqzBus::receiveImpl(QObject *receiver, const QString &msgName,
                         std::function<void (const QVariant &)> callback,
                         bool once)
{
    if (!receiver || msgName.isEmpty() || !callback)
        return 0;

    QMutexLocker lock(&_mutex);

    // 避免重复连接 destroyed 信号：同一对象只连接一次
    if (!_connectedReceivers.contains(receiver))
    {
        // 修复：使用 Qt::DirectConnection，确保 destroyed 信号在析构线程同步执行。
        // 此时 QPointer 仍有效，onReceiverDestroyed 中 items[i].receiver == obj 比较成功。
        // 原 AutoConnection 跨线程变 QueuedConnection，信号到达时 QPointer 已置空，
        // nullptr == obj → false → 清理失败 → 回调残留 → 内存泄漏。
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
    item.once         = once;

    _callbacks[msgName].append(item);

    return item.id;
}

/// 发送消息的内部实现
/// 修复1：锁内一次性拷贝 blockedReceivers 快照，锁外不再循环加锁（性能提升）
/// 修复2：handler 调用 try/catch 兜底，防止异常击穿整个派发
/// 修复3：once 回调执行后按 id 精确删除，不影响同 (receiver, msgName) 的其他回调
void SqzBus::sendImpl(const QString &msgName, const QVariant &args)
{
    if (msgName.isEmpty())
        return;

    // 锁内：拷贝回调列表快照 + blockedReceivers 快照
    // QList/QSet 的 COW 机制使拷贝为浅拷贝（仅指针数组），成本可控
    QList<CallbackItem> list;
    QSet<QObject*> blockedSnapshot;
    {
        QMutexLocker lock(&_mutex);
        if (!_callbacks.contains(msgName))
            return;
        list = _callbacks[msgName];          // COW 浅拷贝
        blockedSnapshot = _blockedReceivers;  // COW 浅拷贝
    }

    // 收集同线程执行完毕后需要删除的 once 回调 ID
    QList<quint64> onceToRemove;

    for (const auto& item : list)
    {
        // 修复：先取裸指针再判空，缩小 TOCTOU 竞争窗口
        QObject* recvPtr = item.receiver.data();
        if (!recvPtr)
            continue;

        // 修复：用快照检查屏蔽状态，不再每次循环加锁
        if (blockedSnapshot.contains(recvPtr))
            continue;

        // 跨线程 → 安全队列投递
        if (recvPtr->thread() != QThread::currentThread())
        {
            // 按值捕获 item（含 QPointer + func + id + once）和 args
            QMetaObject::invokeMethod(recvPtr, [=]() {
                // 再次检查，防止投递期间对象被销毁或屏蔽状态变化
                if (!item.receiver)
                    return;
                if (IsReceiverBlocked(item.receiver.data()))
                    return;
                // 修复：try/catch 防止异常击穿
                try {
                    item.func(args);
                } catch (...) {
                    qWarning("SqzBus: callback exception suppressed (cross-thread)");
                }
                // 修复：once 回调执行后按 id 精确删除
                if (item.once)
                    OffById(item.id);
            }, Qt::QueuedConnection);
        }
        // 同线程 → 直接执行（高效）
        else
        {
            // 修复：try/catch 防止异常击穿整个派发
            try {
                item.func(args);
            } catch (...) {
                qWarning("SqzBus: callback exception suppressed (same-thread)");
            }
            // 修复：once 回调执行后按 id 精确删除
            if (item.once)
                onceToRemove.append(item.id);
        }
    }

    // 统一删除同线程执行的 once 回调
    for (quint64 id : onceToRemove)
        offByIdImpl(id);
}

/// 对象销毁时的清理槽函数
/// 修复：connect 改为 DirectConnection 后，此槽在析构线程同步执行，
///       QPointer 仍有效，items[i].receiver == obj 比较成功，清理正确。
void SqzBus::onReceiverDestroyed(QObject *obj)
{
    QMutexLocker lock(&_mutex);

    // 从已连接集合中移除
    _connectedReceivers.remove(obj);

    // 从屏蔽集合中移除（如果存在）
    _blockedReceivers.remove(obj);

    // 清理所有回调
    // 修复：使用 receiverRaw 裸指针比较，不依赖 QPointer 有效性
    // （QPointer 在 destroyed 信号发出时可能已置空，导致比较失败）
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
