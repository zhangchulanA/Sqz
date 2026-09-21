#ifndef EVENTAGGREGATOR_H
#define EVENTAGGREGATOR_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QPointer>
#include <QVariantList>
#include <functional>
#include <QMetaMethod>
#include <QMetaObject>
#include <type_traits>
#include <QDebug>
#include "SqzGlobal.h"

/**
 * @brief 事件聚合器（全局单例，线程依附于首次调用 instance() 的线程）
 *
 * 三种聚合模式：
 *   - Debounce 防抖：信号停止触发后延迟 delayMs 执行，仅执行最后一次。
 *     handler 参数：QVariantList = 单次信号参数（扁平，如 args[0]=text）。
 *
 *   - Throttle 节流：第一次信号立即执行，之后每 delayMs 毫秒最多执行一次
 *     （取窗口期最后一次参数）。handler 参数同 Debounce。
 *
 *   - Merge 合并：窗口期内所有信号参数累积，delayMs 到齐后统一执行一次。
 *     handler 参数：QVariantList 中每项 QVariant 内嵌 QVariantList，
 *     即 args[i].toList() 取第 i 次信号参数。推荐使用 mergeEvents(args) 解包。
 *
 * 线程安全：Debounce/Throttle/Merge/Clear 可跨线程调用；
 *           内部 wrapper 始终在单例线程执行（context=this），跨线程自动 Queued。
 */
namespace Sqz {
class SQZ_FRAMEWORK_API EventAggregator : public QObject
{
    Q_OBJECT
public:
    /** @brief 聚合模式枚举 */
    enum class Mode { Debounce, Throttle, Merge };

    /** @brief 全局单例（C++11 Magic Statics：线程安全懒初始化）*/
    static EventAggregator* instance() {
        static EventAggregator s_instance;
        return &s_instance;
    }

    // ========== 静态便捷接口 ==========
    /** @brief Debounce 防抖：最后一次信号后 delayMs 执行最后一次参数 */
    template<typename Sender, typename... Args>
    static void Debounce(Sender* sender, void (Sender::*signal)(Args...),
                         std::function<void(const QVariantList&)> handler,
                         int delayMs = 100) {
        instance()->aggregate(sender, signal, std::move(handler), Mode::Debounce, delayMs);
    }

    /** @brief Throttle 节流：每 delayMs 最多执行一次（首次立即，后续取最后）*/
    template<typename Sender, typename... Args>
    static void Throttle(Sender* sender, void (Sender::*signal)(Args...),
                         std::function<void(const QVariantList&)> handler,
                         int delayMs = 1000) {
        instance()->aggregate(sender, signal, std::move(handler), Mode::Throttle, delayMs);
    }

    /** @brief Merge 合并：delayMs 窗口内累积所有事件后一次执行 */
    template<typename Sender, typename... Args>
    static void Merge(Sender* sender, void (Sender::*signal)(Args...),
                      std::function<void(const QVariantList&)> handler,
                      int delayMs = 200) {
        instance()->aggregate(sender, signal, std::move(handler), Mode::Merge, delayMs);
    }

    /** @brief 清除 sender 全部订阅（传 nullptr 等价于 ClearAll）*/
    static void Clear(QObject* sender = nullptr) { instance()->clear(sender); }
    /** @brief 全局清除所有订阅 */
    static void ClearAll()                        { instance()->clearAll(); }

    /**
     * @brief Merge 模式辅助解包：
     *        将 handler 收到的嵌套 QVariantList 转成 QVector<QVariantList>。
     *        events[i] = 第 i 次信号的扁平参数列表。
     *        非 Merge 扁平列表调用时将整体包为单事件（防御性兼容）。
     */
    static QVector<QVariantList> mergeEvents(const QVariantList& args) {
        QVector<QVariantList> events;
        events.reserve(args.size());
        if (args.isEmpty()) return events;
        // 若首元素本身是 QVariantList，则判定为 Merge 嵌套结构
        if (args.front().userType() == qMetaTypeId<QVariantList>()) {
            for (const QVariant& v : args)
                events.append(v.toList());
        } else {
            // 扁平列表：包成 1 个事件，防御性兼容
            events.append(args);
        }
        return events;
    }

    // ========== 实例方法 ==========

    /**
     * @brief 建立订阅：静态接口内部调用。
     *
     * 已修复：
     *   ❶ 维护 m_senderMap[sender] 反向映射 → Clear(sender) 生效
     *   ❷ 重入同 id 时先 disconnect 旧 signalConn/destroyedConn + 停旧 timer
     *   ❸ wrapper 的 connect context=this → 跨线程安全
     *   ❹ 校验 QMetaMethod::fromSignal 结果
     *   ❼ delayMs<0 钳制为模式默认值并 qWarning
     *   ❻ 保存 per-id destroyedConn 句柄 → 不再重复泄漏
     */
    template<typename Sender, typename... Args>
    void aggregate(Sender* sender,
                   void (Sender::*signal)(Args...),
                   std::function<void(const QVariantList&)> handler,
                   Mode mode = Mode::Debounce,
                   int delayMs = 100)
    {
        // 空 sender / 空 handler：告警并跳过，状态不改动
        if (!sender || !handler) {
            qWarning("EventAggregator::aggregate: sender=%p or handler is null",
                     static_cast<void*>(sender));
            return;
        }

        // ====== 修复❼：delayMs 非负校验 ======
        int delay = delayMs;
        if (delay < 0) {
            const int defaultMs =
                mode == Mode::Throttle ? 1000 :
                mode == Mode::Merge    ? 200  : 100;
            qWarning("EventAggregator::aggregate: delayMs=%d < 0, clamped to %d",
                     delayMs, defaultMs);
            delay = defaultMs;
        }

        // ====== 修复❹：fromSignal 结果校验 ======
        const QMetaMethod meta = QMetaMethod::fromSignal(signal);
        if (!meta.isValid()) {
            qWarning("EventAggregator::aggregate: invalid signal for sender=%p",
                     static_cast<void*>(sender));
            return;
        }

        // 唯一 id = sender 地址(16 进制) + 归一化信号签名
        const QString id = QString("%1_%2")
            .arg(reinterpret_cast<quintptr>(sender), 0, 16)
            .arg(QString::fromUtf8(
                QMetaObject::normalizedSignature(meta.methodSignature())));

        // ====== 修复❷：重入 id 先断旧连接（先 signalConn 后 destroyedConn + timer）======
        if (m_subscriptions.contains(id)) {
            Subscription& oldSub = m_subscriptions[id];
            // 注意：QMetaObject::Connection 成员 disconnect() 是 Qt 6 API；
            // Qt 5 必须使用静态函数 QObject::disconnect(conn)。
            if (static_cast<bool>(oldSub.signalConn))    QObject::disconnect(oldSub.signalConn);
            if (static_cast<bool>(oldSub.destroyedConn)) QObject::disconnect(oldSub.destroyedConn);
            if (oldSub.timer) {
                oldSub.timer->stop();
                oldSub.timer->deleteLater();
            }
            auto mIt = m_senderMap.find(sender);
            if (mIt != m_senderMap.end()) {
                mIt->removeOne(id);
                if (mIt->isEmpty()) m_senderMap.erase(mIt);
            }
            m_subscriptions.remove(id);
        }

        // ====== 新建 Subscription 并写入 ======
        Subscription sub;
        sub.receiver = sender;
        sub.timer    = new QTimer(this);
        sub.timer->setSingleShot(true);
        sub.mode     = mode;
        sub.delay    = delay;
        sub.handler  = std::move(handler);
        sub.hasPending = false;

        // ====== 修复 Timer 连接稳定性：Subscription 创建时一次性 connect timer→execute ======
        // 旧实现每次 trigger 时断所有 timeout 连接，会造成 Merge/Throttle 模式
        // timer active 期间后续 trigger 把已建立的 execute lambda 断掉，造成 execute 永不调用。
        // 新方案：lambda 捕获 QString id（按值），id 在生命周期内稳定，一次 connect 终身复用。
        // Debounce：每次 trigger 只需 stop() + start() 重启 timer（timeout → execute 路径不变）
        // Throttle/Merge：首次 trigger start(窗口)，到期 timeout → execute。
        QObject::connect(sub.timer, &QTimer::timeout, this, [this, id]() {
            this->execute(id);
        });

        // ====== 修复❻：per-id destroyed 连接句柄 ======
        sub.destroyedConn = connect(
            sender, &QObject::destroyed, this,
            [this, id](QObject*) { this->removeSubscriptionById(id); },
            Qt::DirectConnection);

        // ====== 修复❶：维护反向映射 sender -> id list ======
        m_senderMap[sender].append(id);
        m_subscriptions[id] = std::move(sub);
        Subscription& newSub = m_subscriptions[id];

        // 参数包装 lambda
        // 注意1：必须显式使用形参类型列表（不能写 auto&&...），
        //        Qt 5 PMF 连接需要在 lambda operator() 形参上解析出具体类型，否则连接会失败。
        // 注意2：必须使用 const Args&...（const 左值引用），不能写 Args&&...
        //        aggregate 里 Args... 来自成员函数指针签名，并非调用点推导：
        //          - signal(int)         → Args=int  → Args&&=int&&，
        //            无法绑定 Qt 槽机制传来的左值 int&（原版 bug！）
        //          - signal(const QString&) → Args=const QString& → 引用折叠刚好能通过
        //        使用 const Args&... 后两种情况均能正确绑定左值。
        auto wrapper = [this, id](const Args&... args) {
            QVariantList argList{ QVariant::fromValue(args)... };
            this->trigger(id, argList);
        };

        // ====== 修复❸：connect context=this → wrapper 始终在单例线程执行 ======
        newSub.signalConn = QObject::connect(
            sender, signal, this, std::move(wrapper),
            Qt::UniqueConnection);
    }

    // 对外非模板声明（实现在 .cpp）
    /** @brief 清除指定 sender 的全部订阅 */
    void clear(QObject* sender = nullptr);
    /** @brief 全局清除全部订阅 */
    void clearAll();

private:
    explicit EventAggregator(QObject* parent = nullptr) : QObject(parent) {}
    ~EventAggregator() override = default;
    Q_DISABLE_COPY(EventAggregator)

    /** @brief 单条订阅的完整结构 */
    struct Subscription {
        QPointer<QObject> receiver;          ///< 弱引用：sender 析构后自动为 nullptr
        Mode             mode     = Mode::Debounce;
        int              delay    = 100;
        QTimer*          timer    = nullptr; ///< 由 this 父子关系 + deleteLater 管生命周期
        std::function<void(const QVariantList&)> handler;
        QVariantList     accumulatedArgs;    ///< Merge 累积：嵌套结构（每项 QVariantList）
        QVariantList     lastArgs;           ///< Debounce/Throttle 最后一次参数（扁平）
        bool             hasPending = false; ///< 是否有待执行参数
        QMetaObject::Connection signalConn;      ///< 修复❷❺：精确断开信号包装连接
        QMetaObject::Connection destroyedConn;   ///< 修复❺❻：精确断开 destroyed 连接
    };

    QHash<QString, Subscription> m_subscriptions;  ///< id -> Subscription 主表
    QHash<QObject*, QStringList> m_senderMap;       ///< 修复❶：sender -> id[] 反查

    /** @brief 按 id 安全移除单条订阅（被 destroyed/clear/clearAll 共用）*/
    void removeSubscriptionById(const QString& id);

    /** @brief 信号触发时：根据 mode 更新状态并安排 timer */
    void trigger(const QString& id, const QVariantList& args);
    /** @brief timer 到期：调用 handler */
    void execute(const QString& id);
};

// ========== 宏包装 ==========
#define SQZ_DEBOUNCE(s, sig, h, ms)  ::Sqz::EventAggregator::Debounce(s, sig, h, ms)
#define SQZ_THROTTLE(s, sig, h, ms)  ::Sqz::EventAggregator::Throttle(s, sig, h, ms)
#define SQZ_MERGE(s,    sig, h, ms)  ::Sqz::EventAggregator::Merge(s,    sig, h, ms)
#define SQZ_EVT_CLEAR(s)             ::Sqz::EventAggregator::Clear(s)
#define SQZ_EVT_CLEAR_ALL()          ::Sqz::EventAggregator::ClearAll()

} // namespace Sqz
#endif // EVENTAGGREGATOR_H
