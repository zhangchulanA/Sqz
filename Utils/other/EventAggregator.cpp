#include "EventAggregator.h"

namespace Sqz {

// ============================================================
//  removeSubscriptionById
//  按 id 安全移除单条订阅（断开两个连接句柄 + 停timer + 清反查映射）。
//  destroyed 回调 / clear / clearAll / 重入 aggregate 共用本函数。
// ============================================================
void EventAggregator::removeSubscriptionById(const QString& id)
{
    auto it = m_subscriptions.find(id);
    if (it == m_subscriptions.end()) return;

    Subscription& sub = it.value();
    // 修复❺：断开旧 signal + destroyed 连接，防止僵尸 lambda
    // Qt 5 兼容：使用 QObject::disconnect(conn) 静态函数，而非 Qt 6 的成员 disconnect()。
    if (static_cast<bool>(sub.signalConn))    QObject::disconnect(sub.signalConn);
    if (static_cast<bool>(sub.destroyedConn)) QObject::disconnect(sub.destroyedConn);
    // 停 timer + deleteLater（timer 以 this 为父，析构也会删，双重安全）
    if (sub.timer) {
        sub.timer->stop();
        sub.timer->deleteLater();
    }
    // 维护反查映射：从 senderMap 中移除 id
    if (!sub.receiver.isNull()) {
        auto mIt = m_senderMap.find(sub.receiver.data());
        if (mIt != m_senderMap.end()) {
            mIt->removeOne(id);
            if (mIt->isEmpty()) m_senderMap.erase(mIt);
        }
    }
    // 最后删除主表条目
    m_subscriptions.erase(it);
}

// ============================================================
//  clear(QObject* sender)
//  修复❶：从 m_senderMap 找 sender 的所有 id，逐个 removeSubscriptionById。
//  修复❺：每个 id 都会精确断连接、停 timer、清反查，不再泄漏。
// ============================================================
void EventAggregator::clear(QObject* sender)
{
    if (!sender) { clearAll(); return; }

    auto it = m_senderMap.find(sender);
    if (it == m_senderMap.end()) return;

    // 拷贝一份 id 列表，因为 removeSubscriptionById 会改 m_senderMap
    const QStringList ids = it.value();
    for (const QString& id : ids)
        removeSubscriptionById(id);
    // 防御性：若还有残留（通常 removeSubscriptionById 会清），强删
    m_senderMap.remove(sender);
}

// ============================================================
//  clearAll
//  先收集所有 id（避免遍历时改容器），再逐个安全移除。
// ============================================================
void EventAggregator::clearAll()
{
    const QStringList ids = QStringList(m_subscriptions.keys());
    for (const QString& id : ids)
        removeSubscriptionById(id);
    // 防御性清理
    m_subscriptions.clear();
    m_senderMap.clear();
}

// ============================================================
//  trigger(const QString& id, const QVariantList& args)
//  信号触发入口：更新参数 + 重连 timer timeout + 启动 timer。
// ============================================================
void EventAggregator::trigger(const QString& id, const QVariantList& args)
{
    auto it = m_subscriptions.find(id);
    if (it == m_subscriptions.end()) return;
    Subscription& sub = it.value();

    // sender 被析构但 id 未被 destroyed 回调清除的防御性兜底（QPointer 判空）
    if (!sub.receiver) {
        removeSubscriptionById(id);
        return;
    }
    if (!sub.timer) {
        // 防御：timer 不可用，不做任何事（理论不会发生，timer 在 aggregate 时创建）
        return;
    }

    // 注意：timer→execute 的 lambda 连接已在 aggregate() 创建 Subscription 时一次性建立，
    // 此处不要再断/连，否则 Merge/Throttle 模式 timer active 期间后续 trigger 会把已建立
    // 的有效连接断掉，导致 execute 永远不执行。

    switch (sub.mode) {
    case Mode::Debounce:
        // 防抖：每次事件重置计时
        sub.timer->stop();
        sub.lastArgs   = args;
        sub.hasPending = true;
        sub.timer->start(sub.delay);
        break;

    case Mode::Throttle:
        if (!sub.timer->isActive()) {
            // 节流首次：立即执行本次参数，并启动下一窗口用于收敛期内事件
            sub.handler(args);
            sub.hasPending = false;
            sub.lastArgs.clear();
            sub.timer->start(sub.delay);
        } else {
            // 节流窗口期内：仅记录最后一次参数，待 timeout 统一处理
            sub.lastArgs   = args;
            sub.hasPending = true;
        }
        break;

    case Mode::Merge:
        // accumulatedArgs 为嵌套结构：每项是一个 QVariantList(单次事件所有参数)
        sub.accumulatedArgs.append(QVariant::fromValue(args));
        sub.hasPending = true;
        if (!sub.timer->isActive()) {
            // 首次事件开启合并窗口，后续事件仅追加不重启
            sub.timer->start(sub.delay);
        }
        break;
    }
}

// ============================================================
//  execute(const QString& id)
//  timer 到期：从 sub 中取相应参数调用用户 handler。
//  Throttle：若 hasPending 则执行后重启下一周期（仍有 pending 时）。
// ============================================================
void EventAggregator::execute(const QString& id)
{
    auto it = m_subscriptions.find(id);
    if (it == m_subscriptions.end()) return;
    Subscription& sub = it.value();

    if (!sub.receiver) {
        removeSubscriptionById(id);
        return;
    }

    switch (sub.mode) {
    case Mode::Debounce:
        if (sub.hasPending) {
            sub.handler(sub.lastArgs);
            sub.hasPending = false;
            sub.lastArgs.clear();
        }
        break;

    case Mode::Throttle:
        if (sub.hasPending) {
            sub.handler(sub.lastArgs);
            sub.hasPending = false;
            sub.lastArgs.clear();
            // 有 pending 说明窗口期内仍有事件，重启下一个节流周期继续收敛
            // timer→execute 连接在 Subscription 创建时已建立，此处只需启动计时即可。
            if (sub.timer) {
                sub.timer->start(sub.delay);
            }
        }
        break;

    case Mode::Merge:
        if (!sub.accumulatedArgs.isEmpty()) {
            sub.handler(sub.accumulatedArgs);
            sub.accumulatedArgs.clear();
            sub.hasPending = false;
        }
        break;
    }
}

} // namespace Sqz
