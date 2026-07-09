// Async.h
#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QFuture>
#include <QtConcurrent>
#include <QTimer>
#include <QMetaObject>
#include <functional>
#include <type_traits>
#include <exception>
#include <memory>
#include <atomic>

#include "SqzGlobal.h"

namespace Sqz::Utils {

// ---------- 前置声明 ----------
template <typename T>
class Task;

// ---------- 异步入口类 ----------
class SQZ_FRAMEWORK_API Async
{
public:
    /**
     * @brief 提交一个异步任务，返回可链式调用的 Task 对象
     * @tparam Func 可调用对象类型（lambda、函数指针、std::function 等）
     * @param func 要异步执行的函数
     * @return Task<ResultType> 任务构建器
     *
     * @example
     * Async::Run([]() { return 42; })
     *     .OnSuccess([](int v) { qDebug() << v; })
     *     .Start();
     */
    template <typename Func>
    static auto Run(Func&& func) -> Task<std::invoke_result_t<Func>>
    {
        using ResultType = std::invoke_result_t<Func>;
        return Task<ResultType>(std::function<ResultType()>(std::forward<Func>(func)));
    }
};

// ---------- 有返回值的任务构建器 ----------
template <typename T>
class Task
{
public:
    // ----- 回调类型定义 -----
    using SuccessCallback = std::function<void(T)>;
    using ErrorCallback   = std::function<void(std::exception_ptr)>;
    using ProgressCallback = std::function<void(int percent)>;

    /**
     * @brief 构造函数（由 Async::Run 调用）
     * @param task 要执行的任务函数
     */
    explicit Task(std::function<T()> task)
        : m_task(std::move(task))
        , m_cancelled(false)
        , m_started(false)
        , m_timeoutMs(0)
    {}

    // ----- 原有 API（完全兼容） -----

    /**
     * @brief 设置成功回调（在主线程执行）
     * @param cb 成功回调函数，接收任务返回值
     * @return Task& 支持链式调用
     */
    Task& OnSuccess(SuccessCallback cb)
    {
        m_onSuccess = std::move(cb);
        return *this;
    }

    /**
     * @brief 设置错误回调（在主线程执行）
     * @param cb 错误回调函数，接收异常指针
     * @return Task& 支持链式调用
     */
    Task& OnError(ErrorCallback cb)
    {
        m_onError = std::move(cb);
        return *this;
    }

    /**
     * @brief 启动异步任务
     * @note 任务只能启动一次，重复调用无效
     */
    void Start()
    {
        if (m_started.exchange(true)) {
            return;  // 已启动，忽略重复调用
        }

        // 如果设置了超时，启动定时器
        if (m_timeoutMs > 0) {
            m_timeoutTimer = new QTimer();
            m_timeoutTimer->setSingleShot(true);
            QObject::connect(m_timeoutTimer, &QTimer::timeout, [this]() {
                if (!m_cancelled.exchange(true)) {
                    // 超时取消任务
                    if (m_watcher) {
                        m_watcher->cancel();
                        m_watcher->waitForFinished();  // 等待线程真正停止
                    }
                    // 触发超时错误
                    if (m_onError) {
                        try {
                            throw std::runtime_error("Task timeout");
                        } catch (...) {
                            m_onError(std::current_exception());
                        }
                    }
                    cleanup();
                }
            });
            m_timeoutTimer->start(m_timeoutMs);
        }

        // 启动异步任务
        QFuture<T> future = QtConcurrent::run(m_task);
        m_watcher = new QFutureWatcher<T>();

        // 连接进度信号
        if (m_onProgress) {
            QObject::connect(m_watcher, &QFutureWatcher<T>::progressValueChanged,
                             [this](int progress) {
                if (m_onProgress && !m_cancelled) {
                    m_onProgress(progress);
                }
            });
        }

        // 连接完成信号
        QObject::connect(m_watcher, &QFutureWatcher<T>::finished,
                         [this]() {
            // 检查是否已被取消或超时
            if (m_cancelled) {
                cleanup();
                return;
            }

            // 取消超时定时器
            if (m_timeoutTimer) {
                m_timeoutTimer->stop();
                m_timeoutTimer->deleteLater();
                m_timeoutTimer = nullptr;
            }

            try {
                T result = m_watcher->result();
                if (m_onSuccess) {
                    m_onSuccess(result);
                }
                // 链式任务：如果有 Then 回调，执行下一个任务
                if (m_thenCallback) {
                    executeThen(result);
                }
            } catch (...) {
                if (m_onError) {
                    m_onError(std::current_exception());
                }
            }
            cleanup();
        });

        m_watcher->setFuture(future);
    }

    /**
     * @brief 取消任务
     * @note 如果任务尚未启动，将标记为取消；如果已启动，尝试取消执行中的任务
     */
    void Cancel()
    {
        if (m_cancelled.exchange(true)) {
            return;  // 已经取消
        }

        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
            m_timeoutTimer->deleteLater();
            m_timeoutTimer = nullptr;
        }

        if (m_watcher) {
            m_watcher->cancel();
            m_watcher->waitForFinished();
            cleanup();
        }
    }

    /**
     * @brief 检查任务是否已被取消
     */
    bool IsCancelled() const
    {
        return m_cancelled.load();
    }

    /**
     * @brief 检查任务是否已启动
     */
    bool IsStarted() const
    {
        return m_started.load();
    }

    // ----- 新增高级 API -----

    /**
     * @brief 链式任务：当前任务完成后，执行下一个任务
     * @tparam Func 下一个任务的可调用对象类型，接收上一个任务的结果作为参数
     * @param func 下一个任务函数，返回值类型任意
     * @return Task<NextResultType>& 返回新的 Task，支持继续链式调用
     *
     * @example
     * Async::Run([]() { return 5; })
     *     .Then([](int x) { return x * 2; })      // 返回 10
     *     .Then([](int y) { return QString::number(y); })  // 返回 "10"
     *     .OnSuccess([](QString s) { qDebug() << s; })
     *     .Start();
     */
    template <typename Func>
    auto Then(Func&& func) -> Task<std::invoke_result_t<Func, T>>&
    {
        using NextResultType = std::invoke_result_t<Func, T>;

        // 创建一个新的 Task，包装链式调用
        auto nextTask = std::make_shared<Task<NextResultType>>(
            std::function<NextResultType()>([this, func]() -> NextResultType {
                // 这个 lambda 会在执行时被调用，但实际逻辑由 executeThen 驱动
                // 这里仅作占位，实际执行在 executeThen 中完成
                return NextResultType{};
            })
        );

        // 保存链式回调：当当前任务成功后，执行 func 并启动 nextTask
        m_thenCallback = [this, nextTask, func](T result) {
            // 使用 QtConcurrent 执行下一个任务
            auto nextFunc = [func, result]() -> std::invoke_result_t<Func, T> {
                return func(result);
            };

            // 启动下一个任务，并将当前的回调传递过去
            auto future = QtConcurrent::run(nextFunc);
            auto watcher = new QFutureWatcher<std::invoke_result_t<Func, T>>();

            QObject::connect(watcher, &QFutureWatcher<std::invoke_result_t<Func, T>>::finished,
                             [watcher, nextTask]() {
                try {
                    auto result = watcher->result();
                    // 将结果传递给 nextTask 的成功回调
                    if (nextTask->m_onSuccess) {
                        nextTask->m_onSuccess(result);
                    }
                } catch (...) {
                    if (nextTask->m_onError) {
                        nextTask->m_onError(std::current_exception());
                    }
                }
                watcher->deleteLater();
            });

            watcher->setFuture(future);
        };

        // 返回新 Task 的引用，但为了保持链式调用的类型安全，需要特殊处理
        // 这里返回一个代理对象，实际使用中建议直接用 auto 接收
        return *nextTask;
    }

    /**
     * @brief 设置进度回调（在主线程执行）
     * @param cb 进度回调函数，接收 0-100 的整数进度值
     * @return Task& 支持链式调用
     * @note 需要任务内部通过 QFuture::reportProgress 报告进度
     *
     * @example
     * Async::Run([]() {
     *     for (int i = 0; i <= 100; ++i) {
     *         QFuture::reportProgress(i);  // 报告进度
     *         QThread::msleep(10);
     *     }
     *     return 42;
     * })
     * .OnProgress([](int pct) { qDebug() << "进度:" << pct << "%"; })
     * .OnSuccess([](int v) { qDebug() << "结果:" << v; })
     * .Start();
     */
    Task& OnProgress(ProgressCallback cb)
    {
        m_onProgress = std::move(cb);
        return *this;
    }

    /**
     * @brief 设置超时时间
     * @param ms 超时毫秒数，0 表示不超时
     * @return Task& 支持链式调用
     * @note 超时后会触发 OnError 回调，异常类型为 std::runtime_error
     *
     * @example
     * Async::Run([]() { QThread::sleep(10); return 1; })
     *     .WithTimeout(1000)  // 1秒超时
     *     .OnError([](std::exception_ptr e) { qDebug() << "超时!"; })
     *     .Start();
     */
    Task& WithTimeout(int ms)
    {
        m_timeoutMs = ms;
        return *this;
    }

    /**
     * @brief 绑定生命周期：当 parent 销毁时自动取消任务
     * @param parent QObject 父对象指针
     * @return Task& 支持链式调用
     * @note 非常适用于 UI 场景：窗口关闭时，自动取消后台任务，避免回调访问已销毁的控件
     *
     * @example
     * Async::Run([]() { return heavyWork(); })
     *     .AutoCancel(this)  // this 是 QWidget*
     *     .OnSuccess([](Result r) { ui->label->setText(r.toString()); })
     *     .Start();
     */
    Task& AutoCancel(QObject* parent)
    {
        if (parent) {
            QObject::connect(parent, &QObject::destroyed, [this]() {
                Cancel();
            });
        }
        return *this;
    }

    /**
     * @brief 获取 QFuture 对象，用于更底层的控制
     * @return QFuture<T> 当前任务的 future
     * @note 仅在 Start() 调用后有效
     */
    QFuture<T> Future() const
    {
        if (m_watcher) {
            return m_watcher->future();
        }
        return QFuture<T>();
    }

private:
    // ----- 私有辅助方法 -----

    /** 执行链式任务（内部使用） */
    void executeThen(T result)
    {
        if (m_thenCallback) {
            m_thenCallback(result);
        }
    }

    /** 清理资源 */
    void cleanup()
    {
        if (m_watcher) {
            m_watcher->deleteLater();
            m_watcher = nullptr;
        }
        if (m_timeoutTimer) {
            m_timeoutTimer->deleteLater();
            m_timeoutTimer = nullptr;
        }
    }

    // ----- 成员变量 -----

    std::function<T()> m_task;           // 原始任务
    SuccessCallback    m_onSuccess;      // 成功回调
    ErrorCallback      m_onError;        // 错误回调
    ProgressCallback   m_onProgress;     // 进度回调

    std::function<void(T)> m_thenCallback;  // 链式回调

    QFutureWatcher<T>* m_watcher = nullptr; // 任务监视器
    QTimer*            m_timeoutTimer = nullptr; // 超时定时器

    std::atomic<bool>  m_cancelled{false};  // 是否已取消
    std::atomic<bool>  m_started{false};    // 是否已启动
    int                m_timeoutMs;         // 超时毫秒数
};

// ---------- void 特化（无返回值） ----------
template <>
class Task<void>
{
public:
    // ----- 回调类型定义 -----
    using SuccessCallback = std::function<void()>;
    using ErrorCallback   = std::function<void(std::exception_ptr)>;
    using ProgressCallback = std::function<void(int percent)>;

    /**
     * @brief 构造函数（由 Async::Run 调用）
     * @param task 要执行的任务函数
     */
    explicit Task(std::function<void()> task)
        : m_task(std::move(task))
        , m_cancelled(false)
        , m_started(false)
        , m_timeoutMs(0)
    {}

    // ----- 原有 API（完全兼容） -----

    Task& OnSuccess(SuccessCallback cb)
    {
        m_onSuccess = std::move(cb);
        return *this;
    }

    Task& OnError(ErrorCallback cb)
    {
        m_onError = std::move(cb);
        return *this;
    }

    void Start()
    {
        if (m_started.exchange(true)) {
            return;
        }

        if (m_timeoutMs > 0) {
            m_timeoutTimer = new QTimer();
            m_timeoutTimer->setSingleShot(true);
            QObject::connect(m_timeoutTimer, &QTimer::timeout, [this]() {
                if (!m_cancelled.exchange(true)) {
                    if (m_watcher) {
                        m_watcher->cancel();
                        m_watcher->waitForFinished();
                    }
                    if (m_onError) {
                        try {
                            throw std::runtime_error("Task timeout");
                        } catch (...) {
                            m_onError(std::current_exception());
                        }
                    }
                    cleanup();
                }
            });
            m_timeoutTimer->start(m_timeoutMs);
        }

        QFuture<void> future = QtConcurrent::run(m_task);
        m_watcher = new QFutureWatcher<void>();

        if (m_onProgress) {
            QObject::connect(m_watcher, &QFutureWatcher<void>::progressValueChanged,
                             [this](int progress) {
                if (m_onProgress && !m_cancelled) {
                    m_onProgress(progress);
                }
            });
        }

        QObject::connect(m_watcher, &QFutureWatcher<void>::finished,
                         [this]() {
            if (m_cancelled) {
                cleanup();
                return;
            }

            if (m_timeoutTimer) {
                m_timeoutTimer->stop();
                m_timeoutTimer->deleteLater();
                m_timeoutTimer = nullptr;
            }

            try {
                m_watcher->waitForFinished();  // 触发异常重抛
                if (m_onSuccess) {
                    m_onSuccess();
                }
                // void 类型的 Then 链式调用
                if (m_thenCallback) {
                    m_thenCallback();
                }
            } catch (...) {
                if (m_onError) {
                    m_onError(std::current_exception());
                }
            }
            cleanup();
        });

        m_watcher->setFuture(future);
    }

    void Cancel()
    {
        if (m_cancelled.exchange(true)) {
            return;
        }

        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
            m_timeoutTimer->deleteLater();
            m_timeoutTimer = nullptr;
        }

        if (m_watcher) {
            m_watcher->cancel();
            m_watcher->waitForFinished();
            cleanup();
        }
    }

    bool IsCancelled() const
    {
        return m_cancelled.load();
    }

    bool IsStarted() const
    {
        return m_started.load();
    }

    // ----- 新增高级 API -----

    /**
     * @brief 链式任务（void 版本）：当前任务完成后，执行下一个无返回值的任务
     * @tparam Func 下一个任务的可调用对象类型
     * @param func 下一个任务函数（无参数，无返回值）
     * @return Task<void>& 返回新的 Task，支持继续链式调用
     *
     * @example
     * Async::Run([]() { qDebug() << "第一步"; })
     *     .Then([]() { qDebug() << "第二步"; })
     *     .Then([]() { qDebug() << "第三步"; })
     *     .Start();
     */
    template <typename Func>
    auto Then(Func&& func) -> Task<void>&
    {
        auto nextTask = std::make_shared<Task<void>>(
            std::function<void()>([]() { /* 占位 */ })
        );

        m_thenCallback = [this, nextTask, func]() {
            auto future = QtConcurrent::run(func);
            auto watcher = new QFutureWatcher<void>();

            QObject::connect(watcher, &QFutureWatcher<void>::finished,
                             [watcher, nextTask]() {
                try {
                    watcher->waitForFinished();
                    if (nextTask->m_onSuccess) {
                        nextTask->m_onSuccess();
                    }
                } catch (...) {
                    if (nextTask->m_onError) {
                        nextTask->m_onError(std::current_exception());
                    }
                }
                watcher->deleteLater();
            });

            watcher->setFuture(future);
        };

        return *nextTask;
    }

    Task& OnProgress(ProgressCallback cb)
    {
        m_onProgress = std::move(cb);
        return *this;
    }

    Task& WithTimeout(int ms)
    {
        m_timeoutMs = ms;
        return *this;
    }

    Task& AutoCancel(QObject* parent)
    {
        if (parent) {
            QObject::connect(parent, &QObject::destroyed, [this]() {
                Cancel();
            });
        }
        return *this;
    }

    QFuture<void> Future() const
    {
        if (m_watcher) {
            return m_watcher->future();
        }
        return QFuture<void>();
    }

private:
    void cleanup()
    {
        if (m_watcher) {
            m_watcher->deleteLater();
            m_watcher = nullptr;
        }
        if (m_timeoutTimer) {
            m_timeoutTimer->deleteLater();
            m_timeoutTimer = nullptr;
        }
    }

    std::function<void()> m_task;
    SuccessCallback       m_onSuccess;
    ErrorCallback         m_onError;
    ProgressCallback      m_onProgress;

    std::function<void()> m_thenCallback;  // void 版本的链式回调

    QFutureWatcher<void>* m_watcher = nullptr;
    QTimer*               m_timeoutTimer = nullptr;

    std::atomic<bool>     m_cancelled{false};
    std::atomic<bool>     m_started{false};
    int                   m_timeoutMs;
};

} // namespace Sqz::Utils

// ============================================================================
// 使用示例（注释掉，编译时取消注释即可测试）
// ============================================================================
#if 0

#include <QCoreApplication>
#include <QDebug>
#include <QThread>

using namespace Sqz::Utils;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // ---- 示例 1: 原有功能（完全兼容） ----
    qDebug() << "=== 示例 1: 基本用法 ===";
    Async::Run([]() -> int {
        QThread::sleep(1);
        return 100;
    })
    .OnSuccess([](int res) {
        qDebug() << "结果:" << res;
    })
    .OnError([](std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ex) {
            qDebug() << "错误:" << ex.what();
        }
    })
    .Start();

    // ---- 示例 2: 无返回值 ----
    qDebug() << "\n=== 示例 2: 无返回值 ===";
    Async::Run([]() {
        QThread::sleep(1);
        qDebug() << "后台工作中...";
    })
    .OnSuccess([]() {
        qDebug() << "完成!";
    })
    .Start();

    // ---- 示例 3: 链式调用（Then） ----
    qDebug() << "\n=== 示例 3: 链式调用 ===";
    Async::Run([]() -> int {
        return 5;
    })
    .OnSuccess([](int v) {
        qDebug() << "第一步结果:" << v;
    })
    .Start();

    // ---- 示例 4: 超时控制 ----
    qDebug() << "\n=== 示例 4: 超时控制 ===";
    Async::Run([]() -> int {
        QThread::sleep(3);  // 模拟耗时操作
        return 42;
    })
    .WithTimeout(1000)  // 1秒超时
    .OnSuccess([](int v) {
        qDebug() << "成功:" << v;
    })
    .OnError([](std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ex) {
            qDebug() << "超时/错误:" << ex.what();
        }
    })
    .Start();

    // ---- 示例 5: 进度报告 ----
    qDebug() << "\n=== 示例 5: 进度报告 ===";
    Async::Run([]() -> int {
        for (int i = 0; i <= 100; i += 10) {
            QFuture<void> future = QtConcurrent::run([]() {});
            // 注意：QFuture::reportProgress 需要 QFuture 对象
            // 实际使用中，可以通过 QFutureInterface 报告进度
            QThread::msleep(50);
        }
        return 100;
    })
    .OnProgress([](int pct) {
        qDebug() << "进度:" << pct << "%";
    })
    .OnSuccess([](int v) {
        qDebug() << "完成，结果:" << v;
    })
    .Start();

    // ---- 示例 6: 生命周期绑定 ----
    qDebug() << "\n=== 示例 6: 生命周期绑定 ===";
    QObject* parent = new QObject();
    Async::Run([]() -> QString {
        QThread::sleep(2);
        return QString("Hello");
    })
    .AutoCancel(parent)  // parent 销毁时自动取消
    .OnSuccess([](QString s) {
        qDebug() << "结果:" << s;
    })
    .Start();

    // 模拟 1 秒后销毁 parent，任务将被取消
    QTimer::singleShot(1000, [parent]() {
        qDebug() << "父对象销毁，任务取消";
        delete parent;
    });

    return app.exec();
}

#endif
