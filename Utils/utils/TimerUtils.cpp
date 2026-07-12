#include "TimerUtils.h"
#include <algorithm>

// ═══════════════════════════════════════════════════════════════
// 构造与析构
// ═══════════════════════════════════════════════════════════════

namespace Sqz {
TimerUtils::TimerUtils(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_resumeTimer(new QTimer(this))        // 新增：用于恢复的专用定时器
    , m_remainUpdateTimer(new QTimer(this))
{
    // 主定时器超时连接
    connect(m_timer, &QTimer::timeout, this, &TimerUtils::onInternalTimeout);

    // 恢复定时器：单次触发，超时后执行 onInternalTimeout
    m_resumeTimer->setSingleShot(true);
    connect(m_resumeTimer, &QTimer::timeout, this, &TimerUtils::onInternalTimeout);

    // 剩余时间更新定时器：每 100ms 发射一次 remainingTimeChanged 信号
    m_remainUpdateTimer->setInterval(100);
    connect(m_remainUpdateTimer, &QTimer::timeout, this, [this]() {
        if (isActive()) {
            emit remainingTimeChanged(remainingTime());
        }
    });
}

TimerUtils::~TimerUtils()
{
    stop();
    // QTimer 是 QObject 子对象，会自动清理，无需额外 delete
}

// ═══════════════════════════════════════════════════════════════
// 工厂方法
// ═══════════════════════════════════════════════════════════════

TimerUtils* TimerUtils::create(QObject *parent)
{
    return new TimerUtils(parent);
}

TimerUtils* TimerUtils::singleShot(int msec, std::function<void()> callback,
                                    QObject *parent, bool autoDelete)
{
    auto *timer = new TimerUtils(parent);
    timer->setInterval(msec)
          .setSingleShot(true)
          .setAutoDelete(autoDelete)
          .onTimeout(std::move(callback));
    timer->start();
    return timer;
}

TimerUtils* TimerUtils::periodic(int msec, std::function<void()> callback,
                                  QObject *parent)
{
    auto *timer = new TimerUtils(parent);
    timer->setInterval(msec)
          .setSingleShot(false)
          .onTimeout(std::move(callback));
    timer->start();
    return timer;
}

// ═══════════════════════════════════════════════════════════════
// 链式配置
// ═══════════════════════════════════════════════════════════════

TimerUtils& TimerUtils::setInterval(int msec)
{
    if (msec <= 0) msec = 1;
    if (m_interval != msec) {
        m_interval = msec;
        // 如果定时器正在运行且未暂停，同步更新底层 QTimer 间隔
        if (m_timer->isActive() && !m_paused) {
            m_timer->setInterval(msec);
        }
        emit intervalChanged(m_interval);
    }
    return *this;
}

TimerUtils& TimerUtils::setSingleShot(bool single)
{
    if (m_singleShot != single) {
        m_singleShot = single;
        m_timer->setSingleShot(single);
        emit singleShotChanged(m_singleShot);
    }
    return *this;
}

TimerUtils& TimerUtils::setAutoDelete(bool autoDelete)
{
    m_autoDelete = autoDelete;
    return *this;
}

TimerUtils& TimerUtils::onTimeout(std::function<void()> callback)
{
    m_callback = std::move(callback);
    return *this;
}

TimerUtils& TimerUtils::setTimerType(Qt::TimerType type)
{
    m_timer->setTimerType(type);
    return *this;
}

TimerUtils& TimerUtils::setRepeatCount(int count)
{
    m_repeatCount = (count <= 0) ? -1 : count;
    return *this;
}

TimerUtils& TimerUtils::setImmediateExecution(bool immediate)
{
    m_immediateExecution = immediate;
    return *this;
}

TimerUtils& TimerUtils::setFrequencyWindowSize(int size)
{
    m_freqWindowSize = std::max(2, size);
    while (static_cast<int>(m_timestamps.size()) > m_freqWindowSize) {
        m_timestamps.pop_front();
    }
    return *this;
}

// ═══════════════════════════════════════════════════════════════
// 控制接口
// ═══════════════════════════════════════════════════════════════

void TimerUtils::start()
{
    // 重置运行时状态
    m_paused = false;
    m_executionCount = 0;
    m_timestamps.clear();
    cancelResumeTimer();   // 确保没有残留的恢复定时器

    // 处理立即执行
    bool immediateExecuted = false;
    if (m_immediateExecution && m_callback) {
        // 记录时间戳以支持频率统计（立即执行也算一次触发）
        m_timestamps.push_back(std::chrono::steady_clock::now());
        // 裁剪窗口
        while (static_cast<int>(m_timestamps.size()) > m_freqWindowSize) {
            m_timestamps.pop_front();
        }
        // 执行回调（注意保护重入）
        m_inTimeout = true;
        m_callback();
        m_inTimeout = false;
        emit timeout();
        m_executionCount++;
        emit executionCountChanged(m_executionCount);
        immediateExecuted = true;
    }

    // 如果单次模式且立即执行已经发生过，则不再启动定时器，避免二次触发
    if (m_singleShot && immediateExecuted) {
        // 单次 + immediate: 已经执行了回调，直接结束，不再启动定时器
        // 根据 autoDelete 决定是否自动删除
        if (m_autoDelete) {
            deleteLater();
        }
        return;
    }

    // 启动剩余时间更新定时器
    m_remainUpdateTimer->start();
    // 启动底层 QTimer
    m_timer->start(m_interval);
    emit activeChanged(true);
}

void TimerUtils::stop()
{
    m_paused = false;
    m_timer->stop();
    m_remainUpdateTimer->stop();
    cancelResumeTimer();          // 取消恢复定时器
    m_timestamps.clear();
    emit activeChanged(false);
    // 注意：剩余时间信号不发射，因为定时器已停用，remainingTime() 返回 -1，UI 应自行处理
}

void TimerUtils::reset()
{
    stop();
    start();
}

void TimerUtils::pause()
{
    if (!m_timer->isActive() || m_paused) return;

    // 记录剩余时间
    m_remainingOnPause = m_timer->remainingTime();
    if (m_remainingOnPause < 0) m_remainingOnPause = 0;

    m_timer->stop();
    m_remainUpdateTimer->stop();
    cancelResumeTimer();          // 如果有正在等待的恢复定时器，取消它
    m_paused = true;
    emit remainingTimeChanged(m_remainingOnPause);   // 发射当前剩余时间
}

void TimerUtils::resume()
{
    if (!m_paused) return;

    m_paused = false;
    m_remainUpdateTimer->start();
    startResumeTimer();           // 安全的恢复机制
    emit remainingTimeChanged(remainingTime());   // 立即更新一次剩余时间
}

void TimerUtils::triggerNow()
{
    if (m_callback && !m_inTimeout) {   // 防止递归
        m_inTimeout = true;
        m_callback();
        m_inTimeout = false;
        emit timeout();
    }
}

// ═══════════════════════════════════════════════════════════════
// 查询接口
// ═══════════════════════════════════════════════════════════════

bool TimerUtils::isActive() const
{
    return m_timer->isActive() || m_paused;
}

bool TimerUtils::isSingleShot() const
{
    return m_singleShot;
}

int TimerUtils::interval() const
{
    return m_interval;
}

int TimerUtils::remainingTime() const
{
    if (m_paused) {
        return m_remainingOnPause;
    }
    return m_timer->isActive() ? m_timer->remainingTime() : -1;
}

int TimerUtils::executionCount() const
{
    return m_executionCount;
}

double TimerUtils::averageFrequency() const
{
    if (m_timestamps.size() < 2) return 0.0;
    auto totalDuration = m_timestamps.back() - m_timestamps.front();
    double totalSeconds = std::chrono::duration<double>(totalDuration).count();
    return (totalSeconds > 0.0) ? (m_timestamps.size() - 1) / totalSeconds : 0.0;
}

Qt::TimerType TimerUtils::timerType() const
{
    return m_timer->timerType();
}

// ═══════════════════════════════════════════════════════════════
// 操作符重载
// ═══════════════════════════════════════════════════════════════

TimerUtils& TimerUtils::operator+=(int msec)
{
    return setInterval(m_interval + msec);
}

TimerUtils& TimerUtils::operator-=(int msec)
{
    return setInterval(std::max(1, m_interval - msec));
}

// ═══════════════════════════════════════════════════════════════
// 内部实现
// ═══════════════════════════════════════════════════════════════

void TimerUtils::onInternalTimeout()
{
    // 防止回调中再次进入（如回调内调用 pause/resume/stop 等）
    if (m_inTimeout) return;

    // ── 更新时间戳（频率统计） ──
    m_timestamps.push_back(std::chrono::steady_clock::now());
    while (static_cast<int>(m_timestamps.size()) > m_freqWindowSize) {
        m_timestamps.pop_front();
    }

    // ── 执行计数 ──
    m_executionCount++;
    emit executionCountChanged(m_executionCount);

    // ── 发射频率变化信号 ──
    if (m_timestamps.size() >= 2) {
        emit averageFrequencyChanged(averageFrequency());
    }

    // ── 执行用户回调 ──
    if (m_callback) {
        m_inTimeout = true;
        m_callback();
        m_inTimeout = false;
    }

    // ── 发射超时信号 ──
    emit timeout();

    // ── 重复次数限制检查 ──
    if (m_repeatCount > 0 && m_executionCount >= m_repeatCount) {
        if (m_autoDelete) {
            deleteLater();
        } else {
            stop();   // 自动停止
        }
        return;
    }

    // ── 单次模式自动删除 ──
    if (m_singleShot && m_autoDelete) {
        deleteLater();
    }
}

void TimerUtils::startResumeTimer()
{
    // 取消之前可能未执行的恢复定时器
    cancelResumeTimer();

    // 剩余时间可能是 0 或负数？若为 0 则立即触发
    if (m_remainingOnPause <= 0) {
        onInternalTimeout();
        // 如果是周期模式，需要重新启动主定时器
        if (!m_paused && !m_singleShot) {
            m_timer->start(m_interval);
        }
    } else {
        m_resumeTimer->start(m_remainingOnPause);
    }
}

void TimerUtils::cancelResumeTimer()
{
    if (m_resumeTimer->isActive()) {
        m_resumeTimer->stop();
    }
}
}
