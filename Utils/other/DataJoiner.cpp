#include "DataJoiner.h"
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QMap>
#include <QDebug>

namespace Sqz::Utils {

// ==================== 内部数据结构 ====================

struct Task {
    int id;
    int expectedCount;
    QMap<QString, QVariant> buffer;   // 已收到的数据（tag -> data）
    QTimer* timer;                     // 超时定时器
    bool isReady = false;

    Sqz::Utils::ReadyCallback onReady;
    Sqz::Utils::TimeoutCallback onTimeout;

    Task(int _id, int count) : id(_id), expectedCount(count) {
        timer = new QTimer();
        timer->setSingleShot(true);
    }

    ~Task() {
        if (timer) {
            timer->stop();
            delete timer;
        }
    }

    // 禁止拷贝
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
};

// ==================== 全局管理器 ====================

namespace {
    QMutex g_mutex;
    QMap<int, Task*> g_tasks;
    int g_nextId = 1;
}

// 内部函数：删除任务
static void RemoveTask(int taskId) {
    if (g_tasks.contains(taskId)) {
        delete g_tasks[taskId];
        g_tasks.remove(taskId);
    }
}

// 内部函数：检查是否凑齐
static void CheckAndNotify(int taskId) {
    Task* task = nullptr;
    {
        QMutexLocker locker(&g_mutex);
        if (!g_tasks.contains(taskId)) return;
        task = g_tasks[taskId];
    }

    // 检查是否凑齐
    if (task->buffer.size() < task->expectedCount) return;

    // 凑齐了！
    task->isReady = true;
    task->timer->stop();

    QMap<QString, QVariant> dataCopy = task->buffer;

    // 触发 OnReady 回调
    if (task->onReady) {
        task->onReady(dataCopy);
    }

    // 任务完成，自动销毁
    RemoveTask(taskId);
}

// 内部函数：超时处理
static void OnTimeout(int taskId) {
    Task* task = nullptr;
    {
        QMutexLocker locker(&g_mutex);
        if (!g_tasks.contains(taskId)) return;
        task = g_tasks[taskId];
    }

    // 如果已经凑齐了，忽略超时（实际上凑齐时任务已删除，不会走到这里）
    if (task->isReady) return;

    // 触发 OnTimeout 回调（如果有部分数据）
    if (task->onTimeout && !task->buffer.isEmpty()) {
        task->onTimeout(task->buffer);
    }

    // 超时后自动删除任务
    RemoveTask(taskId);
}

// ==================== 对外接口实现 ====================

int Begin(int expectedCount) {
    if (expectedCount <= 0) {
        qWarning() << "DataJoiner::Begin: expectedCount must be > 0";
        return -1;
    }

    QMutexLocker locker(&g_mutex);
    int id = g_nextId++;
    Task* task = new Task(id, expectedCount);

    // 连接定时器超时信号
    QObject::connect(task->timer, &QTimer::timeout, [id]() {
        OnTimeout(id);
    });

    g_tasks[id] = task;
    return id;
}

void Cancel(int taskId) {
    RemoveTask(taskId);
}

void Reset(int taskId) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) return;

    Task* task = g_tasks[taskId];
    task->buffer.clear();
    task->isReady = false;
    task->timer->stop();
}

void OnReady(int taskId, ReadyCallback callback) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) {
        qWarning() << "DataJoiner::OnReady: task not found" << taskId;
        return;
    }
    g_tasks[taskId]->onReady = callback;
}

void OnTimeout(int taskId, TimeoutCallback callback) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) {
        qWarning() << "DataJoiner::OnTimeout: task not found" << taskId;
        return;
    }
    g_tasks[taskId]->onTimeout = callback;
}

void SetTimeout(int taskId, int ms) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) {
        qWarning() << "DataJoiner::SetTimeout: task not found" << taskId;
        return;
    }
    g_tasks[taskId]->timer->setInterval(ms);
}

void Feed(int taskId, const QString& tag, const QVariant& data) {
    Task* task = nullptr;
    {
        QMutexLocker locker(&g_mutex);
        if (!g_tasks.contains(taskId)) {
            qWarning() << "DataJoiner::Feed: task not found" << taskId;
            return;
        }
        task = g_tasks[taskId];
    }

    // 如果已经凑齐了，自动重置并重新开始（或者可以选择忽略）
    if (task->isReady) {
        task->buffer.clear();
        task->isReady = false;
    }

    // 存入数据（相同 tag 覆盖旧值，不会重复计数）
    task->buffer[tag] = data;

    // 刷新超时计时器（每来一路数据，重新计时）
    task->timer->start();

    // 检查是否凑齐
    CheckAndNotify(taskId);
}

bool IsReady(int taskId) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) return false;
    return g_tasks[taskId]->isReady;
}

int ReceivedCount(int taskId) {
    QMutexLocker locker(&g_mutex);
    if (!g_tasks.contains(taskId)) return -1;
    return g_tasks[taskId]->buffer.size();
}

int ActiveCount() {
    QMutexLocker locker(&g_mutex);
    return g_tasks.size();
}

void ResetAll() {
    QMutexLocker locker(&g_mutex);
    for (auto it = g_tasks.begin(); it != g_tasks.end(); ++it) {
        delete it.value();
    }
    g_tasks.clear();
    g_nextId = 1;
}

} // namespace DataJoiner
