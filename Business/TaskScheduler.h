/**
 * @file TaskScheduler.h
 * @brief 延迟任务调度器 - 支持延迟执行、周期执行、Cron表达式调度
 */

#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMetaObject>
#include <functional>

namespace Sqz {


/**
 * @brief 任务状态枚举
 */
enum class TaskStatus {
    Idle,       // Idle (not started)
    Running,    // Running
    Paused,     // Paused
    Stopped,    // Stopped
    Completed,  // Completed (one-time task)
    Failed      // Execution failed
};

/**
 * @brief 任务类型枚举
 */
enum class TaskType {
    Once,       // One-time task
    Periodic,   // Periodic task
    Cron        // Cron scheduled task
};

/**
 * @brief Cron解析结果
 */
struct CronTime {
    int minute;     // 0-59, -1 means any
    int hour;       // 0-23, -1 means any
    int day;        // 1-31, -1 means any
    int month;      // 1-12, -1 means any
    int weekday;    // 0-6 (0=Sunday), -1 means any
    bool valid;     // Whether valid

    CronTime() : minute(-1), hour(-1), day(-1), month(-1), weekday(-1), valid(false) {}
};

/**
 * @brief 任务信息结构体
 */
struct TaskInfo {
    QString id;                 // Task unique ID
    QString name;               // Task name
    TaskType type;              // Task type
    TaskStatus status;          // Task status
    int intervalMs;             // Periodic interval (ms)
    CronTime cronTime;          // Cron time
    int executeCount;           // Executed count
    int maxExecuteCount;        // Max execute count (-1 means unlimited)
    QDateTime createTime;       // Create time
    QDateTime lastExecuteTime;  // Last execute time
    QDateTime nextExecuteTime;  // Next execute time
    QString lastError;          // Last error message
    bool autoDelete;            // Auto delete after completion

    TaskInfo()
        : type(TaskType::Once),
          status(TaskStatus::Idle),
          intervalMs(0),
          executeCount(0),
          maxExecuteCount(-1),
          autoDelete(true) {}
};

/**
 * @brief 任务统计信息
 */
struct TaskStatistics {
    int totalTasks;         // Total tasks
    int runningTasks;       // Running
    int pausedTasks;        // Paused
    int stoppedTasks;       // Stopped
    int completedTasks;     // Completed
    int failedTasks;        // Failed
    qint64 totalExecutions; // Total executions

    TaskStatistics()
        : totalTasks(0), runningTasks(0), pausedTasks(0),
          stoppedTasks(0), completedTasks(0), failedTasks(0),
          totalExecutions(0) {}
};

/**
 * @brief 延迟任务调度器类
 */
class TaskScheduler : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 任务回调函数类型
     */
    using TaskCallback = std::function<void()>;

    /**
     * @brief 单例模式获取实例
     */
    static TaskScheduler* instance() {
        static TaskScheduler scheduler;
        return &scheduler;
    }

    /**
     * @brief 构造函数（私有，单例模式）
     */
    explicit TaskScheduler(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TaskScheduler();

    // ========== Task Scheduling Interface ==========

    /**
     * @brief Execute task immediately
     */
    QString scheduleNow(TaskCallback callback, const QString& name = "");

    /**
     * @brief Execute task once after delay
     */
    QString scheduleOnce(int delayMs, TaskCallback callback, const QString& name = "");

    /**
     * @brief Execute task periodically
     */
    QString schedulePeriodic(int intervalMs, TaskCallback callback,
                             const QString& name, int maxExecutions = -1);

//    /**
//     * @brief Execute task by Cron expression
//     * Examples:
//     * "0 2 * * *"    Every day at 2:00 AM
//     *  */10 * * * * Every 10 minutes
//     * "0 9 * * 1"    Every Monday at 9:00 AM
//     */
    QString scheduleCron(const QString& cronExpr, TaskCallback callback, const QString& name);

    // ========== Task Management Interface ==========

    /**
     * @brief Stop a task by ID
     */
    bool stopTask(const QString& taskId);

    /**
     * @brief Stop a task by name
     */
    bool stopTaskByName(const QString& name);

    /**
     * @brief Pause a task
     */
    bool pauseTask(const QString& taskId);

    /**
     * @brief Resume a paused task
     */
    bool resumeTask(const QString& taskId);

    /**
     * @brief Stop all tasks
     */
    void stopAllTasks();

    /**
     * @brief Get task info by ID
     */
    TaskInfo getTaskInfo(const QString& taskId) const;

    /**
     * @brief Get all active tasks
     */
    QList<TaskInfo> getAllTasks() const;

    /**
     * @brief Get task history
     */
    QList<TaskInfo> getTaskHistory(int count = 100) const;

    /**
     * @brief Check if task exists
     */
    bool hasTask(const QString& taskId) const;

    /**
     * @brief Check if task is running
     */
    bool isTaskRunning(const QString& taskId) const;

    /**
     * @brief Get statistics
     */
    TaskStatistics getStatistics() const;

signals:
    void taskCompleted(const QString& taskId);
    void taskFailed(const QString& taskId, const QString& error);
    void taskExecuted(const QString& taskId, int count);
    void taskStopped(const QString& taskId);
    void taskPaused(const QString& taskId);
    void taskResumed(const QString& taskId);

private:
    /**
     * @brief Create task info
     */
    TaskInfo createTaskInfo(const QString& name, TaskType type);

    /**
     * @brief Parse Cron expression
     */
    bool parseCronExpression(const QString& expr, CronTime& out);

    /**
     * @brief Parse single Cron field
     */
    int parseCronField(const QString& field, int min, int max);

    /**
     * @brief Calculate next Cron execution time
     */
    QDateTime calculateNextCronTime(const CronTime& cronTime);

    /**
     * @brief Clean up completed tasks
     */
    void cleanupCompletedTasks();

    /**
     * @brief Load config (internal)
     */
    bool loadConfig(TaskInfo& info);

private:
    struct TaskEntry {
        TaskInfo info;
        QTimer* timer;
        TaskCallback callback;

        TaskEntry() : timer(nullptr) {}
    };

    int m_nextTaskId;
    bool m_isShuttingDown;
    QHash<QString, TaskEntry> m_activeTasks;
    QHash<QString, QString> m_taskNames;
    QList<TaskInfo> m_taskHistory;
    QTimer m_cleanupTimer;
    mutable QMutex m_mutex;
};
}
#endif // TASKSCHEDULER_H
