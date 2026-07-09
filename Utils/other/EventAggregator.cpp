#include "EventAggregator.h"
namespace Sqz::Utils {
void EventAggregator::clear(QObject* sender)
{
    if (!sender) {
        clearAll();
        return;
    }
    auto it = m_senderMap.find(sender);
    if (it == m_senderMap.end()) return;
    for (const QString& id : it.value()) {
        if (m_subscriptions.contains(id)) {
            if (m_subscriptions[id].timer) {
                m_subscriptions[id].timer->stop();
                m_subscriptions[id].timer->deleteLater();
            }
            m_subscriptions.remove(id);
        }
    }
    m_senderMap.remove(sender);
}

void EventAggregator::clearAll()
{
    for (auto& sub : m_subscriptions) {
        if (sub.timer) {
            sub.timer->stop();
            sub.timer->deleteLater();
        }
    }
    m_subscriptions.clear();
    m_senderMap.clear();
}

void EventAggregator::trigger(const QString& id, const QVariantList& args)
{
    if (!m_subscriptions.contains(id)) return;
    auto& sub = m_subscriptions[id];

    if (!sub.receiver) {
        m_subscriptions.remove(id);
        return;
    }

    // 断开旧定时器连接，防止重复
    disconnect(sub.timer, &QTimer::timeout, this, nullptr);

    switch (sub.mode) {
    case Mode::Debounce:
        sub.timer->stop();
        sub.lastArgs = args;
        sub.hasPending = true;
        connect(sub.timer, &QTimer::timeout, this, [this, id]() {
            execute(id);
        });
        sub.timer->start(sub.delay);
        break;

    case Mode::Throttle:
        if (!sub.timer->isActive()) {
            // 立即执行一次
            sub.handler(args);
            sub.hasPending = false;
            sub.lastArgs.clear();
            connect(sub.timer, &QTimer::timeout, this, [this, id]() {
                execute(id);
            });
            sub.timer->start(sub.delay);
        } else {
            // 记录最后一次参数，待周期结束执行
            sub.lastArgs = args;
            sub.hasPending = true;
        }
        break;

    case Mode::Merge:
        sub.accumulatedArgs.append(args);
        sub.hasPending = true;
        if (!sub.timer->isActive()) {
            connect(sub.timer, &QTimer::timeout, this, [this, id]() {
                execute(id);
            });
            sub.timer->start(sub.delay);
        }
        break;
    }
}

void EventAggregator::execute(const QString& id)
{
    if (!m_subscriptions.contains(id)) return;
    auto& sub = m_subscriptions[id];

    if (!sub.receiver) {
        m_subscriptions.remove(id);
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
            // 重新开始节流周期
            connect(sub.timer, &QTimer::timeout, this, [this, id]() {
                execute(id);
            });
            sub.timer->start(sub.delay);
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
}
