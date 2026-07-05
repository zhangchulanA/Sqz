#ifndef EVENTAGGREGATOR_H
#define EVENTAGGREGATOR_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QPointer>
#include <QVariantList>
#include <functional>
#include <QMetaMethod>
#include <type_traits>
#include <QDebug>

/**
 * @brief 事件聚合器（全局单例）
 *
 * 三种模式：
 * - Debounce：防抖，停止触发后延迟执行（只执行最后一次）
 * - Throttle：节流，固定频率执行
 * - Merge：合并，多个信号合并为一次执行（累积所有参数）
 *
 * 使用示例：
 * @code
 *   // 搜索框防抖
 *   SQZ_DEBOUNCE(searchEdit, &QLineEdit::textChanged,
 *       [](const QVariantList& args) { doSearch(args.first().toString()); }, 300);
 *
 *   // 进度节流
 *   SQZ_THROTTLE(progressObj, &Progress::valueChanged,
 *       [](const QVariantList& args) { updateUI(args.first().toInt()); }, 1000);
 *
 *   // 表单合并保存
 *   SQZ_MERGE(nameEdit, &QLineEdit::textChanged, [](...) { saveForm(); }, 200);
 *   SQZ_MERGE(ageSpin, &QSpinBox::valueChanged, [](...) { saveForm(); }, 200);
 * @endcode
 */
class EventAggregator : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Debounce, Throttle, Merge };

    /** @brief 获取全局单例 */
    static EventAggregator* instance() {
        static EventAggregator s_instance;
        return &s_instance;
    }

    // ==================== 静态便捷接口 ====================

    template<typename Sender, typename... Args>
    static void Debounce(Sender* sender,
                         void (Sender::*signal)(Args...),
                         std::function<void(const QVariantList&)> handler,
                         int delayMs = 100) {
        instance()->aggregate(sender, signal, handler, Mode::Debounce, delayMs);
    }

    template<typename Sender, typename... Args>
    static void Throttle(Sender* sender,
                         void (Sender::*signal)(Args...),
                         std::function<void(const QVariantList&)> handler,
                         int delayMs = 1000) {
        instance()->aggregate(sender, signal, handler, Mode::Throttle, delayMs);
    }

    template<typename Sender, typename... Args>
    static void Merge(Sender* sender,
                      void (Sender::*signal)(Args...),
                      std::function<void(const QVariantList&)> handler,
                      int delayMs = 200) {
        instance()->aggregate(sender, signal, handler, Mode::Merge, delayMs);
    }

    static void Clear(QObject* sender = nullptr) {
        instance()->clear(sender);
    }

    static void ClearAll() {
        instance()->clearAll();
    }

    // ==================== 实例方法 ====================

    template<typename Sender, typename... Args>
    void aggregate(Sender* sender,
                   void (Sender::*signal)(Args...),
                   std::function<void(const QVariantList&)> handler,
                   Mode mode = Mode::Debounce,
                   int delayMs = 100)
    {
        if (!sender || !handler) return;

        // ★★★★★ 修复：使用 QString 拼接，不进行指针转换 ★★★★★
        // 用 sender 地址 + 信号名称 作为唯一 ID
        QString id = QString("%1_%2")
            .arg((quintptr)sender, 0, 16)
            .arg(QString::fromUtf8(QMetaObject::normalizedSignature(
                QMetaMethod::fromSignal(signal).methodSignature())));

        if (m_subscriptions.contains(id)) {
            auto& sub = m_subscriptions[id];
            if (sub.timer) {
                sub.timer->stop();
                sub.timer->deleteLater();
            }
        }

        Subscription sub;
        sub.receiver = sender;
        sub.timer = new QTimer(this);
        sub.timer->setSingleShot(true);
        sub.mode = mode;
        sub.delay = delayMs;
        sub.handler = handler;
        sub.hasPending = false;

        connect(sender, &QObject::destroyed, this, [this, id]() {
            m_subscriptions.remove(id);
        });

        m_subscriptions[id] = sub;

        // 用 lambda 包装信号参数
        auto wrapper = [this, id](Args... args) {
            QVariantList argList{ QVariant::fromValue(args)... };
            this->trigger(id, argList);
        };

        QObject::connect(sender, signal, sender, wrapper, Qt::UniqueConnection);
    }

    void clear(QObject* sender = nullptr);
    void clearAll();

private:
    explicit EventAggregator(QObject *parent = nullptr) : QObject(parent) {}
    ~EventAggregator() = default;
    Q_DISABLE_COPY(EventAggregator)

    struct Subscription {
        QPointer<QObject> receiver;
        Mode mode = Mode::Debounce;
        int delay = 100;
        QTimer* timer = nullptr;
        std::function<void(const QVariantList&)> handler;
        QVariantList accumulatedArgs;
        QVariantList lastArgs;
        bool hasPending = false;
    };

    QHash<QString, Subscription> m_subscriptions;
    QHash<QObject*, QStringList> m_senderMap;

    void trigger(const QString& id, const QVariantList& args);
    void execute(const QString& id);
};

// ==================== 宏包装 ====================

#define SQZ_DEBOUNCE(sender, signal, handler, delayMs) \
    EventAggregator::Debounce(sender, signal, handler, delayMs)

#define SQZ_THROTTLE(sender, signal, handler, delayMs) \
    EventAggregator::Throttle(sender, signal, handler, delayMs)

#define SQZ_MERGE(sender, signal, handler, delayMs) \
    EventAggregator::Merge(sender, signal, handler, delayMs)

#define SQZ_EVT_CLEAR(sender) \
    EventAggregator::Clear(sender)

#define SQZ_EVT_CLEAR_ALL() \
    EventAggregator::ClearAll()

#endif // EVENTAGGREGATOR_H
