#ifndef SQZSTATE_H
#define SQZSTATE_H

#include <QObject>
#include <QHash>
#include <QVariant>
#include <QDateTime>
#include <QMutex>
#include <QTimer>
#include <QSet>
#include <QPointer>
#include <functional>
#include "SqzGlobal.h"

/* ============================================================
   数据包裹结构体：存储值 + 时间戳 + 来源 + 有效性
   ============================================================ */

namespace Sqz {
struct SQZ_FRAMEWORK_API DataItem {
    QVariant Value;          // 存储任意类型数据
    QDateTime Timestamp;     // 存入时间（自动生成）
    QString Source;          // 数据来源（如 "COM1", "计算模块"）
    bool Valid;              // 是否有效

    DataItem() : Valid(false) {}
    DataItem(const QVariant& v, const QString& src)
        : Value(v), Timestamp(QDateTime::currentDateTime()), Source(src), Valid(true) {}

    // 判断数据是否过期（距今超过 ms 毫秒）
    bool IsStale(int ms) const {
        return !Valid || Timestamp.msecsTo(QDateTime::currentDateTime()) > ms;
    }

    // 数据年龄（毫秒），-1 表示无效
    qint64 Age() const {
        return Valid ? Timestamp.msecsTo(QDateTime::currentDateTime()) : -1;
    }
};

/* ============================================================
   SqzState 主类：全局数据中心，线程安全
   ============================================================ */
class SQZ_FRAMEWORK_API SqzState : public QObject {
    Q_OBJECT
public:
    // 单例
    static SqzState* Instance();

    // ---------- 核心读写 ----------
    // forceUpdate=true：强制更新，刷新时间戳并通知
    // forceUpdate=false：值未变则忽略（去重）
    void Set(const QString& key, const QVariant& value,
             const QString& source = "Unknown", bool forceUpdate = true);

    DataItem Get(const QString& key) const;                      // 获取完整包裹
    QVariant Value(const QString& key, const QVariant& defaultValue = QVariant()) const; // 直接取值
    bool Has(const QString& key) const;                         // 是否存在
    bool IsStale(const QString& key, int timeoutMs) const;      // 是否过期
    void Remove(const QString& key);                            // 删除
    void Clear();                                               // 清空

    // ---------- 批量更新 ----------
    void BeginBatch();    // 开始批量，Set() 暂不通知
    void EndBatch();      // 结束批量，统一通知

    // ---------- 自动清理 ----------
    void SetAutoCleanup(int intervalMs = 5000, int staleMs = 3000);

    // ---------- 值变化监控（Watch 模式） ----------
    // 监控 key，值变化时执行回调，返回监控 ID（无生命周期绑定，保持向后兼容）
    // sendCurrent=true：注册时若 key 已有值，立即推送当前值给回调（S4 修复）
    int Watch(const QString& key, std::function<void(const QVariant&)> callback, bool sendCurrent = false);

    // 监控 key（带生命周期绑定）：receiver 销毁时自动清理该监控，避免悬挂回调
    // sendCurrent=true：注册时若 key 已有值，立即推送当前值给回调（S4 修复）
    int Watch(QObject* receiver, const QString& key, std::function<void(const QVariant&)> callback, bool sendCurrent = false);

    // 取消监控：移除指定 key 下的某个监控者
    bool Unwatch(const QString& key, int watcherId);

    // 取消监控：移除指定 key 下的所有监控者
    void Unwatch(const QString& key);

    // 取消监控：移除某对象的所有监控（receiver 销毁时自动调用）
    void Unwatch(QObject* receiver);

signals:
    // Qt 信号（与 Watch 回调并存）
    void DataUpdated(const QString& key, const QVariant& value, const QString& source);

private:
    explicit SqzState(QObject* parent = nullptr);
    ~SqzState();

    SqzState(const SqzState&) = delete;
    SqzState& operator=(const SqzState&) = delete;

    void Notify(const QString& key, const QVariant& value, const QString& source);  // 内部通知
    void CleanupStale();   // 清理过期数据

    mutable QMutex m_mutex;
    QHash<QString, DataItem> m_cache;   // 数据仓库

    bool m_batching = false;
    QSet<QString> m_batchKeys;          // 批量中更新的 Key

    QTimer* m_cleanupTimer = nullptr;
    int m_staleMs = 3000;

    // 监控者
    struct Watcher {
        int Id;
        std::function<void(const QVariant&)> Callback;
        QPointer<QObject> Receiver;   // 监控者，用于生命周期绑定（销毁时自动清理，空表示不绑定）
        bool BoundReceiver = false;    // 是否绑定了 receiver（区分"未绑定"与"已失效"）
    };
    QHash<QString, QList<Watcher>> m_watchers;
    int m_nextWatcherId = 0;
    QSet<QObject*> m_connectedReceivers;  // 已连接 destroyed 信号的 receiver（去重，避免重复连接）
};

#define SqzStateIns SqzState::Instance()
}
#endif // SQZSTATE_H
