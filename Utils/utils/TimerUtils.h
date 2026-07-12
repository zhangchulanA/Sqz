#ifndef TIMERUTILS_H
#define TIMERUTILS_H

#include <QObject>
#include <QTimer>
#include <functional>
#include <chrono>
#include <deque>
#include "SqzGlobal.h"
/**
 * @brief 定时器工具类（修复版）
 *
 * TimerUtils 是一个基于 QTimer 封装的高级定时器工具类，提供以下特性：
 *
 * 核心功能：
 * - 链式调用配置，所有 setter 返回引用（&）以支持连续调用
 * - 支持 std::chrono 时长字面量（如 500ms、2s、1min）设置间隔
 * - 单次/周期性定时、暂停/恢复、重置、手动触发
 * - 重复次数限制（执行指定次数后自动停止）
 * - 可指定定时器精度：PreciseTimer / CoarseTimer / VeryCoarseTimer
 * - 启动时立即执行选项（immediate execution）
 * - 运行频率统计（基于滑动窗口的平均频率计算）
 * - 单次定时器自动删除机制（避免内存泄漏）
 * - 丰富的 Qt 信号（活跃状态、间隔、剩余时间、执行次数、频率变化）
 * - 操作符重载（+=/-= 调整间隔，bool 转换判断活跃状态）
 *
 * 创建方式（工厂方法）：
 * - TimerUtils::create(parent)       → 创建空定时器，需手动配置并 start()
 * - TimerUtils::singleShot(...)      → 创建单次定时器，立即启动，默认自动删除
 * - TimerUtils::periodic(...)        → 创建周期定时器，立即启动，持续执行
 *
 * 线程安全：
 * - 必须在拥有事件循环的线程（如主线程）中使用
 * - 不可跨线程操作同一个 TimerUtils 对象
 *
 * 生命周期：
 * - 继承 QObject，随父对象销毁自动清理
 * - 单次模式可通过 setAutoDelete(true) 在执行后自动 deleteLater()
 *
 * 依赖：
 * - Qt 5.12.9+ (Core 模块)
 * - C++14 或更高版本
 *
 * @note 使用前引入 using namespace std::chrono_literals; 时间可以写成 2s
 */

namespace Sqz {
class SQZ_FRAMEWORK_API TimerUtils : public QObject
{
    Q_OBJECT

    // Qt 属性（可在 QML/设计器中使用）
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(int remainingTime READ remainingTime NOTIFY remainingTimeChanged)
    Q_PROPERTY(bool singleShot READ isSingleShot NOTIFY singleShotChanged)
    Q_PROPERTY(int executionCount READ executionCount NOTIFY executionCountChanged)
    Q_PROPERTY(double averageFrequency READ averageFrequency NOTIFY averageFrequencyChanged)

public:
    // ─────────────────── 工厂方法 ───────────────────

    /**
     * @brief 创建一个待配置的定时器（未启动状态）
     */
    static TimerUtils* create(QObject *parent = nullptr);

    /**
     * @brief 创建并立即启动一个单次定时器（毫秒版本）
     * @param msec 延迟时间（毫秒），必须 > 0
     * @param callback 超时回调函数
     * @param parent 父对象
     * @param autoDelete 执行后是否自动 deleteLater()，默认 true
     * @note 如果 immediateExecution 被设置为 true，则行为变化：立即执行且不再启动定时器（即直接同步执行一次）
     */
    static TimerUtils* singleShot(int msec, std::function<void()> callback,
                                  QObject *parent = nullptr, bool autoDelete = true);

    /**
     * @brief 创建并立即启动一个单次定时器（std::chrono 版本）
     */
    template<typename Rep, typename Period>
    static TimerUtils* singleShot(std::chrono::duration<Rep, Period> interval,
                                  std::function<void()> callback,
                                  QObject *parent = nullptr, bool autoDelete = true);

    /**
     * @brief 创建并立即启动一个周期性定时器（毫秒版本）
     */
    static TimerUtils* periodic(int msec, std::function<void()> callback,
                                QObject *parent = nullptr);

    /**
     * @brief 创建并立即启动一个周期性定时器（std::chrono 版本）
     */
    template<typename Rep, typename Period>
    static TimerUtils* periodic(std::chrono::duration<Rep, Period> interval,
                                std::function<void()> callback,
                                QObject *parent = nullptr);

    // ─────────────────── 构造与析构 ───────────────────

    ~TimerUtils() override;

    TimerUtils(const TimerUtils&) = delete;
    TimerUtils& operator=(const TimerUtils&) = delete;

    // ─────────────────── 链式配置接口（全部返回引用 &） ───────────────────

    TimerUtils& setInterval(int msec);
    template<typename Rep, typename Period>
    TimerUtils& setInterval(std::chrono::duration<Rep, Period> interval);
    TimerUtils& setSingleShot(bool single);
    TimerUtils& setAutoDelete(bool autoDelete);
    TimerUtils& onTimeout(std::function<void()> callback);
    TimerUtils& setTimerType(Qt::TimerType type);
    TimerUtils& setRepeatCount(int count);      // count <= 0 视为无限（-1）
    TimerUtils& setImmediateExecution(bool immediate);
    TimerUtils& setFrequencyWindowSize(int size);   // 至少 2

    // ─────────────────── 控制接口 ───────────────────

    void start();
    void stop();
    void reset();
    void pause();
    void resume();
    void triggerNow();

    // ─────────────────── 查询接口 ───────────────────

    bool isActive() const;
    bool isSingleShot() const;
    int interval() const;
    int remainingTime() const;
    int executionCount() const;
    double averageFrequency() const;
    Qt::TimerType timerType() const;

    // ─────────────────── 操作符重载 ───────────────────

    TimerUtils& operator+=(int msec);
    TimerUtils& operator-=(int msec);
    explicit operator bool() const { return isActive(); }

signals:
    void timeout();
    void activeChanged(bool active);
    void intervalChanged(int interval);
    void remainingTimeChanged(int remaining);
    void singleShotChanged(bool single);
    void executionCountChanged(int count);
    void averageFrequencyChanged(double freq);

private:
    explicit TimerUtils(QObject *parent = nullptr);

    void onInternalTimeout();
    void startResumeTimer();       // 安全的恢复实现
    void cancelResumeTimer();      // 取消等待中的恢复定时器

    // 成员变量
    QTimer *m_timer = nullptr;
    QTimer *m_resumeTimer = nullptr;   // 用于暂停恢复的单次定时器（替代原先的 anonymous singleShot）
    std::function<void()> m_callback;
    int m_interval = 0;
    bool m_singleShot = false;
    bool m_autoDelete = false;
    bool m_paused = false;
    bool m_immediateExecution = false;
    int m_repeatCount = -1;        // -1 表示无限
    int m_remainingOnPause = 0;
    int m_executionCount = 0;
    bool m_inTimeout = false;      // 防止回调中重入的保护标志

    int m_freqWindowSize = 10;
    std::deque<std::chrono::steady_clock::time_point> m_timestamps;

    QTimer *m_remainUpdateTimer = nullptr;   // 剩余时间更新定时器
};

// ─────────────────── 模板实现（必须放在头文件中） ───────────────────

template<typename Rep, typename Period>
TimerUtils* TimerUtils::singleShot(std::chrono::duration<Rep, Period> interval,
                                    std::function<void()> callback,
                                    QObject *parent, bool autoDelete)
{
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
    return singleShot(static_cast<int>(msec), std::move(callback), parent, autoDelete);
}

template<typename Rep, typename Period>
TimerUtils* TimerUtils::periodic(std::chrono::duration<Rep, Period> interval,
                                  std::function<void()> callback,
                                  QObject *parent)
{
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
    return periodic(static_cast<int>(msec), std::move(callback), parent);
}

template<typename Rep, typename Period>
TimerUtils& TimerUtils::setInterval(std::chrono::duration<Rep, Period> interval)
{
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
    return setInterval(static_cast<int>(msec));
}
}
#endif // TIMERUTILS_H
