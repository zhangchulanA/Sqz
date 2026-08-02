#ifndef SqzBus_H
#define SqzBus_H

/**
 * @file SqzBus.h
 * @brief 线程安全的 Qt 消息总线（单例模式）
 *
 * @details 功能特性：
 *          - 线程安全的发送/接收，自动处理跨线程投递
 *          - 自动生命周期管理：对象销毁时自动清理其所有回调
 *          - 支持多种数据类型重载（int, double, QString, QByteArray, QVariantList 等）
 *          - 支持成员函数指针的模板绑定，类型安全
 *          - 一次性监听（ReceiveOnce）：收到一次消息后自动注销
 *          - 临时屏蔽（BlockReceiver）：不删除回调，动态控制是否响应消息
 *          - 精确删除（OffById）：按回调 ID 删除单条回调，不影响同 (receiver, msgName) 的其他回调
 *
 * @usage 基本用法：
 *        // 注册监听
 *        SqzBus::Receive(this, "test", [](const QVariant& data) {
 *            qDebug() << data.toString();
 *        });
 *
 *        // 发送消息
 *        SqzBus::Send("test", "hello");
 *
 *        // 一次性监听
 *        SqzBus::ReceiveOnce(this, "once", []() {
 *            qDebug() << "只执行一次";
 *        });
 *
 *        // 临时屏蔽
 *        SqzBus::BlockReceiver(this);   // 屏蔽该对象的所有回调
 *        SqzBus::UnblockReceiver(this); // 恢复
 *
 *        // 精确删除单条回调（Receive 返回 ID 后可用）
 *        quint64 id = SqzBus::Receive(this, "evt", handler);
 *        SqzBus::OffById(id);
 */

#include <QObject>
#include <QHash>
#include <QList>
#include <QVariant>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <functional>
#include <QMetaObject>
#include <QThread>
#include <QJsonObject>
#include <atomic>
#include "SqzGlobal.h"
namespace Sqz {
class SQZ_FRAMEWORK_API SqzBus : public QObject
{
    Q_OBJECT

    // ==============================
    // 构造与单例（私有化）
    // ==============================
private:
    explicit SqzBus(QObject *parent = nullptr) : QObject(parent) {}
    SqzBus(const SqzBus&) = delete;
    SqzBus& operator=(const SqzBus&) = delete;

    static SqzBus* instance();

    // ==============================
    // 对外公开接口
    // ==============================
public:
    // ---------- 发送消息 ----------
    /// 发送无参数消息
    static void Send(const QString &msgName);
    /// 发送 QVariant 参数消息
    static void Send(const QString &msgName, const QVariant &args);
    /// 发送 QString 参数消息
    static void Send(const QString &msgName, const QString &str);
    /// 发送 int 参数消息
    static void Send(const QString &msgName, const int &value);
    /// 发送 double 参数消息
    static void Send(const QString &msgName, const double &value);
    /// 发送 bool 参数消息
    static void Send(const QString &msgName, const bool &value);
    /// 发送 QByteArray 参数消息
    static void Send(const QString &msgName, const QByteArray &value);
    /// 发送 qint64 参数消息
    static void Send(const QString &msgName, const qint64 &value);
    /// 发送 QVariantList 参数消息
    static void Send(const QString &msgName, const QVariantList &list);
    /// 发送 QVariantMap 参数消息
    static void Send(const QString &msgName, const QVariantMap &map);
    /// 发送 QJsonObject 参数消息
    static void Send(const QString &msgName, const QJsonObject &map);

    // ---------- 注册监听 ----------
    /**
     * @brief 注册无参数回调监听
     * @param receiver 接收者对象（用于生命周期绑定和线程判断）
     * @param msgName  消息名称
     * @param callback 无参回调函数
     * @return 回调唯一 ID，可用于 OffById 精确删除
     */
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        std::function<void()> callback);

    /**
     * @brief 注册带参数回调监听
     * @param receiver 接收者对象（用于生命周期绑定和线程判断）
     * @param msgName  消息名称
     * @param callback 带 QVariant 参数的回调函数
     * @return 回调唯一 ID，可用于 OffById 精确删除
     */
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        std::function<void(const QVariant&)> callback);

    // 模板版本：成员函数指针绑定
    /**
     * @brief 模板绑定无参成员函数
     * @tparam T 接收者类型
     * @param receiver  生命周期绑定对象（销毁时自动注销）
     * @param msgName   消息名称
     * @param instance  实际执行成员函数的对象指针
     * @param func      成员函数指针 void T::func()
     * @return 回调唯一 ID
     * @note 修复：instance 用 QPointer 捕获，防止 instance 先于 receiver 销毁时悬挂
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)()) {
        // 修复：用 QPointer 捕获 instance，防止悬挂指针
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func]() {
            if (!safeInstance) return;
            (safeInstance.data()->*func)();
        });
    }

    /**
     * @brief 模板绑定 QString 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QString&)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toString());
        });
    }

    /**
     * @brief 模板绑定 int 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(int)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toInt());
        });
    }

    /**
     * @brief 模板绑定 double 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(double)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toDouble());
        });
    }

    /**
     * @brief 模板绑定 bool 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(bool)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toBool());
        });
    }

    /**
     * @brief 模板绑定 QByteArray 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QByteArray&)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toByteArray());
        });
    }

    /**
     * @brief 模板绑定 QVariantList 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QVariantList&)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toList());
        });
    }

    /**
     * @brief 模板绑定 QVariantMap 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QVariantMap&)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toMap());
        });
    }

    /**
     * @brief 模板绑定 QJsonObject 参数成员函数
     * @return 回调唯一 ID
     */
    template<typename T>
    static quint64 Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QJsonObject&)) {
        QPointer<T> safeInstance = instance;
        return Receive(receiver, msgName, [safeInstance, func](const QVariant& data) {
            if (!safeInstance) return;
            (safeInstance.data()->*func)(data.toJsonObject());
        });
    }

    // ---------- 一次性监听 ----------
    /**
     * @brief 一次性监听消息（收到一次后自动注销）
     * @param receiver 接收者对象（用于生命周期绑定和线程判断）
     * @param msgName  消息名称
     * @param callback 回调函数（收到消息后执行一次，然后自动注销）
     * @return 回调唯一 ID
     *
     * @note 修复：原实现执行后调用 Off(receiver, msgName) 会删除该 (receiver, msgName)
     *       下的所有回调（包括非 once 的）。新实现使用 once 标志，sendImpl 执行后
     *       只按 id 精确删除该条回调，不影响同 (receiver, msgName) 的其他回调。
     * @note 跨线程调用时，回调仍会在 receiver 所在线程执行。
     */
    static quint64 ReceiveOnce(QObject *receiver, const QString &msgName,
                            std::function<void(const QVariant&)> callback);

    /**
     * @brief 一次性监听消息（无参数版本）
     * @param receiver 接收者对象
     * @param msgName  消息名称
     * @param callback 无参回调函数
     * @return 回调唯一 ID
     */
    static quint64 ReceiveOnce(QObject *receiver, const QString &msgName,
                            std::function<void()> callback);

    // ---------- 清理接口 ----------
    /**
     * @brief 清空指定消息的所有回调
     * @param msgName 消息名称
     * @note 所有监听该消息的 receiver 都会失去响应，慎用
     */
    static void Clear(const QString &msgName);

    /**
     * @brief 清空所有消息的所有回调
     * @note 总线进入空状态，所有监听全部失效
     */
    static void ClearAll();

    /**
     * @brief 批量清理：删除指定对象的所有回调（所有消息）
     * @param obj 要清理的对象
     * @note 修复：同时清理 _blockedReceivers 中的记录（原实现遗漏）
     */
    static void Reset(QObject* obj);

    /**
     * @brief 清理指定对象在某一条消息上的回调
     * @param obj     接收者对象
     * @param msgName 消息名称
     * @note 只删除匹配 (obj, msgName) 的回调条目，
     *       如果该消息下该对象有多个回调，会全部删除。
     */
    static void Off(QObject* obj, const QString& msgName);

    /**
     * @brief 按回调 ID 精确删除单条回调
     * @param id Receive/ReceiveOnce 返回的回调唯一 ID
     * @note 修复：新增接口，用于 ReceiveOnce 执行后精确删除，
     *       不影响同 (receiver, msgName) 的其他回调。
     *       线程安全，可在任意线程调用。
     */
    static void OffById(quint64 id);

    // ---------- 临时屏蔽接口 ----------
    /**
     * @brief 临时屏蔽某个对象的所有回调
     * @param receiver 要屏蔽的对象
     * @note 屏蔽后，该对象的所有回调都不会被执行（但回调条目依然存在）。
     *       适用于 UI 界面暂时不可交互的场景，避免恢复时需要重新注册。
     */
    static void BlockReceiver(QObject *receiver);

    /**
     * @brief 解除屏蔽，恢复对象的所有回调
     * @param receiver 要恢复的对象
     * @note 解除后，后续发送的消息会正常触发该对象的回调。
     *       不会影响屏蔽期间遗漏的消息（消息不会积压）。
     */
    static void UnblockReceiver(QObject *receiver);

    /**
     * @brief 检查对象是否被屏蔽
     * @param receiver 要检查的对象
     * @return true 表示被屏蔽，false 表示未屏蔽或对象为空
     */
    static bool IsReceiverBlocked(QObject *receiver);

    /**
     * @brief 调试接口：查询某消息的回调条目数
     * @param msgName 消息名称
     * @return 该消息当前注册的回调条目数
     * @note 线程安全，可用于测试验证 destroyed 清理是否生效
     */
    static int CallbackCount(const QString &msgName);

    // ==============================
    // 内部实现（私有）
    // ==============================

private:
    /**
     * @brief 单条回调条目
     * 修复：增加 id（唯一标识）和 once（一次性标志）字段
     */
    struct CallbackItem
    {
        quint64 id = 0;                                   // 唯一标识，用于 OffById 精确删除
        QPointer<QObject> receiver;                       // 安全指针，sendImpl 中判空用
        QObject* receiverRaw = nullptr;                   // 裸指针，onReceiverDestroyed 中比较用
                                                          // （QPointer 在 destroyed 信号时可能已置空）
        std::function<void(const QVariant&)> func;
        bool once = false;                                // 是否一次性回调（执行后自动删除）
    };

    /// 注册回调的内部实现
    quint64 receiveImpl(QObject *receiver, const QString &msgName,
                     std::function<void(const QVariant&)> callback,
                     bool once = false);

    /// 发送消息的内部实现
    void sendImpl(const QString &msgName, const QVariant &args);

    /// 非静态 off 实现（供内部调用）
    void offImpl(QObject* obj, const QString& msgName);

    /// 按 id 精确删除单条回调（供 OffById 和 sendImpl once 清理调用）
    void offByIdImpl(quint64 id);

    /// 全局回调 ID 自增生成器（线程安全）
    static std::atomic<quint64> s_nextId;

private slots:
    /**
     * @brief 对象销毁时的清理槽函数
     * @param obj 被销毁的对象
     * @note 修复：connect 改为 Qt::DirectConnection，确保在析构线程同步执行。
     *       此时 QPointer 仍有效，items[i].receiver == obj 比较成功，清理正确。
     *       原 AutoConnection 跨线程变 QueuedConnection，信号到达时 QPointer 已置空，
     *       nullptr == obj → false → 清理失败 → 回调残留 → 内存泄漏。
     */
    void onReceiverDestroyed(QObject *obj);

private:
    QHash<QString, QList<CallbackItem>> _callbacks;          // 消息名 -> 回调列表
    QSet<QObject*>                      _connectedReceivers; // 已连接 destroyed 信号的对象，避免重复连接
    QSet<QObject*>                      _blockedReceivers;   // 被临时屏蔽的对象，其回调不会被执行
    mutable QMutex                      _mutex;              // 线程安全锁
};
}
#endif // SqzBus_H
