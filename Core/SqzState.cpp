#include "SqzState.h"
#include <QDebug>
namespace Sqz {
// ==================== 单例 ====================
SqzState* SqzState::Instance() {
    static SqzState instance;
    return &instance;
}

// ==================== 构造 / 析构 ====================
SqzState::SqzState(QObject* parent) : QObject(parent) {}

SqzState::~SqzState() {
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        delete m_cleanupTimer;
    }
}

// ==================== 核心读写 ====================
void SqzState::Set(const QString& key, const QVariant& value,
                  const QString& source, bool forceUpdate) {
    bool shouldNotify = false;
    {
        QMutexLocker locker(&m_mutex);

        // 去重：值未变且不强制，直接忽略
        if (!forceUpdate && m_cache.contains(key) && m_cache[key].Value == value) {
            return;
        }

        m_cache[key] = DataItem(value, source);

        if (m_batching) {
            m_batchKeys.insert(key);
        } else {
            shouldNotify = true;
        }
    }

    if (shouldNotify) {
        Notify(key, value, source);
    }
}

DataItem SqzState::Get(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_cache.value(key, DataItem());
}

QVariant SqzState::Value(const QString& key, const QVariant& defaultValue) const {
    DataItem item = Get(key);
    return item.Valid ? item.Value : defaultValue;
}

bool SqzState::Has(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

bool SqzState::IsStale(const QString& key, int timeoutMs) const {
    return Get(key).IsStale(timeoutMs);
}

void SqzState::Remove(const QString& key) {
    {
        QMutexLocker locker(&m_mutex);
        if (!m_cache.contains(key)) return;
        m_cache.remove(key);
    }
    emit DataUpdated(key, QVariant(), "Removed");
}

void SqzState::Clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

// ==================== 批量更新 ====================
void SqzState::BeginBatch() {
    QMutexLocker locker(&m_mutex);
    m_batching = true;
    m_batchKeys.clear();
}

void SqzState::EndBatch() {
    QList<QPair<QString, QVariant>> items;
    QList<QString> sources;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_batching) {
            qWarning() << "SqzState::EndBatch() called without BeginBatch()";
            return;
        }
        for (const QString& key : m_batchKeys) {
            if (m_cache.contains(key)) {
                const DataItem& item = m_cache[key];
                items.append(qMakePair(key, item.Value));
                sources.append(item.Source);
            }
        }
        m_batchKeys.clear();
        m_batching = false;
    }

    for (int i = 0; i < items.size(); ++i) {
        Notify(items[i].first, items[i].second, sources[i]);
    }
}

// ==================== 自动清理 ====================
void SqzState::SetAutoCleanup(int intervalMs, int staleMs) {
    QMutexLocker locker(&m_mutex);
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        delete m_cleanupTimer;
        m_cleanupTimer = nullptr;
    }
    m_staleMs = staleMs;
    if (intervalMs > 0) {
        m_cleanupTimer = new QTimer(this);
        connect(m_cleanupTimer, &QTimer::timeout, this, &SqzState::CleanupStale);
        m_cleanupTimer->start(intervalMs);
    }
}

void SqzState::CleanupStale() {
    QList<QString> toRemove;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (it->IsStale(m_staleMs)) {
                toRemove.append(it.key());
            }
        }
        for (const QString& key : toRemove) {
            m_cache.remove(key);
        }
    }
    for (const QString& key : toRemove) {
        emit DataUpdated(key, QVariant(), "AutoClean");
    }
}

// ==================== 监控（Watch） ====================
int SqzState::Watch(const QString& key, std::function<void(const QVariant&)> callback) {
    QMutexLocker locker(&m_mutex);
    Watcher w;
    w.Id = m_nextWatcherId++;
    w.Callback = callback;
    m_watchers[key].append(w);
    return w.Id;
}

bool SqzState::Unwatch(const QString& key, int watcherId) {
    QMutexLocker locker(&m_mutex);
    auto it = m_watchers.find(key);
    if (it == m_watchers.end()) return false;

    QList<Watcher>& list = it.value();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].Id == watcherId) {
            list.removeAt(i);
            if (list.isEmpty()) {
                m_watchers.erase(it);
            }
            return true;
        }
    }
    return false;
}

void SqzState::Unwatch(const QString& key) {
    QMutexLocker locker(&m_mutex);
    m_watchers.remove(key);
}

// ==================== 内部通知 ====================
void SqzState::Notify(const QString& key, const QVariant& value, const QString& source) {
    // 1. 发射 Qt 信号
    emit DataUpdated(key, value, source);

    // 2. 执行 Watch 回调
    QList<Watcher> watchersCopy;
    {
        QMutexLocker locker(&m_mutex);
        if (m_watchers.contains(key)) {
            watchersCopy = m_watchers[key];
        }
    }
    for (const Watcher& w : watchersCopy) {
        if (w.Callback) {
            w.Callback(value);
        }
    }
}
}
