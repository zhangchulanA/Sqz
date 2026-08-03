#include "SqzState.h"
#include <QDebug>
#include <QThread>
#include "Logger.h"
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
    // S2 修复：统一走 Notify，确保 Watch 回调也收到"已删除"通知（原仅 emit 信号）
    Notify(key, QVariant(), "Removed");
}

void SqzState::Clear() {
    // S2 修复：清空前收集所有 key，释放锁后逐个通知（确保 Watch 回调收到"已清空"通知）
    QList<QString> allKeys;
    {
        QMutexLocker locker(&m_mutex);
        allKeys = m_cache.keys();
        m_cache.clear();
    }
    for (const QString& key : allKeys) {
        Notify(key, QVariant(), "Cleared");
    }
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
    // S6 修复：参数范围校验
    // staleMs <= 0 会导致所有数据立即被认为过期（IsStale 中 msecsTo(now) > 0 对刚存入的数据为 true），
    // 清理器一跑就全删，属误用。拒绝设置，保护已有数据。
    // intervalMs <= 0 是合法意图（关闭自动清理），不拒绝。
    if (staleMs <= 0) {
        logwarn << "[SqzState] SetAutoCleanup: staleMs 必须 > 0，实际：" << staleMs
                << "，拒绝设置（避免误删所有数据）";
        return;
    }
    // QTimer 必须在其所属线程创建/销毁（线程亲和性要求）
    // 跨线程调用时投递到本对象线程执行（修复 Bug #10：跨线程 delete QTimer 崩溃）
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, intervalMs, staleMs]() {
            SetAutoCleanup(intervalMs, staleMs);
        }, Qt::QueuedConnection);
        return;
    }
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
        // S2 修复：统一走 Notify，确保 Watch 回调也收到"已清理"通知（原仅 emit 信号）
        Notify(key, QVariant(), "AutoClean");
    }
}

// ==================== 监控（Watch） ====================
// 无生命周期绑定的监控（向后兼容），转发到带 receiver 版本
int SqzState::Watch(const QString& key, std::function<void(const QVariant&)> callback, bool sendCurrent) {
    return Watch(nullptr, key, std::move(callback), sendCurrent);
}

// 带生命周期绑定的监控：receiver 销毁时自动清理，避免悬挂回调（修复 Bug #9）
// S4 修复：sendCurrent=true 时，注册后立即推送当前值给回调
int SqzState::Watch(QObject* receiver, const QString& key, std::function<void(const QVariant&)> callback, bool sendCurrent) {
    int watcherId = -1;
    QVariant currentValue;
    bool hasValue = false;
    {
        QMutexLocker locker(&m_mutex);
        Watcher w;
        w.Id = m_nextWatcherId++;
        w.Callback = callback;   // 拷贝（不能 move，锁外推送初始值还要用）
        w.Receiver = receiver;
        w.BoundReceiver = (receiver != nullptr);
        m_watchers[key].append(w);
        watcherId = w.Id;

        // S4 修复：sendCurrent=true 时锁内读取当前值（锁外再推送，避免回调中 Set/Get 死锁）
        if (sendCurrent && m_cache.contains(key)) {
            currentValue = m_cache[key].Value;
            hasValue = true;
        }

        // S3 修复：绑定 receiver 生命周期，destroyed 连接去重
        // 同一 receiver 多次 Watch 时只连接一次 destroyed，避免销毁时重复调用 Unwatch（虽幂等但浪费）
        if (receiver && !m_connectedReceivers.contains(receiver)) {
            m_connectedReceivers.insert(receiver);
            // DirectConnection：在 receiver 析构线程同步执行（此时 QPointer 仍有效，清理正确）
            connect(receiver, &QObject::destroyed, this, [this, receiver](QObject*) {
                Unwatch(receiver);  // 内部会清理 m_connectedReceivers 中的记录
            }, Qt::DirectConnection);
        }
    }
    // S4 修复：锁外推送初始值（避免回调中 Set/Get 死锁）
    // 注意：Watch 在调用方线程执行，初始值推送也在调用方线程同步执行
    if (hasValue && callback) {
        callback(currentValue);
    }
    return watcherId;
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

// 移除某对象的所有监控（receiver 销毁时自动调用，避免悬挂回调）
void SqzState::Unwatch(QObject* receiver) {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_watchers.begin(); it != m_watchers.end(); ) {
        QList<Watcher>& list = it.value();
        for (int i = list.size() - 1; i >= 0; --i) {
            // 精确匹配 receiver，或已失效的绑定监控（QPointer 因 receiver 销毁而置空）
            if (list[i].Receiver == receiver ||
                (list[i].BoundReceiver && list[i].Receiver.isNull())) {
                list.removeAt(i);
            }
        }
        if (list.isEmpty()) {
            it = m_watchers.erase(it);
        } else {
            ++it;
        }
    }
    // S3 修复：清理 destroyed 连接记录，允许后续重新 Watch 时重连
    m_connectedReceivers.remove(receiver);
}

// ==================== 内部通知 ====================
void SqzState::Notify(const QString& key, const QVariant& value, const QString& source) {
    // 1. 发射 Qt 信号（信号连接者自行处理跨线程）
    emit DataUpdated(key, value, source);

    // 2. 执行 Watch 回调：拷贝后锁外执行（避免回调中再次 Set/Get 死锁）
    QList<Watcher> watchersCopy;
    {
        QMutexLocker locker(&m_mutex);
        if (m_watchers.contains(key)) {
            watchersCopy = m_watchers[key];
        }
    }
    for (const Watcher& w : watchersCopy) {
        if (!w.Callback) continue;
        // 已绑定但 receiver 已失效：跳过（防御性，避免悬挂回调）
        if (w.BoundReceiver && w.Receiver.isNull()) continue;

        // S1 修复：跨线程安全投递
        // 绑定了 receiver 的 watcher：检查回调目标线程是否为当前线程
        // 跨线程时用 QueuedConnection 投递到 receiver 线程执行（UI 操作安全）
        if (w.BoundReceiver && w.Receiver->thread() != QThread::currentThread()) {
            QPointer<QObject> recv = w.Receiver;
            auto cb = w.Callback;            // 拷贝 std::function（可能捕获 this 等）
            QMetaObject::invokeMethod(recv, [cb, value]() {
                cb(value);
            }, Qt::QueuedConnection);
        } else {
            // 同线程或未绑定 receiver（旧 API 无线程信息）：直接同步执行
            w.Callback(value);
        }
    }
}
}
