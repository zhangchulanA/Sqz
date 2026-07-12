#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <functional>
#include <QAtomicInteger>
#include <QTimer>
#include <QMap>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QDebug>
#include "SqzGlobal.h"
/**
 * @brief 工业级全局线程池（单例模式）
 * @details 该类基于 Qt 的 QThreadPool 和 QtConcurrent 封装，旨在解决 Qt 程序中耗时操作阻塞主线程导致的界面卡顿问题。
 *          所有耗时的操作（如文件读写、网络请求、数据库访问、图片处理、复杂计算等）都应通过本线程池异步执行。
 *
 * @section 主要功能
 * - 基础异步任务：start()
 * - 带完成回调的任务（回调在主线程执行）：startWithCallback()
 * - 带返回值的任务（通过回调返回）：startWithResult()
 * - 带进度更新的任务（进度回调自动转到主线程）：startWithProgress()
 * - 带超时控制的任务：startWithTimeout()
 * - 延迟执行的任务：startDelayed()
 * - 周期循环执行的任务：startPeriodic()
 * - 任务依赖（等待另一个任务完成）：startDependOn()
 * - 任务取消、等待、清空等控制接口
 * - 线程池配置（最大线程数、暂停/恢复）
 *
 * @section 线程安全
 * 所有公开接口均可从任意线程调用，内部使用 QMutex 保护共享数据，任务 ID 使用原子计数器生成。
 *
 * @section 使用示例
 * @code
 *   // 获取单例
 *   ThreadPool* pool = ThreadPool::instance();
 *
 *   // 1. 基础任务
 *   pool->start([](){ qDebug() << "后台任务"; });
 *
 *   // 2. 带完成回调（主线程执行）
 *   pool->startWithCallback(
 *       [](){ QThread::msleep(500); },
 *       [](){ qDebug() << "任务完成，可更新UI"; }
 *   );
 *
 *   // 3. 带返回值
 *   pool->startWithResult(
 *       []()->QString{ return "结果"; },
 *       [](QString res){ qDebug() << "返回值:" << res; }
 *   );
 *
 *   // 4. 带进度
 *   pool->startWithProgress(
 *       [](auto progress){ for(int i=0;i<=100;i+=20) progress(i); },
 *       [](int v){ ui->progressBar->setValue(v); },
 *       [](){ qDebug() << "完成"; }
 *   );
 *
 *   // 5. 超时控制（2秒未完成则取消）
 *   pool->startWithTimeout([](){ QThread::sleep(3); }, 2000);
 *
 *   // 6. 延迟3秒执行
 *   quint64 id = pool->startDelayed([](){ qDebug() << "延迟"; }, 3000);
 *
 *   // 7. 周期任务（每秒一次）
 *   quint64 pid = pool->startPeriodic([](){ qDebug() << "心跳"; }, 1000);
 *   // 停止周期任务
 *   pool->cancelPeriodic(pid);
 *
 *   // 8. 任务依赖
 *   quint64 a = pool->start([](){ 步骤A });
 *   pool->startDependOn(a, [](){  步骤B });
 *
 *   // 9. 等待所有任务完成（程序退出前）
 *   pool->waitForAllDone();
 * @endcode
 *
 * @note 需要 C++17 支持，请在 .pro 文件中添加：CONFIG += c++17
 */


namespace Sqz {
class SQZ_FRAMEWORK_API ThreadPool : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 获取全局唯一单例实例（线程安全，使用 C++11 Magic Static）
     * @return ThreadPool* 单例指针，无需手动释放，程序退出时自动销毁
     */
    static ThreadPool* instance();

    /**
     * @brief 任务优先级枚举（当前版本预留，暂未实现调度，但保留接口扩展）
     */
    enum TaskPriority {
        LowPriority,     //!< 低优先级（预留）
        NormalPriority,  //!< 默认优先级（推荐）
        HighPriority     //!< 高优先级（预留）
    };

    // ---------------------- 基础异步任务接口 ----------------------

    /**
     * @brief 启动一个无参数、无返回值的基础异步任务
     * @tparam Func 可调用对象类型（函数、lambda、std::function 等）
     * @param task 要执行的耗时任务（在子线程中运行，不可操作 UI）
     * @param pri 任务优先级（当前未使用，保留）
     * @return quint64 任务的唯一 ID，可用于后续取消或等待；返回 0 表示提交失败（如线程池处于暂停状态）
     */
    template<typename Func>
    quint64 start(Func task, TaskPriority pri = NormalPriority)
    {
        return startImpl(std::move(task), nullptr, nullptr, pri, -1);
    }

    // ---------------------- 带完成回调的任务接口 ----------------------

    /**
     * @brief 启动异步任务，并在任务完成后执行回调（回调在主线程执行）
     * @tparam Func 任务类型
     * @tparam Callback 回调函数类型（void()）
     * @param task 后台耗时任务
     * @param cb 任务完成后的回调，在主线程执行，可安全操作 UI
     * @param pri 优先级（预留）
     * @return 任务 ID，失败返回 0
     */
    template<typename Func, typename Callback>
    quint64 startWithCallback(Func task, Callback cb, TaskPriority pri = NormalPriority)
    {
        return startImpl(std::move(task), std::move(cb), nullptr, pri, -1);
    }

    // ---------------------- 带返回值的任务接口 ----------------------

    /**
     * @brief 启动带返回值的异步任务，返回值通过回调返回（回调在主线程）
     * @tparam Func 任务类型，要求返回值类型 R
     * @tparam ResultCB 结果回调类型 void(R)
     * @param task 带返回值的任务（子线程执行）
     * @param resultCB 接收任务返回值的回调（主线程执行）
     * @param pri 优先级（预留）
     * @return 任务 ID
     */
    template<typename Func, typename ResultCB>
    quint64 startWithResult(Func task, ResultCB resultCB, TaskPriority pri = NormalPriority)
    {
        return startImpl(std::move(task), nullptr, std::move(resultCB), pri, -1);
    }

    // ---------------------- 带进度回调的任务接口 ----------------------

    /**
     * @brief 启动带进度更新的异步任务（进度回调保证在主线程执行）
     * @tparam Func 任务类型，接受一个 std::function<void(int)> 参数用于报告进度
     * @tparam ProgressCB 进度回调类型 void(int)
     * @tparam FinishCB 完成回调类型 void()
     * @param task 耗时的任务函数，内部调用传入的进度回调来报告进度（0~100）
     * @param progressCB 进度回调，在主线程执行，参数为当前进度值
     * @param finishCB 任务完成回调，在主线程执行
     * @param pri 任务优先级（预留）
     * @return quint64 任务ID
     */
    template<typename Func, typename ProgressCB, typename FinishCB>
    quint64 startWithProgress(Func task, ProgressCB progressCB, FinishCB finishCB, TaskPriority pri = NormalPriority)
    {
        // 包装进度回调：使用 QMetaObject::invokeMethod 将进度回调转到主线程
        auto safeProgressCB = [progressCB](int value) {
            QMetaObject::invokeMethod(qApp, [progressCB, value]() {
                if (progressCB) progressCB(value);
            }, Qt::QueuedConnection);
        };
        // 包装任务：将安全进度回调传给用户任务
        auto taskWrapper = [task, safeProgressCB]() {
            task(safeProgressCB);
        };
        // 使用带完成回调的接口提交任务
        return startWithCallback(std::move(taskWrapper), std::move(finishCB), pri);
    }

    // ---------------------- 带超时的任务接口 ----------------------

    /**
     * @brief 启动带超时的异步任务，超时后自动尝试取消任务
     * @tparam Func 任务类型
     * @param task 耗时任务
     * @param timeoutMs 超时时间（毫秒），超过此时间任务未完成则自动取消并发射 taskTimeout 信号
     * @param pri 优先级（预留）
     * @return 任务 ID
     * @note 取消操作会等待任务真正结束（可能已经执行完），超时信号在取消成功后发射。
     */
    template<typename Func>
    quint64 startWithTimeout(Func task, int timeoutMs, TaskPriority pri = NormalPriority)
    {
        return startImpl(std::move(task), nullptr, nullptr, pri, timeoutMs);
    }

    // ---------------------- 延迟执行任务接口 ----------------------

    /**
     * @brief 延迟指定时间后执行异步任务（不阻塞主线程）
     * @tparam Func 任务类型
     * @param task 延迟执行的任务
     * @param delayMs 延迟毫秒数
     * @param pri 优先级（预留）
     * @return 任务 ID，可用于在延迟期间取消任务（调用 cancelTask）
     */
    template<typename Func>
    quint64 startDelayed(Func task, int delayMs, TaskPriority pri = NormalPriority)
    {
        quint64 taskId = generateId();
        QMutexLocker lock(&m_mutex);
        // 创建一次性定时器，存入延迟任务映射表，便于取消
        QTimer* delayTimer = new QTimer(this);
        delayTimer->setSingleShot(true);
        m_delayTimers.insert(taskId, delayTimer);
        // 定时器触发后提交任务，并清理延迟记录
        connect(delayTimer, &QTimer::timeout, this, [=]() {
            {
                QMutexLocker lock2(&m_mutex);
                m_delayTimers.remove(taskId);
            }
            start(std::move(task), pri);
            delayTimer->deleteLater();
        });
        delayTimer->start(delayMs);
        return taskId;
    }

    // ---------------------- 周期循环任务接口 ----------------------

    /**
     * @brief 启动周期循环执行的任务（每隔固定时间执行一次）
     * @tparam Func 任务类型
     * @param task 每次执行的任务（在子线程运行）
     * @param intervalMs 执行间隔（毫秒）
     * @param pri 优先级（预留）
     * @return 周期任务 ID，用于后续调用 cancelPeriodic 停止
     */
    template<typename Func>
    quint64 startPeriodic(Func task, int intervalMs, TaskPriority pri = NormalPriority)
    {
        quint64 taskId = generateId();
        QTimer* periodicTimer = new QTimer(this);
        {
            QMutexLocker lock(&m_mutex);
            m_periodicTimers.insert(taskId, periodicTimer);
        }
        connect(periodicTimer, &QTimer::timeout, this, [=]() {
            start(task, pri);
        });
        periodicTimer->start(intervalMs);
        return taskId;
    }

    // ---------------------- 任务依赖接口（阻塞式，简单场景使用）---------------------

    /**
     * @brief 依赖某个任务完成后，再执行当前任务（阻塞式等待）
     * @tparam Func 任务类型
     * @param dependTaskId 依赖的任务 ID（必须是已经提交的任务）
     * @param task 要执行的任务
     * @param pri 优先级（预留）
     * @return 当前任务的 ID
     * @note 实现原理：在新任务中调用 waitForTaskFinished 阻塞等待依赖任务完成后再执行。
     *       这会占用一个线程池线程，适用于依赖链较短的简单场景。
     */
    template<typename Func>
    quint64 startDependOn(quint64 dependTaskId, Func task, TaskPriority pri = NormalPriority)
    {
        auto dependTaskWrapper = [=]() {
            waitForTaskFinished(dependTaskId);
            task();
        };
        return start(std::move(dependTaskWrapper), pri);
    }

    // ---------------------- 任务控制接口 ----------------------

    /**
     * @brief 取消指定 ID 的任务
     * @param taskId 任务 ID（由 start 等接口返回）
     * @return true 取消成功（任务未开始或已停止）；false 任务不存在或已完成
     * @details 对于延迟任务：直接停止定时器，任务永远不会提交。
     *          对于周期任务：停止周期定时器，不再触发新任务。
     *          对于已提交到线程池的任务：如果任务还未开始执行，会被取消；如果已经开始执行，会等待其执行完毕（无法中断）。
     */
    bool cancelTask(quint64 taskId);

    /**
     * @brief 阻塞等待指定 ID 的任务完成
     * @param taskId 任务 ID
     * @details 若任务不存在（已完成或从未存在），立即返回。
     *          通常在子线程中调用，等待某个关键任务完成后再继续。
     */
    void waitForTaskFinished(quint64 taskId);

    /**
     * @brief 阻塞等待所有已提交的任务完成（包括已在队列和正在执行的任务）
     * @details 常用于程序退出前，确保所有后台任务正常结束，避免数据丢失。
     *          注意：周期任务不会自动停止，需要先调用 cancelPeriodic。
     */
    void waitForAllDone();

    /**
     * @brief 清空线程池中尚未开始的任务队列（正在执行的任务不受影响）
     * @attention 只能清空通过 QtConcurrent::run 提交且尚未开始的任务，
     *            由于我们使用 QFutureWatcher，清空可能不完全可靠，建议谨慎使用。
     */
    void clearPending();

    /**
     * @brief 取消周期任务（停止后续的周期执行）
     * @param taskId 周期任务 ID（由 startPeriodic 返回）
     * @details 调用后会停止内部定时器并释放资源。任务若正在执行，会继续执行完毕。
     */
    void cancelPeriodic(quint64 taskId);

    // ---------------------- 线程池配置接口 ----------------------

    /**
     * @brief 设置线程池的最大并发线程数
     * @param count 最大线程数
     * @note 建议值：CPU 密集型任务 = QThread::idealThreadCount()；
     *              IO 密集型任务 = idealThreadCount() * 2
     */
    void setMaxThreadCount(int count);

    /**
     * @brief 获取当前最大并发线程数
     * @return int
     */
    int maxThreadCount() const;

    /**
     * @brief 暂停线程池：拒绝新任务的提交，已提交但未开始的任务会保持等待（不清空），已在执行的任务继续运行
     * @attention 由于底层 QThreadPool 不支持暂停已入队的任务，我们采用标志位阻止新任务提交，
     *            已提交给 Qt 线程池的任务无法暂停，但也不会丢失。
     */
    void pause();

    /**
     * @brief 恢复线程池：允许新任务提交
     */
    void resume();

    /**
     * @brief 查询当前是否处于暂停状态
     * @return true 已暂停，false 正常运行
     */
    bool isPaused() const;

signals:
    /**
     * @brief 任务开始执行时发射的信号
     * @param id 任务 ID
     * @note 该信号在子线程中发射（因为任务在子线程运行），若需在主线程处理请使用 Qt::QueuedConnection。
     */
    void taskStarted(quint64 id);

    /**
     * @brief 任务正常完成时发射的信号
     * @param id 任务 ID
     * @note 该信号在主线程发射（通过 QFutureWatcher 的 finished 信号，QueuedConnection）。
     */
    void taskFinished(quint64 id);

    /**
     * @brief 任务超时被取消时发射的信号
     * @param id 任务 ID
     * @note 在主线程发射。
     */
    void taskTimeout(quint64 id);

    /**
     * @brief 所有任务（非周期、非延迟等待中的）都已完成时发射的信号
     * @note 当最后一个活动的任务清理后发射，在主线程。
     */
    void allTasksFinished();

private:
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent QObject 父对象，通常为 nullptr
     * @details 初始化 Qt 全局线程池，设置默认最大线程数为 CPU 核心数 × 2（IO 密集型优化），
     *          空闲线程存活时间 30 秒。
     */
    explicit ThreadPool(QObject* parent = nullptr);

    /**
     * @brief 析构函数：等待所有任务完成，清理定时器和监听器
     */
    ~ThreadPool() override;

    /**
     * @brief 生成全局唯一的任务 ID（线程安全）
     * @return 新的 ID（从 1 开始递增，0 保留为无效 ID）
     */
    quint64 generateId();

    /**
     * @brief 清理单个任务的相关资源（监听器、超时定时器等），若所有任务都清理完则发射 allTasksFinished
     * @param id 任务 ID
     */
    void cleanTask(quint64 id);

    /**
     * @brief 核心任务提交实现（所有 public 接口最终调用此模板函数）
     * @tparam Func   任务可调用对象类型
     * @tparam CB     完成回调类型（默认为 nullptr_t）
     * @tparam ResultCB 返回值回调类型（默认为 nullptr_t）
     * @param task     要执行的任务
     * @param cb       完成回调（可选，在主线程执行）
     * @param resultCB 结果回调（可选，在主线程执行，接收 task 的返回值）
     * @param pri      优先级（预留，未使用）
     * @param timeoutMs 超时毫秒数，-1 表示无超时
     * @return 任务 ID，0 表示提交失败（如线程池暂停）
     * @details 内部逻辑：
     *          - 生成任务 ID
     *          - 创建 QFutureWatcher 并关联任务 ID
     *          - 若超时 >0，创建单次定时器，超时后调用 cancelTask
     *          - 通过 QtConcurrent::run 提交任务到全局线程池
     *          - 连接 watcher 的 finished 信号，在主线程执行回调并清理
     *          - 异常捕获并输出警告
     */
    template<typename Func, typename CB, typename ResultCB>
    quint64 startImpl(Func task, CB cb, ResultCB resultCB, TaskPriority pri, int timeoutMs)
    {
        Q_UNUSED(pri);  // 优先级当前未实现，保留接口
        QMutexLocker lock(&m_mutex);
        if (m_paused) {
            qWarning() << "[ThreadPool] 线程池已暂停，拒绝提交新任务";
            return 0;
        }

        quint64 taskId = generateId();

        // 创建监听器
        QFutureWatcher<void>* taskWatcher = new QFutureWatcher<void>(this);
        m_taskWatchers.insert(taskId, taskWatcher);

        // 超时处理：创建定时器并保存到超时映射表
        QTimer* timeoutTimer = nullptr;
        if (timeoutMs > 0) {
            timeoutTimer = new QTimer(this);
            timeoutTimer->setSingleShot(true);
            m_timeoutTimers.insert(taskId, timeoutTimer);
            connect(timeoutTimer, &QTimer::timeout, this, [=]() {
                if (cancelTask(taskId)) {
                    emit taskTimeout(taskId);
                }
                // 定时器在 cancelTask 中会被删除，此处无需额外操作
            });
            timeoutTimer->start(timeoutMs);
        }

        // 任务完成处理（连接 finished 信号）
        connect(taskWatcher, &QFutureWatcher<void>::finished, this, [=]() {
            // 清理超时定时器（如果存在且未被清理）
            if (timeoutTimer) {
                QMutexLocker lock2(&m_mutex);
                if (m_timeoutTimers.contains(taskId)) {
                    m_timeoutTimers.take(taskId)->deleteLater();
                }
            }
            // 调用完成回调（如果提供了）
            if constexpr (!std::is_same_v<std::decay_t<CB>, std::nullptr_t>) {
                if (cb) cb();
            }
            emit taskFinished(taskId);
            cleanTask(taskId);
        }, Qt::QueuedConnection);

        // 执行任务的 Runnable（处理返回值和异常）
        auto taskRunnable = [=]() {
            emit taskStarted(taskId);
            try {
                if constexpr (!std::is_same_v<std::decay_t<ResultCB>, std::nullptr_t>) {
                    auto taskResult = task();
                    if (resultCB) {
                        // 结果回调需要转到主线程（因为可能涉及 UI）
                        QMetaObject::invokeMethod(qApp, [resultCB, taskResult]() {
                            resultCB(taskResult);
                        }, Qt::QueuedConnection);
                    }
                } else {
                    task();
                }
            } catch (const std::exception& e) {
                qWarning() << "[ThreadPool] 任务异常:" << e.what();
            } catch (...) {
                qWarning() << "[ThreadPool] 未知异常";
            }
        };

        QFuture<void> taskFuture = QtConcurrent::run(taskRunnable);
        taskWatcher->setFuture(taskFuture);
        return taskId;
    }

private:
    static ThreadPool* m_instance;  //!< 静态指针（保留但实际单例已改用 Magic Static，为兼容旧代码）
    QThreadPool* m_threadPool;         //!< Qt 全局线程池指针，实际执行任务的底层
    mutable QMutex m_mutex;            //!< 保护所有共享容器的互斥锁
    QAtomicInteger<quint64> m_taskIdGenerator; //!< 原子 ID 生成器（线程安全自增）

    QMap<quint64, QFutureWatcher<void>*> m_taskWatchers; //!< 任务 ID -> 对应的 FutureWatcher，用于取消和等待
    QMap<quint64, QTimer*> m_periodicTimers;             //!< 周期任务 ID -> 周期定时器
    QMap<quint64, QTimer*> m_delayTimers;                //!< 延迟任务 ID -> 一次性定时器（延迟期间）
    QMap<quint64, QTimer*> m_timeoutTimers;              //!< 超时任务 ID -> 超时定时器

    bool m_paused = false;  //!< 线程池暂停标志，true 时拒绝新任务提交
};
}
#endif // THREADPOOL_H
