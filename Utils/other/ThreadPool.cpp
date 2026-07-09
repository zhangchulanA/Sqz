#include "ThreadPool.h"
#include <QThread>
#include <QtConcurrent>

namespace Sqz::Utils {
// 全局单例（保留但不再用于实例获取，仅用于兼容可能的旧代码）
ThreadPool* ThreadPool::m_instance = nullptr;

/**
 * @brief 获取全局单例（C++11 Magic Static，线程安全）
 * @return ThreadPool* 单例指针
 */
ThreadPool* ThreadPool::instance()
{
    static ThreadPool inst;
    return &inst;
}

/**
 * @brief 构造函数（私有）
 * @param parent 父对象，默认为 nullptr
 * @details 初始化 Qt 全局线程池，设置最大线程数为 CPU 核心数×2（适合 IO 密集型任务），
 *          空闲线程存活时间 30 秒。
 */
ThreadPool::ThreadPool(QObject *parent)
    : QObject(parent)
    , m_taskIdGenerator(1)   // 任务 ID 从 1 开始，0 作为无效 ID
{
    m_threadPool = QThreadPool::globalInstance();
    int cpuCoreCount = QThread::idealThreadCount();
    m_threadPool->setMaxThreadCount(cpuCoreCount * 2);
    m_threadPool->setExpiryTimeout(30000);  // 30 秒空闲回收
}

/**
 * @brief 析构函数：等待所有任务完成，并清理所有定时器和监听器
 */
ThreadPool::~ThreadPool()
{
    waitForAllDone();  // 等待所有已提交任务完成

    QMutexLocker lock(&m_mutex);

    // 清理周期任务定时器
    for (QTimer* timer : m_periodicTimers.values()) {
        timer->stop();
        timer->deleteLater();
    }
    m_periodicTimers.clear();

    // 清理延迟任务定时器
    for (QTimer* timer : m_delayTimers.values()) {
        timer->stop();
        timer->deleteLater();
    }
    m_delayTimers.clear();

    // 清理超时定时器
    for (QTimer* timer : m_timeoutTimers.values()) {
        timer->stop();
        timer->deleteLater();
    }
    m_timeoutTimers.clear();

    // 清理任务监听器
    for (QFutureWatcher<void>* watcher : m_taskWatchers.values()) {
        watcher->deleteLater();
    }
    m_taskWatchers.clear();
}

/**
 * @brief 生成原子递增的任务 ID
 * @return 新 ID
 */
quint64 ThreadPool::generateId()
{
    return m_taskIdGenerator.fetchAndAddRelaxed(1);
}

/**
 * @brief 清理指定任务 ID 相关的所有资源
 * @param id 任务 ID
 * @details 从 m_taskWatchers 中移除并删除 QFutureWatcher，
 *          当所有任务监听器为空时发射 allTasksFinished 信号。
 */
void ThreadPool::cleanTask(quint64 id)
{
    QMutexLocker lock(&m_mutex);
    if (m_taskWatchers.contains(id)) {
        QFutureWatcher<void>* watcher = m_taskWatchers.take(id);
        watcher->deleteLater();
    }
    if (m_taskWatchers.isEmpty()) {
        emit allTasksFinished();
    }
}

/**
 * @brief 取消指定任务
 * @param taskId 任务 ID
 * @return true 取消成功，false 任务不存在或已完成
 * @details 依次检查：延迟定时器 -> 周期定时器 -> 已提交到线程池的任务。
 *          对于已提交的任务，会先释放锁再调用 waitForFinished 以避免死锁。
 */
bool ThreadPool::cancelTask(quint64 taskId)
{
    QMutexLocker lock(&m_mutex);

    // 1. 检查是否为延迟任务（尚未提交）
    if (m_delayTimers.contains(taskId)) {
        QTimer* timer = m_delayTimers.take(taskId);
        timer->stop();
        timer->deleteLater();
        return true;
    }

    // 2. 检查是否为周期任务
    if (m_periodicTimers.contains(taskId)) {
        QTimer* timer = m_periodicTimers.take(taskId);
        timer->stop();
        timer->deleteLater();
        return true;
    }

    // 3. 检查是否为已提交的任务
    if (!m_taskWatchers.contains(taskId)) {
        return false;
    }

    QFutureWatcher<void>* watcher = m_taskWatchers[taskId];
    // 在等待前释放锁，避免死锁（waitForFinished 可能阻塞并处理事件）
    lock.unlock();
    watcher->cancel();
    watcher->waitForFinished();
    lock.relock();

    cleanTask(taskId);
    return true;
}

/**
 * @brief 阻塞等待指定任务完成
 * @param taskId 任务 ID
 * @details 如果任务 ID 不存在，立即返回。等待期间释放互斥锁，避免阻塞其他操作。
 */
void ThreadPool::waitForTaskFinished(quint64 taskId)
{
    QMutexLocker lock(&m_mutex);
    if (m_taskWatchers.contains(taskId)) {
        QFutureWatcher<void>* watcher = m_taskWatchers[taskId];
        lock.unlock();
        watcher->waitForFinished();
        lock.relock();
    }
}

/**
 * @brief 阻塞等待所有已提交的任务完成（包括队列中的和正在执行的）
 */
void ThreadPool::waitForAllDone()
{
    m_threadPool->waitForDone();
}

/**
 * @brief 清空尚未开始的任务队列（谨慎使用）
 * @attention 只能清空通过 QtConcurrent::run 提交且尚未开始的任务，由于我们使用 QFutureWatcher，清空可能不完全可靠。
 */
void ThreadPool::clearPending()
{
    m_threadPool->clear();
}

/**
 * @brief 取消周期任务
 * @param taskId 周期任务 ID
 * @details 停止关联的 QTimer 并销毁。
 */
void ThreadPool::cancelPeriodic(quint64 taskId)
{
    QMutexLocker lock(&m_mutex);
    if (m_periodicTimers.contains(taskId)) {
        QTimer* timer = m_periodicTimers.take(taskId);
        timer->stop();
        timer->deleteLater();
    }
}

/**
 * @brief 设置最大并发线程数
 * @param count 新线程数
 */
void ThreadPool::setMaxThreadCount(int count)
{
    QMutexLocker lock(&m_mutex);
    m_threadPool->setMaxThreadCount(count);
}

/**
 * @brief 获取当前最大并发线程数
 * @return int
 */
int ThreadPool::maxThreadCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_threadPool->maxThreadCount();
}

/**
 * @brief 暂停线程池：拒绝新任务提交，已提交但未开始的任务保持等待，正在执行的任务继续
 * @details 实现方式：设置 m_paused = true，并在 startImpl 中检查该标志。
 *          由于底层 QThreadPool 无法真正暂停队列中的任务，这些任务仍然会被调度，
 *          但新任务不会再被接受。这是一种折衷，避免任务丢失。
 */
void ThreadPool::pause()
{
    QMutexLocker lock(&m_mutex);
    if (m_paused) return;
    m_paused = true;
    // 注意：不清空队列，因为 Qt 线程池不支持暂停未开始任务。
    // 已提交到线程池的任务会继续执行完成。新任务将被拒绝提交。
    // 这样至少不会丢失任务。
}

/**
 * @brief 恢复线程池：允许新任务提交
 */
void ThreadPool::resume()
{
    QMutexLocker lock(&m_mutex);
    if (!m_paused) return;
    m_paused = false;
    // 恢复后，新任务可以正常提交
}

/**
 * @brief 查询暂停状态
 * @return bool
 */
bool ThreadPool::isPaused() const
{
    QMutexLocker lock(&m_mutex);
    return m_paused;
}
}
