#include "TimeoutKeeper.h"
#include <QDebug>

namespace Sqz {
TimeoutKeeper::TimeoutKeeper(QObject *parent)
    : QObject(parent)
    , m_defaultTimeoutMs(5000)
    , m_started(false)
{
    connect(&m_checkTimer, &QTimer::timeout, this, &TimeoutKeeper::checkAllTimeouts);
}

TimeoutKeeper::~TimeoutKeeper()
{
    stop();
}

void TimeoutKeeper::start(int globalCheckIntervalMs)
{
    if (m_started) {
        qWarning() << "[TimeoutKeeper] Already started, call stop() first if need restart";
        return;
    }

    if (globalCheckIntervalMs <= 0) {
        qWarning() << "[TimeoutKeeper] Invalid check interval, using 1000ms";
        globalCheckIntervalMs = 1000;
    }

    m_checkTimer.start(globalCheckIntervalMs);
    m_started = true;
    qDebug() << "[TimeoutKeeper] Started, check interval:" << globalCheckIntervalMs << "ms";
}

void TimeoutKeeper::stop()
{
    if (!m_started)
        return;

    m_checkTimer.stop();
    {
        QMutexLocker locker(&m_mutex);
        m_infos.clear();
    }
    m_started = false;
    qDebug() << "[TimeoutKeeper] Stopped and cleared all monitors";
}

void TimeoutKeeper::refresh(const QString &key)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_infos.find(key);
    if (it == m_infos.end()) {
        // 自动注册，使用当前默认超时阈值
        TimeoutInfo info;
        info.lastReceivedTimer.start();
        info.timeoutMs = m_defaultTimeoutMs;
        info.isTimeout = false;
        m_infos.insert(key, info);
        qDebug() << "[TimeoutKeeper] Auto-registered key:" << key
                 << ", timeout:" << m_defaultTimeoutMs << "ms";
    } else {
        TimeoutInfo &info = it.value();
        if (info.isTimeout) {
            qDebug() << "[TimeoutKeeper] Key" << key << "resumed from timeout";
            info.isTimeout = false;
        }
        info.lastReceivedTimer.restart();
    }
}

void TimeoutKeeper::addMonitor(const QString &key, int timeoutMs, std::function<void()> onTimeout)
{
    if (timeoutMs <= 0) {
        qWarning() << "[TimeoutKeeper] Invalid timeoutMs for key:" << key << ", ignored";
        return;
    }

    QMutexLocker locker(&m_mutex);

    TimeoutInfo info;
    info.lastReceivedTimer.start();
    info.timeoutMs = timeoutMs;
    info.isTimeout = false;
    info.callback = onTimeout;
    m_infos.insert(key, info);
    qDebug() << "[TimeoutKeeper] Added monitor for key:" << key
             << ", timeout:" << timeoutMs << "ms";
}

void TimeoutKeeper::removeMonitor(const QString &key)
{
    QMutexLocker locker(&m_mutex);
    if (m_infos.remove(key) > 0) {
        qDebug() << "[TimeoutKeeper] Removed monitor for key:" << key;
    }
}

void TimeoutKeeper::setTimeoutHandler(const QString &key, std::function<void()> handler)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_infos.find(key);
    if (it != m_infos.end()) {
        it->callback = handler;
        qDebug() << "[TimeoutKeeper] Set timeout handler for key:" << key;
    } else {
        qWarning() << "[TimeoutKeeper] Cannot set handler for unknown key:" << key;
    }
}

bool TimeoutKeeper::isTimeout(const QString &key) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_infos.find(key);
    if (it == m_infos.end())
        return false;
    return it->isTimeout;
}

void TimeoutKeeper::setTimeoutMs(const QString &key, int ms)
{
    if (ms <= 0) {
        qWarning() << "[TimeoutKeeper] Invalid timeout ms for key:" << key;
        return;
    }

    QMutexLocker locker(&m_mutex);
    auto it = m_infos.find(key);
    if (it != m_infos.end()) {
        it->timeoutMs = ms;
        qDebug() << "[TimeoutKeeper] Updated timeout for key:" << key << "->" << ms << "ms";
    } else {
        qWarning() << "[TimeoutKeeper] Cannot set timeout for unknown key:" << key;
    }
}

int TimeoutKeeper::defaultTimeoutMs() const
{
    return m_defaultTimeoutMs;
}

void TimeoutKeeper::setDefaultTimeoutMs(int ms)
{
    if (ms <= 0) {
        qWarning() << "[TimeoutKeeper] Invalid default timeout, ignored";
        return;
    }
    m_defaultTimeoutMs = ms;
    qDebug() << "[TimeoutKeeper] Default timeout changed to:" << ms << "ms";
}

void TimeoutKeeper::setCheckInterval(int ms)
{
    if (ms <= 0) {
        qWarning() << "[TimeoutKeeper] Invalid check interval, ignored";
        return;
    }
    if (m_started) {
        m_checkTimer.start(ms);
        qDebug() << "[TimeoutKeeper] Check interval changed to:" << ms << "ms";
    } else {
        qWarning() << "[TimeoutKeeper] Keeper not started, interval change will take effect after start()";
    }
}

void TimeoutKeeper::checkAllTimeouts()
{
    QMutexLocker locker(&m_mutex);

    // 遍历所有 key，检查超时
    QMutableHashIterator<QString, TimeoutInfo> it(m_infos);
    while (it.hasNext()) {
        it.next();
        TimeoutInfo &info = it.value();

        // 已经超时的 key 不再重复触发
        if (info.isTimeout)
            continue;

        qint64 elapsed = info.lastReceivedTimer.elapsed();
        if (elapsed > info.timeoutMs) {
            info.isTimeout = true;
            const QString &key = it.key();

            qDebug() << "[TimeoutKeeper] Timeout for key:" << key
                     << ", elapsed:" << elapsed << "ms"
                     << ", threshold:" << info.timeoutMs << "ms";

            // 发出信号，供信号槽使用
            emit timeoutOccurred(key);

            // 调用自定义回调（如果存在）
            if (info.callback) {
                info.callback();
            }
        }
    }
}
}
