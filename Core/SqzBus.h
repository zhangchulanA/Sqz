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
 *          - 一次性监听（receiveOnce）：收到一次消息后自动注销
 *          - 临时屏蔽（blockReceiver）：不删除回调，动态控制是否响应消息
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
 *        SqzBus::receiveOnce(this, "once", []() {
 *            qDebug() << "只执行一次";
 *        });
 *
 *        // 临时屏蔽
 *        SqzBus::blockReceiver(this);   // 屏蔽该对象的所有回调
 *        SqzBus::unblockReceiver(this); // 恢复
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
    // ---------- 发送消息（无注释，保持简洁）----------
    static void Send(const QString &msgName);
    static void Send(const QString &msgName, const QVariant &args);
    static void Send(const QString &msgName, const QString &str);
    static void Send(const QString &msgName, const int &value);
    static void Send(const QString &msgName, const double &value);
    static void Send(const QString &msgName, const bool &value);
    static void Send(const QString &msgName, const QByteArray &value);
    static void Send(const QString &msgName, const qint64 &value);
    static void Send(const QString &msgName, const QVariantList &list);
    // 增加这个重载
    static void Send(const QString &msgName, const QVariantMap &map);

    // ---------- 注册监听（无注释，保持简洁）----------
    static void Receive(QObject *receiver, const QString &msgName,
                        std::function<void()> callback);
    static void Receive(QObject *receiver, const QString &msgName,
                        std::function<void(const QVariant&)> callback);

    // 模板版本：成员函数指针绑定
    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)()) {
        Receive(receiver, msgName, [instance, func]() { (instance->*func)(); });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QString&)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toString());
        });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(int)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toInt());
        });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(double)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toDouble());
        });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(bool)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toBool());
        });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QByteArray&)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toByteArray());
        });
    }

    template<typename T>
    static void Receive(QObject *receiver, const QString &msgName,
                        T* instance, void(T::*func)(const QVariantList&)) {
        Receive(receiver, msgName, [instance, func](const QVariant& data) {
            (instance->*func)(data.toList());
        });
    }

    // ---------- 一次性监听 ----------
    /**
     * @brief 一次性监听消息（收到一次后自动注销）
     * @param receiver 接收者对象（用于生命周期绑定和线程判断）
     * @param msgName  消息名称
     * @param callback 回调函数（收到消息后执行一次，然后自动 off）
     *
     * @note 执行完回调后会自动调用 off(receiver, msgName)，
     *       只注销该 (receiver, msgName) 条目，不影响其他 receiver 的同名消息。
     * @note 跨线程调用时，回调仍会在 receiver 所在线程执行。
     */
    static void ReceiveOnce(QObject *receiver, const QString &msgName,
                            std::function<void(const QVariant&)> callback);

    /**
     * @brief 一次性监听消息（无参数版本）
     * @param receiver 接收者对象
     * @param msgName  消息名称
     * @param callback 无参回调函数
     */
    static void ReceiveOnce(QObject *receiver, const QString &msgName,
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
     * @note 通常不需要手动调用，对象销毁时会自动清理。
     *       但可用于主动断开某对象与总线的所有连接。
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

    // ---------- 临时屏蔽接口 ----------
    /**
     * @brief 临时屏蔽某个对象的所有回调
     * @param receiver 要屏蔽的对象
     * @note 屏蔽后，该对象的所有回调都不会被执行（但回调条目依然存在）。
     *       适用于 UI 界面暂时不可交互的场景，避免恢复时需要重新注册。
     * @see unblockReceiver, isReceiverBlocked
     */
    static void BlockReceiver(QObject *receiver);

    /**
     * @brief 解除屏蔽，恢复对象的所有回调
     * @param receiver 要恢复的对象
     * @note 解除后，后续发送的消息会正常触发该对象的回调。
     *       不会影响屏蔽期间遗漏的消息（消息不会积压）。
     * @see blockReceiver, isReceiverBlocked
     */
    static void UnblockReceiver(QObject *receiver);

    /**
     * @brief 检查对象是否被屏蔽
     * @param receiver 要检查的对象
     * @return true 表示被屏蔽，false 表示未屏蔽或对象为空
     * @see blockReceiver, unblockReceiver
     */
    static bool IsReceiverBlocked(QObject *receiver);

    // ==============================
    // 内部实现（私有）
    // ==============================


private:
    struct CallbackItem
    {
        QPointer<QObject> receiver;          // 安全指针，自动检测对象销毁
        std::function<void(const QVariant&)> func;
    };

    void receiveImpl(QObject *receiver, const QString &msgName,
                     std::function<void(const QVariant&)> callback);
    void sendImpl(const QString &msgName, const QVariant &args);

    // 非静态 off 实现（供内部调用）
    void offImpl(QObject* obj, const QString& msgName);

private slots:
    /**
     * @brief 对象销毁时的清理槽函数
     * @param obj 被销毁的对象
     * @note 自动从 _callbacks 和 _connectedReceivers 中移除
     */
    void onReceiverDestroyed(QObject *obj);

private:
    QHash<QString, QList<CallbackItem>> _callbacks;      // 消息名 -> 回调列表
    QSet<QObject*>                      _connectedReceivers; // 已连接 destroyed 信号的对象，避免重复连接
    QSet<QObject*>                      _blockedReceivers;    // 被临时屏蔽的对象，其回调不会被执行
    mutable QMutex                      _mutex;               // 线程安全锁
};
}
#endif // SqzBus_H
