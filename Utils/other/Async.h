// Async.h
#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <functional>
#include <type_traits>
#include <exception>

namespace Async{


// ---------- 前置声明 ----------
template <typename T>
class Task;

// ---------- 异步入口类 ----------
class Async
{
public:
    // 提交一个任务，返回可链式调用的 Task 对象
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
        using SuccessCallback = std::function<void(T)>;
        using ErrorCallback   = std::function<void(std::exception_ptr)>;

        explicit Task(std::function<T()> task) : m_task(std::move(task)) {}

        // 设置成功回调（主线程执行）
        Task& OnSuccess(SuccessCallback cb)
        {
            m_onSuccess = std::move(cb);
            return *this;
        }

        // 设置错误回调（主线程执行）
        Task& OnError(ErrorCallback cb)
        {
            m_onError = std::move(cb);
            return *this;
        }

        // 启动异步任务
        void Start()
        {
            QFuture<T> future = QtConcurrent::run(m_task);
            auto watcher = new QFutureWatcher<T>();

            QObject::connect(watcher, &QFutureWatcher<T>::finished,
                             [watcher, onSuccess = m_onSuccess, onError = m_onError]() {
                if (watcher->isCanceled()) return;
                try {
                    T result = watcher->result();
                    if (onSuccess) onSuccess(result);
                } catch (...) {
                    if (onError) onError(std::current_exception());
                }
                watcher->deleteLater();
            });

            watcher->setFuture(future);
        }

    private:
        std::function<T()> m_task;
        SuccessCallback    m_onSuccess;
        ErrorCallback      m_onError;
    };

    // ---------- void 特化（无返回值） ----------
    template <>
    class Task<void>
    {
    public:
        using SuccessCallback = std::function<void()>;
        using ErrorCallback   = std::function<void(std::exception_ptr)>;

        explicit Task(std::function<void()> task) : m_task(std::move(task)) {}

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
            QFuture<void> future = QtConcurrent::run(m_task);
            auto watcher = new QFutureWatcher<void>();

            QObject::connect(watcher, &QFutureWatcher<void>::finished,
                             [watcher, onSuccess = m_onSuccess, onError = m_onError]() {
                if (watcher->isCanceled()) return;
                try {
                    watcher->waitForFinished();   // 触发异常重抛
                    if (onSuccess) onSuccess();
                } catch (...) {
                    if (onError) onError(std::current_exception());
                }
                watcher->deleteLater();
            });

            watcher->setFuture(future);
        }

    private:
        std::function<void()> m_task;
        SuccessCallback       m_onSuccess;
        ErrorCallback    m_onError;
    };
}

#if A
// 1. 有返回值
Async::Run([]() -> int {
    QThread::sleep(1);
    return 100;
})
.OnSuccess([](int res) { qDebug() << "结果:" << res; })
.OnError([](std::exception_ptr e) { /* 处理异常 */ })
.Start();

// 2. 无返回值
Async::Run([]() {
    qDebug() << "后台工作中...";
})
.OnSuccess([]() { qDebug() << "完成!"; })
.Start();

// 3. 只关注成功，忽略错误（不写 OnError 即可）
Async::Run([]() { return 3.14; })
.OnSuccess([](double pi) { qDebug() << pi; })
.Start();

// 4. 先构建，稍后启动
auto task = Async::Run([]() { return "Hello"; });
task.OnSuccess([](QString s) { qDebug() << s; });
task.Start();   // 可选时机触发

#endif
