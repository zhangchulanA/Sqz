/**
 * @file TaskScheduler.cpp
 * @brief 延迟任务调度器实现
 */

#include "TaskScheduler.h"
#include <QFile>
#include <QCryptographicHash>

// ========== Constructor / Destructor ==========
namespace Sqz {


TaskScheduler::TaskScheduler(QObject* parent)
    : QObject(parent)
    , m_nextTaskId(1)
    , m_isShuttingDown(false)
{
    // Start cleanup timer (check every minute)
    m_cleanupTimer.setInterval(60000);
    connect(&m_cleanupTimer, &QTimer::timeout,
            this, &TaskScheduler::cleanupCompletedTasks);
    m_cleanupTimer.start();

    qDebug() << "[TaskScheduler] Scheduler started";
}

TaskScheduler::~TaskScheduler()
{
    m_isShuttingDown = true;
    stopAllTasks();
    qDebug() << "[TaskScheduler] Scheduler stopped";
}

// ========== Task Scheduling ==========

QString TaskScheduler::scheduleNow(TaskCallback callback, const QString& name)
{
    QMutexLocker locker(&m_mutex);

    TaskInfo info = createTaskInfo(name, TaskType::Once);
    info.status = TaskStatus::Running;

    qDebug() << "[TaskScheduler] Execute immediately:" << info.name << "ID:" << info.id;

    try {
        callback();
        info.status = TaskStatus::Completed;
        emit taskCompleted(info.id);
    } catch (const std::exception& e) {
        info.status = TaskStatus::Failed;
        info.lastError = e.what();
        qCritical() << "[TaskScheduler] Task failed:" << info.id << "Error:" << e.what();
        emit taskFailed(info.id, info.lastError);
    }

    m_taskHistory.append(info);
    if (m_taskHistory.size() > 1000) {
        m_taskHistory.removeFirst();
    }

    return info.id;
}

QString TaskScheduler::scheduleOnce(int delayMs, TaskCallback callback, const QString& name)
{
    QMutexLocker locker(&m_mutex);

    TaskInfo info = createTaskInfo(name, TaskType::Once);
    info.status = TaskStatus::Running;
    info.nextExecuteTime = QDateTime::currentDateTime().addMSecs(delayMs);

    qDebug() << "[TaskScheduler] Schedule once after" << delayMs << "ms:"
             << info.name << "ID:" << info.id;

    TaskEntry entry;
    entry.info = info;
    entry.callback = callback;
    entry.timer = new QTimer(this);
    entry.timer->setSingleShot(true);
    entry.timer->setInterval(delayMs);

    connect(entry.timer, &QTimer::timeout, this, [this, info, callback]() {
        if (m_isShuttingDown) {
            qDebug() << "[TaskScheduler] Shutting down, cancel task:" << info.id;
            return;
        }

        qDebug() << "[TaskScheduler] Executing delayed task:" << info.name;

        try {
            callback();
            // Update task info
            QMutexLocker locker(&m_mutex);
            if (m_activeTasks.contains(info.id)) {
                m_activeTasks[info.id].info.status = TaskStatus::Completed;
                emit taskCompleted(info.id);
            }
            m_taskHistory.append(info);
        } catch (const std::exception& e) {
            qCritical() << "[TaskScheduler] Task failed:" << info.id << "Error:" << e.what();
            emit taskFailed(info.id, e.what());
        }

        // Clean up
        QMutexLocker locker(&m_mutex);
        m_activeTasks.remove(info.id);
    });

    entry.timer->start();
    m_activeTasks[info.id] = entry;
    if (!name.isEmpty()) {
        m_taskNames[name] = info.id;
    }

    return info.id;
}

QString TaskScheduler::schedulePeriodic(int intervalMs, TaskCallback callback,
                                        const QString& name, int maxExecutions)
{
    QMutexLocker locker(&m_mutex);

    if (name.isEmpty()) {
        qWarning() << "[TaskScheduler] Periodic task must have a name";
        return "";
    }

    // Stop existing task with same name
    if (m_taskNames.contains(name)) {
        QString oldId = m_taskNames[name];
        stopTask(oldId);
    }

    TaskInfo info = createTaskInfo(name, TaskType::Periodic);
    info.intervalMs = intervalMs;
    info.maxExecuteCount = maxExecutions;
    info.status = TaskStatus::Running;
    info.nextExecuteTime = QDateTime::currentDateTime().addMSecs(intervalMs);

    qDebug() << "[TaskScheduler] Create periodic task:" << name
             << "Interval:" << intervalMs << "ms Max:" << maxExecutions;

    TaskEntry entry;
    entry.info = info;
    entry.callback = callback;
    entry.timer = new QTimer(this);
    entry.timer->setInterval(intervalMs);

    connect(entry.timer, &QTimer::timeout, this, [this, info, callback]() mutable {
        if (m_isShuttingDown) {
            return;
        }

        QMutexLocker locker(&m_mutex);

        // Check if task still exists
        if (!m_activeTasks.contains(info.id)) {
            return;
        }

        TaskEntry& entry = m_activeTasks[info.id];

        // Check max executions
        if (entry.info.maxExecuteCount > 0 &&
            entry.info.executeCount >= entry.info.maxExecuteCount) {
            qDebug() << "[TaskScheduler] Task reached max executions:" << entry.info.name;
            stopTask(entry.info.id);
            return;
        }

        // Check if paused
        if (entry.info.status == TaskStatus::Paused) {
            return;
        }

        qDebug() << "[TaskScheduler] Execute periodic task:" << entry.info.name
                 << "#" << (entry.info.executeCount + 1);

        try {
            callback();
            entry.info.executeCount++;
            entry.info.lastExecuteTime = QDateTime::currentDateTime();
            entry.info.nextExecuteTime = entry.info.lastExecuteTime.addMSecs(entry.info.intervalMs);

            emit taskExecuted(entry.info.id, entry.info.executeCount);
            qDebug() << "[TaskScheduler] Periodic task done:" << entry.info.name
                     << "Executed" << entry.info.executeCount << "times";

        } catch (const std::exception& e) {
            entry.info.status = TaskStatus::Failed;
            entry.info.lastError = e.what();
            qCritical() << "[TaskScheduler] Periodic task failed:" << entry.info.id
                        << "Error:" << e.what();
            emit taskFailed(entry.info.id, entry.info.lastError);
            stopTask(entry.info.id);
        }
    });

    entry.timer->start();
    m_activeTasks[info.id] = entry;
    m_taskNames[name] = info.id;

    return info.id;
}

QString TaskScheduler::scheduleCron(const QString& cronExpr, TaskCallback callback,
                                    const QString& name)
{
    QMutexLocker locker(&m_mutex);

    if (name.isEmpty()) {
        qWarning() << "[TaskScheduler] Cron task must have a name";
        return "";
    }

    // Parse Cron expression
    CronTime cronTime;
    if (!parseCronExpression(cronExpr, cronTime)) {
        qWarning() << "[TaskScheduler] Invalid Cron expression:" << cronExpr;
        return "";
    }

    // Stop existing task with same name
    if (m_taskNames.contains(name)) {
        QString oldId = m_taskNames[name];
        stopTask(oldId);
    }

    TaskInfo info = createTaskInfo(name, TaskType::Cron);
    info.cronTime = cronTime;
    info.status = TaskStatus::Running;
    info.nextExecuteTime = calculateNextCronTime(cronTime);

    qDebug() << "[TaskScheduler] Create Cron task:" << name
             << "Expression:" << cronExpr
             << "Next:" << info.nextExecuteTime.toString("yyyy-MM-dd hh:mm:ss");

    TaskEntry entry;
    entry.info = info;
    entry.callback = callback;
    entry.timer = new QTimer(this);
    entry.timer->setInterval(60000);  // Check every minute

    connect(entry.timer, &QTimer::timeout, this, [this, info, callback, cronTime]() mutable {
        if (m_isShuttingDown) {
            return;
        }

        QMutexLocker locker(&m_mutex);

        if (!m_activeTasks.contains(info.id)) {
            return;
        }

        TaskEntry& entry = m_activeTasks[info.id];

        if (entry.info.status == TaskStatus::Stopped ||
            entry.info.status == TaskStatus::Paused) {
            return;
        }

        // Check if it's time to execute
        QDateTime now = QDateTime::currentDateTime();
        if (now >= entry.info.nextExecuteTime) {
            qDebug() << "[TaskScheduler] Execute Cron task:" << entry.info.name;

            try {
                callback();
                entry.info.executeCount++;
                entry.info.lastExecuteTime = now;
                entry.info.nextExecuteTime = calculateNextCronTime(cronTime);

                emit taskExecuted(entry.info.id, entry.info.executeCount);
                qDebug() << "[TaskScheduler] Cron task done:" << entry.info.name
                         << "Next:" << entry.info.nextExecuteTime.toString("yyyy-MM-dd hh:mm:ss");

            } catch (const std::exception& e) {
                entry.info.status = TaskStatus::Failed;
                entry.info.lastError = e.what();
                qCritical() << "[TaskScheduler] Cron task failed:" << entry.info.id
                            << "Error:" << e.what();
                emit taskFailed(entry.info.id, entry.info.lastError);
            }
        }
    });

    entry.timer->start();
    m_activeTasks[info.id] = entry;
    m_taskNames[name] = info.id;

    return info.id;
}

// ========== Task Management ==========

bool TaskScheduler::stopTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[TaskScheduler] Task not found:" << taskId;
        return false;
    }

    TaskEntry& entry = m_activeTasks[taskId];
    entry.info.status = TaskStatus::Stopped;

    if (entry.timer) {
        entry.timer->stop();
        entry.timer->deleteLater();
        entry.timer = nullptr;
    }

    if (!entry.info.name.isEmpty()) {
        m_taskNames.remove(entry.info.name);
    }

    qDebug() << "[TaskScheduler] Stopped task:" << entry.info.name << "ID:" << taskId;
    emit taskStopped(taskId);

    return true;
}

bool TaskScheduler::stopTaskByName(const QString& name)
{
    if (!m_taskNames.contains(name)) {
        qWarning() << "[TaskScheduler] Task not found:" << name;
        return false;
    }

    QString taskId = m_taskNames[name];
    return stopTask(taskId);
}

bool TaskScheduler::pauseTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_activeTasks.contains(taskId)) {
        return false;
    }

    TaskEntry& entry = m_activeTasks[taskId];
    if (entry.info.status == TaskStatus::Running) {
        entry.info.status = TaskStatus::Paused;
        qDebug() << "[TaskScheduler] Paused task:" << entry.info.name;
        emit taskPaused(taskId);
        return true;
    }

    return false;
}

bool TaskScheduler::resumeTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_activeTasks.contains(taskId)) {
        return false;
    }

    TaskEntry& entry = m_activeTasks[taskId];
    if (entry.info.status == TaskStatus::Paused) {
        entry.info.status = TaskStatus::Running;
        qDebug() << "[TaskScheduler] Resumed task:" << entry.info.name;
        emit taskResumed(taskId);
        return true;
    }

    return false;
}

void TaskScheduler::stopAllTasks()
{
    QMutexLocker locker(&m_mutex);

    QList<QString> taskIds = m_activeTasks.keys();
    for (const auto& id : taskIds) {
        stopTask(id);
    }
    qDebug() << "[TaskScheduler] Stopped all tasks";
}

TaskInfo TaskScheduler::getTaskInfo(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);

    if (m_activeTasks.contains(taskId)) {
        return m_activeTasks[taskId].info;
    }
    return TaskInfo();
}

QList<TaskInfo> TaskScheduler::getAllTasks() const
{
    QMutexLocker locker(&m_mutex);

    QList<TaskInfo> result;
    for (const auto& entry : m_activeTasks) {
        result.append(entry.info);
    }
    return result;
}

QList<TaskInfo> TaskScheduler::getTaskHistory(int count) const
{
    QMutexLocker locker(&m_mutex);

    if (count >= m_taskHistory.size()) {
        return m_taskHistory;
    }
    return m_taskHistory.mid(m_taskHistory.size() - count);
}

bool TaskScheduler::hasTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_activeTasks.contains(taskId);
}

bool TaskScheduler::isTaskRunning(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_activeTasks.contains(taskId)) {
        return false;
    }
    return m_activeTasks[taskId].info.status == TaskStatus::Running;
}

TaskStatistics TaskScheduler::getStatistics() const
{
    QMutexLocker locker(&m_mutex);

    TaskStatistics stats;

    for (const auto& entry : m_activeTasks) {
        stats.totalTasks++;
        switch (entry.info.status) {
            case TaskStatus::Running:   stats.runningTasks++; break;
            case TaskStatus::Paused:    stats.pausedTasks++; break;
            case TaskStatus::Stopped:   stats.stoppedTasks++; break;
            case TaskStatus::Completed: stats.completedTasks++; break;
            case TaskStatus::Failed:    stats.failedTasks++; break;
            default: break;
        }
        stats.totalExecutions += entry.info.executeCount;
    }

    return stats;
}

// ========== Private Methods ==========

TaskInfo TaskScheduler::createTaskInfo(const QString& name, TaskType type)
{
    TaskInfo info;
    info.id = QString("task_%1").arg(m_nextTaskId++);
    info.name = name.isEmpty() ? QString("Task_%1").arg(info.id) : name;
    info.type = type;
    info.status = TaskStatus::Idle;
    info.intervalMs = 0;
    info.executeCount = 0;
    info.maxExecuteCount = -1;
    info.createTime = QDateTime::currentDateTime();
    info.lastExecuteTime = QDateTime();
    info.nextExecuteTime = QDateTime();
    info.lastError = "";
    info.autoDelete = true;
    return info;
}

bool TaskScheduler::parseCronExpression(const QString& expr, CronTime& out)
{
    QStringList parts = expr.split(' ', QString::SkipEmptyParts);
    if (parts.size() != 5) {
        qWarning() << "[Cron] Invalid format, need 5 parts:" << expr;
        return false;
    }

    out.minute = parseCronField(parts[0], 0, 59);
    out.hour = parseCronField(parts[1], 0, 23);
    out.day = parseCronField(parts[2], 1, 31);
    out.month = parseCronField(parts[3], 1, 12);
    out.weekday = parseCronField(parts[4], 0, 6);
    out.valid = true;

    return true;
}

int TaskScheduler::parseCronField(const QString& field, int min, int max)
{
    if (field == "*") {
        return -1;
    }

    // Step expression: "*/10"
    if (field.startsWith("*/")) {
        int step = field.mid(2).toInt();
        if (step <= 0) return -1;
        return min;
    }

    // Range expression: "1-5"
    if (field.contains('-')) {
        QStringList range = field.split('-');
        if (range.size() == 2) {
            return range[0].toInt();
        }
    }

    // List expression: "1,3,5"
    if (field.contains(',')) {
        QStringList values = field.split(',');
        if (!values.isEmpty()) {
            return values[0].toInt();
        }
    }

    // Single number
    int value = field.toInt();
    if (value >= min && value <= max) {
        return value;
    }

    return -1;
}

QDateTime TaskScheduler::calculateNextCronTime(const CronTime& cronTime)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime next = now.addSecs(60);

    // Search for next matching time (max 1 year)
    for (int i = 0; i < 365 * 24 * 60; ++i) {
        QDateTime check = next.addSecs(i * 60);

        int minute = check.time().minute();
        int hour = check.time().hour();
        int day = check.date().day();
        int month = check.date().month();
        int weekday = check.date().dayOfWeek() % 7;

        bool match = true;
        if (cronTime.minute >= 0 && cronTime.minute != minute) match = false;
        if (cronTime.hour >= 0 && cronTime.hour != hour) match = false;
        if (cronTime.day >= 0 && cronTime.day != day) match = false;
        if (cronTime.month >= 0 && cronTime.month != month) match = false;
        if (cronTime.weekday >= 0 && cronTime.weekday != weekday) match = false;

        if (match) {
            return check;
        }
    }

    return now.addSecs(3600);
}

void TaskScheduler::cleanupCompletedTasks()
{
    QMutexLocker locker(&m_mutex);

    QList<QString> toRemove;

    for (auto it = m_activeTasks.begin(); it != m_activeTasks.end(); ++it) {
        TaskStatus status = it->info.status;
        TaskType type = it->info.type;

        if ((status == TaskStatus::Completed ||
             status == TaskStatus::Stopped ||
             status == TaskStatus::Failed) &&
            (type == TaskType::Once)) {
            toRemove.append(it.key());
        }
    }

    for (const auto& id : toRemove) {
        m_activeTasks.remove(id);
        qDebug() << "[TaskScheduler] Cleaned up task:" << id;
    }
}

bool TaskScheduler::loadConfig(TaskInfo& info)
{
    Q_UNUSED(info);
    return true;
}
}
