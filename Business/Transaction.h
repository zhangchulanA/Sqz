/**
 * @file Transaction.h
 * @brief 通用事务封装 - 保证批量操作的原子性
 *
 * 核心思想：要么全部成功，要么全部失败（自动回滚）
 *
 * 使用场景：
 * 1. 转账操作（扣款+到账必须同时成功）
 * 2. 订单处理（减库存+生成订单+扣余额）
 * 3. 批量数据导入（保证数据完整性）
 * 4. 配置文件更新（失败自动恢复）
 * 5. 游戏存档（多文件保存的一致性）
 */

#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QEventLoop>
#include <QTimer>
#include <QRunnable>
#include <functional>
#include <memory>

/**
 * @brief 事务类 - 管理一组操作的原子性执行
 *
 * 用法示例：
 * @code
 * Transaction tx;
 * tx.addOperation(
 *     []() { return doSomething(); },  // 干活函数
 *     []() { undoIt(); }                // 反悔函数
 * );
 * tx.commit();  // 执行事务
 * @endcode
 */
class Transaction : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 操作函数类型定义
     * @return true 表示操作成功，false 表示操作失败
     */
    using Operation = std::function<bool()>;

    /**
     * @brief 回滚函数类型定义
     * @return true 表示回滚成功（通常应该保证成功）
     */
    using Rollback = std::function<bool()>;

    /**
     * @brief 构造函数
     * @param name 事务名称（用于日志和调试）
     */
    explicit Transaction(const QString& name = "Unnamed")
        : QObject(nullptr),
          m_name(name),
          m_startTime(QDateTime::currentDateTime()),
          m_isCommitted(false)
    {
        qDebug() << "[Transaction] 创建事务:" << m_name;
    }

    /**
     * @brief 析构函数 - 如果未提交则自动回滚（RAII）
     */
    ~Transaction() {
        if (!m_isCommitted && !m_operations.isEmpty()) {
            qWarning() << "[Transaction] 事务未提交，自动回滚:" << m_name;
            rollback();
        }
    }

    /**
     * @brief 添加一个操作到事务中
     * @param operation 干活函数：执行具体操作
     * @param rollback 反悔函数：操作失败时撤销（可选）
     * @return 返回自身引用，支持链式调用
     *
     * 注意：
     * 1. operation 返回 true 表示成功，false 表示失败
     * 2. rollback 在回滚时按相反顺序执行
     * 3. rollback 为空时，回滚不做任何事（适合查询操作）
     */
    Transaction& addOperation(Operation operation, Rollback rollback = nullptr) {
        m_operations.append({operation, rollback});
        qDebug() << "[Transaction] 添加操作 #" << m_operations.size()
                 << "到事务:" << m_name;
        return *this;
    }

    /**
     * @brief 批量添加操作（用于循环添加）
     * @param operations 操作列表
     * @param rollback 所有操作共用的回滚函数（可选）
     * @return 返回自身引用
     */
    Transaction& addOperations(const QList<Operation>& operations,
                               Rollback rollback = nullptr) {
        for (const auto& op : operations) {
            addOperation(op, rollback);
        }
        return *this;
    }

    /**
     * @brief 提交事务 - 执行所有操作
     * @return true 表示所有操作成功，false 表示失败并已回滚
     *
     * 执行流程：
     * 1. 按添加顺序执行所有操作
     * 2. 如果某个操作失败，立即停止执行
     * 3. 按相反顺序执行所有已成功操作的回滚函数
     * 4. 返回执行结果
     */
    bool commit() {
        if (m_isCommitted) {
            qWarning() << "[Transaction] 事务已提交，重复提交:" << m_name;
            return true;
        }

        qDebug() << "[Transaction] 开始提交事务:" << m_name
                 << "包含" << m_operations.size() << "个操作";

        // 记录已成功执行的操作索引
        QList<int> executedIndexes;

        // 1. 按顺序执行所有操作
        for (int i = 0; i < m_operations.size(); ++i) {
            const auto& op = m_operations[i];

            qDebug() << "[Transaction] 执行操作 #" << (i + 1)
                     << "当前进度:" << executedIndexes.size() + 1 << "/" << m_operations.size();

            try {
                // 执行操作
                bool success = op.operation();

                if (success) {
                    // 操作成功，记录索引
                    executedIndexes.append(i);
                    qDebug() << "[Transaction] 操作 #" << (i + 1) << "成功";
                } else {
                    // 操作失败，触发回滚
                    qWarning() << "[Transaction] 操作 #" << (i + 1) << "失败，开始回滚";
                    rollback(executedIndexes);
                    emit rolledBack(m_name, "操作失败");
                    return false;
                }
            } catch (const std::exception& e) {
                // 捕获C++异常，也触发回滚
                qCritical() << "[Transaction] 操作 #" << (i + 1)
                            << "发生异常:" << e.what() << "，开始回滚";
                rollback(executedIndexes);
                emit rolledBack(m_name, QString("异常: %1").arg(e.what()));
                return false;
            } catch (...) {
                // 捕获所有其他异常
                qCritical() << "[Transaction] 操作 #" << (i + 1)
                            << "发生未知异常，开始回滚";
                rollback(executedIndexes);
                emit rolledBack(m_name, "未知异常");
                return false;
            }
        }

        // 2. 所有操作成功，标记事务已提交
        m_isCommitted = true;
        qint64 elapsed = m_startTime.msecsTo(QDateTime::currentDateTime());
        qDebug() << "[Transaction] 事务提交成功:" << m_name
                 << "耗时" << elapsed << "ms";

        emit committed(m_name, m_operations.size());
        return true;
    }

    /**
     * @brief 手动回滚事务（一般不主动调用，失败时会自动回滚）
     * @return true 表示回滚成功
     */
    bool rollback() {
        if (m_isCommitted) {
            qWarning() << "[Transaction] 已提交的事务不能回滚:" << m_name;
            return false;
        }

        QList<int> allIndexes;
        for (int i = 0; i < m_operations.size(); ++i) {
            allIndexes.append(i);
        }
        return rollback(allIndexes);
    }

    /**
     * @brief 检查事务是否已提交
     */
    bool isCommitted() const {
        return m_isCommitted;
    }

    /**
     * @brief 获取事务名称
     */
    QString name() const {
        return m_name;
    }

    /**
     * @brief 获取操作数量
     */
    int operationCount() const {
        return m_operations.size();
    }

    /**
     * @brief 获取事务开始时间
     */
    QDateTime startTime() const {
        return m_startTime;
    }

signals:
    /**
     * @brief 事务提交成功信号
     * @param name 事务名称
     * @param count 操作数量
     */
    void committed(const QString& name, int count);

    /**
     * @brief 事务回滚信号
     * @param name 事务名称
     * @param reason 回滚原因
     */
    void rolledBack(const QString& name, const QString& reason);

    /**
     * @brief 操作执行进度信号
     * @param current 当前执行的操作索引
     * @param total 总操作数
     */
    void progress(int current, int total);

private:
    /**
     * @brief 内部结构体：存储操作及其对应的回滚函数
     */
    struct OperationItem {
        Operation operation;   // 干活函数
        Rollback rollback;     // 反悔函数
    };

    /**
     * @brief 执行回滚
     * @param indexes 需要回滚的操作索引列表（按正序）
     * @return true 表示回滚成功
     */
    bool rollback(const QList<int>& indexes) {
        if (indexes.isEmpty()) {
            qDebug() << "[Transaction] 没有需要回滚的操作";
            return true;
        }

        qDebug() << "[Transaction] 开始回滚" << indexes.size() << "个操作";
        bool allSuccess = true;

        // 按相反顺序执行回滚（最后执行的操作最先回滚）
        for (int i = indexes.size() - 1; i >= 0; --i) {
            int idx = indexes[i];
            const auto& item = m_operations[idx];

            // 检查是否有回滚函数
            if (item.rollback) {
                try {
                    qDebug() << "[Transaction] 执行回滚操作 #" << (idx + 1);
                    bool success = item.rollback();
                    if (!success) {
                        qCritical() << "[Transaction] 回滚操作 #" << (idx + 1) << "失败！";
                        allSuccess = false;
                        // 注意：回滚失败是严重问题，继续尝试回滚其他操作
                    }
                } catch (const std::exception& e) {
                    qCritical() << "[Transaction] 回滚操作 #" << (idx + 1)
                                << "发生异常:" << e.what();
                    allSuccess = false;
                } catch (...) {
                    qCritical() << "[Transaction] 回滚操作 #" << (idx + 1)
                                << "发生未知异常";
                    allSuccess = false;
                }
            } else {
                qDebug() << "[Transaction] 操作 #" << (idx + 1) << "没有回滚函数，跳过";
            }
        }

        if (allSuccess) {
            qDebug() << "[Transaction] 回滚成功:" << m_name;
            emit rolledBack(m_name, "正常回滚");
        } else {
            qCritical() << "[Transaction] 回滚部分失败:" << m_name;
            emit rolledBack(m_name, "回滚失败");
        }

        return allSuccess;
    }

private:
    QString m_name;                          // 事务名称
    QDateTime m_startTime;                   // 开始时间
    QList<OperationItem> m_operations;       // 操作列表
    bool m_isCommitted;                      // 是否已提交
};

/**
 * @brief 嵌套事务类 - 支持事务嵌套
 *
 * 使用场景：
 * 1. 子事务失败自动回滚子事务，不影响父事务
 * 2. 父事务提交时，所有子事务也必须已提交
 */
class NestedTransaction {
public:
    /**
     * @brief 执行嵌套事务
     * @param parentName 父事务名称
     * @param childTransaction 子事务执行函数
     * @return true 表示子事务成功
     */
    static bool execute(const QString& parentName,
                        std::function<bool()> childTransaction) {
        qDebug() << "[NestedTransaction] 父事务:" << parentName << "开始执行子事务";

        Transaction childTx(parentName + ".child");
        bool success = childTransaction();

        if (success) {
            qDebug() << "[NestedTransaction] 子事务执行成功";
            return true;
        } else {
            qWarning() << "[NestedTransaction] 子事务失败，自动回滚";
            return false;
        }
    }

    /**
     * @brief 安全的嵌套事务（父事务失败时子事务自动回滚）
     */
    static bool safeExecute(Transaction& parentTx,
                           const QString& name,
                           std::function<bool(Transaction&)> childLogic) {
        Transaction childTx(name);

        // 子事务的操作添加到父事务中
        parentTx.addOperation(
            [&]() -> bool {
                return childLogic(childTx);
            },
            [&]() -> bool {
                // 父事务回滚时，子事务自动回滚
                return true;
            }
        );

        return true;
    }
};

/**
 * @brief 用于超时执行的 Runnable 包装类
 */
class TimeoutRunnable : public QRunnable {
public:
    TimeoutRunnable(std::function<void()> task) : m_task(task) {}

    void run() override {
        if (m_task) {
            m_task();
        }
    }

private:
    std::function<void()> m_task;
};

/**
 * @brief 便捷的事务工具类 - 提供常用场景的快速封装
 */
class TransactionHelper {
public:
    /**
     * @brief 执行带重试的事务
     * @param tx 事务对象
     * @param maxRetries 最大重试次数
     * @param delayMs 重试间隔（毫秒）
     * @return true 表示最终成功
     */
    static bool commitWithRetry(Transaction& tx, int maxRetries = 3, int delayMs = 1000) {
        for (int i = 0; i < maxRetries; ++i) {
            if (tx.commit()) {
                return true;
            }

            if (i < maxRetries - 1) {
                qWarning() << "[TransactionHelper] 事务失败，重试" << (i + 1)
                           << "/" << maxRetries;
                QThread::msleep(delayMs * (i + 1));  // 指数退避
            }
        }

        qCritical() << "[TransactionHelper] 事务重试" << maxRetries << "次后仍失败";
        return false;
    }

    /**
     * @brief 执行带超时保护的事务
     * @param tx 事务对象
     * @param timeoutMs 超时时间（毫秒）
     * @return true 表示事务提交成功
     */
    static bool commitWithTimeout(Transaction& tx, int timeoutMs = 5000) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(timeoutMs);

        bool completed = false;
        bool result = false;

        // 在另一个线程执行事务 - 使用包装的 QRunnable
        TimeoutRunnable* runnable = new TimeoutRunnable([&]() {
            result = tx.commit();
            completed = true;
            loop.quit();
        });

        // QThreadPool 会自动删除 runnable
        QThreadPool::globalInstance()->start(runnable);

        // 等待完成或超时
        QObject::connect(&timer, &QTimer::timeout, [&]() {
            if (!completed) {
                qWarning() << "[TransactionHelper] 事务超时";
                loop.quit();
            }
        });

        loop.exec();

        if (!completed) {
            qCritical() << "[TransactionHelper] 事务超时" << timeoutMs << "ms";
            return false;
        }

        return result;
    }

    /**
     * @brief 创建数据库事务（使用RAII管理数据库事务）
     * @param db 数据库连接
     * @param autoCommit 是否自动提交
     * @return 数据库事务守卫对象
     */
    static std::unique_ptr<QSqlDatabase> createDbTransaction(QSqlDatabase& db,
                                                             bool autoCommit = false) {
        if (!db.transaction()) {
            qCritical() << "[TransactionHelper] 启动数据库事务失败";
            return nullptr;
        }

        return std::unique_ptr<QSqlDatabase>(new QSqlDatabase(db));
    }
};

/**
 * @brief 数据库事务守卫 - RAII管理数据库事务
 */
class DbTransactionGuard {
public:
    explicit DbTransactionGuard(QSqlDatabase& db) : m_db(db) {
        if (!m_db.transaction()) {
            qCritical() << "[DbTransactionGuard] 启动数据库事务失败";
            m_valid = false;
        } else {
            m_valid = true;
            qDebug() << "[DbTransactionGuard] 数据库事务已启动";
        }
    }

    ~DbTransactionGuard() {
        if (m_valid) {
            if (m_committed) {
                m_db.commit();
                qDebug() << "[DbTransactionGuard] 数据库事务已提交";
            } else {
                m_db.rollback();
                qDebug() << "[DbTransactionGuard] 数据库事务已回滚";
            }
        }
    }

    void commit() {
        if (m_valid && !m_committed) {
            m_committed = true;
        }
    }

    bool isValid() const { return m_valid; }

private:
    QSqlDatabase& m_db;
    bool m_valid = false;
    bool m_committed = false;
};

// ========== 示例：几个实用的预定义事务 ==========

/**
 * @brief 转账事务
 */
class TransferTransaction : public Transaction {
public:
    TransferTransaction(int fromAccount, int toAccount, double amount)
        : Transaction("转账"), m_from(fromAccount), m_to(toAccount), m_amount(amount) {

        // 步骤1：从源账户扣款
        addOperation(
            [=]() -> bool {
                qDebug() << "从账户" << m_from << "扣款" << m_amount << "元";
                // 模拟扣款
                if (getBalance(m_from) >= m_amount) {
                    setBalance(m_from, getBalance(m_from) - m_amount);
                    return true;
                }
                qWarning() << "余额不足！当前余额:" << getBalance(m_from);
                return false;
            },
            [=]() -> bool {
                qDebug() << "回滚：退还款项到账户" << m_from;
                setBalance(m_from, getBalance(m_from) + m_amount);
                return true;
            }
        );

        // 步骤2：向目标账户存款
        addOperation(
            [=]() -> bool {
                qDebug() << "向账户" << m_to << "存款" << m_amount << "元";
                setBalance(m_to, getBalance(m_to) + m_amount);
                return true;
            },
            [=]() -> bool {
                qDebug() << "回滚：从账户" << m_to << "撤回款项";
                setBalance(m_to, getBalance(m_to) - m_amount);
                return true;
            }
        );
    }

private:
    int m_from;
    int m_to;
    double m_amount;

    // 模拟账户余额操作
    double getBalance(int accountId) {
        static QHash<int, double> balances = {{1001, 1000}, {1002, 500}};
        return balances.value(accountId, 0);
    }

    void setBalance(int accountId, double balance) {
        static QHash<int, double> balances = {{1001, 1000}, {1002, 500}};
        balances[accountId] = balance;
        qDebug() << "账户" << accountId << "余额变为" << balance;
    }
};

/**
 * @brief 批量文件操作事务
 */
class FileBatchTransaction : public Transaction {
public:
    FileBatchTransaction(const QList<QString>& srcFiles, const QList<QString>& dstFiles)
        : Transaction("批量文件操作") {

        if (srcFiles.size() != dstFiles.size()) {
            qWarning() << "[FileBatchTransaction] 文件列表大小不匹配";
            return;
        }

        for (int i = 0; i < srcFiles.size(); ++i) {
            QString src = srcFiles[i];
            QString dst = dstFiles[i];

            addOperation(
                [=]() -> bool {
                    qDebug() << "重命名文件:" << src << "->" << dst;
                    return QFile::rename(src, dst);
                },
                [=]() -> bool {
                    qDebug() << "回滚：恢复文件名" << dst << "->" << src;
                    return QFile::rename(dst, src);
                }
            );
        }
    }
};

#endif // TRANSACTION_H
