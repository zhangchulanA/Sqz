#ifndef SQZDBMGR_H
#define SQZDBMGR_H

/**
 * ===================================================================
 * DBManager - Qt 数据库管理类（企业级严谨风格）
 * ===================================================================
 * 功能：连接池、零SQL CRUD、事务、JSON互转、分页、批量操作
 * 命名：全称函数名，结构体用完整单词，枚举值带注释
 * ===================================================================
 */

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QMap>
#include <QVector>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <functional>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "SqzGlobal.h"

namespace Sqz {

class SQZ_FRAMEWORK_API DBManager;
struct SQZ_FRAMEWORK_API Condition;
// ---- 类型别名：让初始化列表更简洁 ----
using CondList = std::initializer_list<Condition>;

// ---- RAII 连接守卫：自动借出和归还 ----
class SQZ_FRAMEWORK_API ConnectionGuard {
public:
    explicit ConnectionGuard(DBManager& mgr);   // 构造时借出连接
    ~ConnectionGuard();                          // 析构时自动归还
    QSqlDatabase& database();                    // 获取数据库连接引用
    bool isValid() const;                        // 检查连接是否有效

    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;

private:
    DBManager& m_manager;
    QSqlDatabase m_database;
};

// ---- 数据库配置结构（链式调用） ----
struct SQZ_FRAMEWORK_API Configuration {
    QString type = "QSQLITE";        // 驱动类型
    QString host = "localhost";      // 主机地址
    int port = 0;                    // 端口（0表示默认）
    QString databaseName;            // 数据库名（SQLite为文件路径）
    QString userName;                // 用户名
    QString password;                // 密码
    QString connectionName;          // 自定义连接名前缀（可选）

    Configuration& setType(const QString& t)            { type = t; return *this; }
    Configuration& setHost(const QString& h)            { host = h; return *this; }
    Configuration& setPort(int p)                       { port = p; return *this; }
    Configuration& setDatabaseName(const QString& name) { databaseName = name; return *this; }
    Configuration& setUserName(const QString& u)        { userName = u; return *this; }
    Configuration& setPassword(const QString& p)        { password = p; return *this; }
    Configuration& setConnectionName(const QString& n)  { connectionName = n; return *this; }
};

// ---- 查询条件结构体 ----
struct Condition {
    // 运算符枚举（每个值附含义注释）
    enum Operator {
        Equal,            // 等于 (=)
        NotEqual,         // 不等于 (!=)
        Greater,          // 大于 (>)
        Less,             // 小于 (<)
        GreaterOrEqual,   // 大于等于 (>=)
        LessOrEqual,      // 小于等于 (<=)
        Like,             // 模糊匹配 (LIKE)
        In                // 包含于 (IN)
    };

    QString field;          // 字段名
    QVariant value;         // 比较值（IN 时为 QVariantList）
    Operator op;            // 运算符
    QString customOperator; // 自定义运算符（非空时覆盖 op）

    Condition() : op(Equal) {}
    Condition(const QString& f, const QVariant& v, Operator o = Equal)
        : field(f), value(v), op(o) {}
    Condition(const QString& f, const QVariant& v, const QString& custom)
        : field(f), value(v), op(Equal), customOperator(custom) {}
};

// ---- 数据库管理器主类 ----
class DBManager : public QObject {
    Q_OBJECT

public:
    // ========== 实例管理 ==========
    static DBManager& instance();                    // 获取默认实例 ("default")
    static DBManager& instance(const QString& name); // 获取命名实例
    static void destroyInstance(const QString& name); // 销毁指定实例
    static void destroyAll();                        // 销毁所有实例

    // ========== 配置 ==========
    void configure(const Configuration& config);     // 配置数据库连接参数
    QString lastError() const;                       // 获取最后一次错误信息

    // ========== 连接池（一般用 ConnectionGuard） ==========
    QSqlDatabase getConnection();                    // 从池中借出一个连接
    void releaseConnection(const QSqlDatabase& db);  // 归还连接到池中

    // ========== 健康检查 ==========
    bool ping();                                     // 检测所有连接是否存活
    void setAutoReconnect(bool enabled);             // 开启/关闭自动重连
    bool autoReconnect() const;                      // 获取自动重连状态

    // ========== 底层 SQL 执行 ==========
    QVector<QVariantMap> executeQuery(const QString& sql, const QMap<QString, QVariant>& bindings = {});  // 执行查询，返回结果集
    int executeNonQuery(const QString& sql, const QMap<QString, QVariant>& bindings = {});                // 执行非查询，返回影响行数
    QVariant executeInsert(const QString& sql, const QMap<QString, QVariant>& bindings = {});             // 执行插入，返回自增ID

    // ========== 事务 ==========
    bool executeTransaction(std::function<bool(QSqlDatabase& db)> func);  // 执行事务，lambda返回true提交，false回滚

    // ========== CRUD - QVariantMap 版本（简单等值条件） ==========
    bool insertRecord(const QString& table, const QVariantMap& values);   // 插入一条记录
    QVector<QVariantMap> selectRecords(const QString& table, const QStringList& fields = {"*"}, const QVariantMap& where = {});  // 查询记录
    bool updateRecords(const QString& table, const QVariantMap& values, const QVariantMap& where);  // 更新记录
    bool deleteRecords(const QString& table, const QVariantMap& where);   // 删除记录
    int countRecords(const QString& table, const QVariantMap& where = {}); // 统计记录数
    bool recordExists(const QString& table, const QVariantMap& where);    // 检查记录是否存在
    bool createTable(const QString& table, const QMap<QString, QString>& columns);  // 创建表
    bool dropTable(const QString& table);                                  // 删除表

    // ========== CRUD - CondList 版本（灵活条件，用花括号传参） ==========
    QVector<QVariantMap> selectRecords(const QString& table, const QStringList& fields, CondList conditions, const QString& orderBy = "");  // 灵活条件查询
    bool updateRecords(const QString& table, const QVariantMap& values, CondList conditions);  // 灵活条件更新
    bool deleteRecords(const QString& table, CondList conditions);        // 灵活条件删除
    int countRecords(const QString& table, CondList conditions);          // 灵活条件统计
    bool recordExists(const QString& table, CondList conditions);         // 灵活条件存在检查

    // ========== CRUD - QVector<Condition> 版本（可复用条件） ==========
    QVector<QVariantMap> selectRecords(const QString& table, const QStringList& fields, const QVector<Condition>& conditions, const QString& orderBy = "");  // 可复用条件查询
    bool updateRecords(const QString& table, const QVariantMap& values, const QVector<Condition>& conditions);  // 可复用条件更新
    bool deleteRecords(const QString& table, const QVector<Condition>& conditions);  // 可复用条件删除
    int countRecords(const QString& table, const QVector<Condition>& conditions);    // 可复用条件统计
    bool recordExists(const QString& table, const QVector<Condition>& conditions);   // 可复用条件存在检查

    // ========== JSON 互转 ==========
    QJsonArray toJson(const QVector<QVariantMap>& rows) const;            // 查询结果转JSON数组
    bool insertRecordFromJson(const QString& table, const QJsonObject& jsonObject);  // 从JSON对象插入记录
    int insertBatchFromJson(const QString& table, const QJsonArray& jsonArray);      // 从JSON数组批量插入

    // ========== 分页查询 ==========
    QVariantMap selectPage(const QString& table, int page, int pageSize, const QStringList& fields = {"*"}, const QVector<Condition>& where = {}, const QString& orderBy = "");  // 分页查询

    // ========== 批量插入 ==========
    int insertBatch(const QString& table, const QVector<QVariantMap>& valuesList);  // 批量插入（事务内）

private:
    explicit DBManager(const QString& name);
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    // 内部辅助函数
    QSqlDatabase createConnection(const QString& connectionName);           // 创建单个连接
    bool bindValuesToQuery(QSqlQuery& query, const QMap<QString, QVariant>& bindings);  // 绑定参数到查询
    QString buildWhereClause(const QVariantMap& where, QMap<QString, QVariant>& outBindings);  // 构建WHERE子句（QVariantMap）
    QString buildWhereClause(const QVector<Condition>& where, QMap<QString, QVariant>& outBindings);  // 构建WHERE子句（Condition）
    void cleanup();                                                         // 清理所有连接

    // 成员变量
    QString m_instanceName;          // 实例名称
    Configuration m_config;          // 配置信息
    QString m_baseConnectionName;    // 基础连接名
    QMutex m_mutex;                  // 互斥锁（线程安全）
    QQueue<QString> m_connectionPool;// 连接池队列
    QString m_lastError;             // 最后错误信息
    bool m_configured = false;       // 是否已配置
    bool m_autoReconnect = false;    // 是否自动重连

    static const int MAX_POOL_SIZE = 10;                   // 池大小
    static QMap<QString, DBManager*> s_instances;          // 所有实例
    static QMutex s_instanceMutex;                         // 实例管理互斥锁
};

// ---- ConnectionGuard 内联实现 ----
inline ConnectionGuard::ConnectionGuard(DBManager& mgr)
    : m_manager(mgr), m_database(mgr.getConnection()) {}

inline ConnectionGuard::~ConnectionGuard() {
    if (m_database.isValid()) {
        m_manager.releaseConnection(m_database);
    }
}

inline QSqlDatabase& ConnectionGuard::database() { return m_database; }
inline bool ConnectionGuard::isValid() const { return m_database.isValid(); }

} // namespace Sqz
#endif // SQZDBMGR_H
