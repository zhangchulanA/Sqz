#ifndef SQZDBMGR_H
#define SQZDBMGR_H

/**
 * ===================================================================
 * SqzDbMgr - Qt 数据库管理类（多实例 + 连接池 + 增强功能）
 * ===================================================================
 *
 * 【核心功能】
 * 1. 零SQL增删改查：insert()、select()、update()、deleteRow()
 * 2. 多数据库实例：通过 instance("name") 管理多个独立数据库
 * 3. 连接池管理：预创建连接，借出归还，避免频繁开关数据库
 * 4. 自动事务：transaction(lambda) 自动提交/回滚
 * 5. 链式配置：DatabaseConfig().setType("QSQLITE").setDatabaseName("xxx.db")
 * 6. RAII连接：ScopedConnection 自动归还连接，任何return路径都安全
 *
 * 【新增增强功能】
 * 1. JSON 互转：toJson() 将查询结果转为 JSON 数组，insertFromJson() 从 JSON 插入
 * 2. 分页查询：selectPage() 自动分页，返回总条数 + 当前页数据
 * 3. 批量插入：insertBatch() 事务内批量插入，高性能
 * 4. 灵活条件：WhereCondition 支持 =, >, <, >=, <=, LIKE, IN 等，安全防注入
 * 5. 连接健康检查：ping() 检测连接是否存活，可配合自动重连策略
 *
 * 【类特点】
 * - 单例模式：每个name对应一个实例，全局唯一
 * - 连接池：启动时预创建10个连接（MAX_POOL_SIZE可改），用完归还，一直保持打开
 * - 线程安全：QMutex保护连接池，多线程可同时借还不同连接
 * - 防注入：所有SQL使用参数绑定，杜绝SQL注入
 * - 防误删：update()和deleteRow()强制要求where条件，防止全表操作
 * - RAII：ScopedConnection析构自动归还连接，避免泄漏
 * - 跨平台：Linux/Windows/macOS，SQLite/MySQL/PostgreSQL
 *
 * 【依赖】
 * .pro 文件添加：QT += sql （JSON功能不需要额外模块，QtCore已包含）
 *
 * @version 3.0
 * ===================================================================
 */

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QVariant>
#include <QMap>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QQueue>
#include <functional>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include "SqzGlobal.h"

namespace Sqz{

class SQZ_FRAMEWORK_API SqzDbMgr;

// ===================================================================
// ScopedConnection - 自动归还连接的RAII包装类
// ===================================================================
class SQZ_FRAMEWORK_API ScopedConnection {
public:
    explicit ScopedConnection(SqzDbMgr& mgr);
    ~ScopedConnection();
    QSqlDatabase& db();
    bool isValid() const;

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

private:
    SqzDbMgr& m_mgr;
    QSqlDatabase m_db;
};

// ===================================================================
// DatabaseConfig - 数据库配置（链式调用）
// ===================================================================
struct  DatabaseConfig {
    QString type = "QSQLITE";
    QString host = "localhost";
    int port = 0;
    QString databaseName;
    QString userName;
    QString password;
    QString connectionName;

    DatabaseConfig& setType(const QString& t)            { type = t; return *this; }
    DatabaseConfig& setHost(const QString& h)            { host = h; return *this; }
    DatabaseConfig& setPort(int p)                       { port = p; return *this; }
    DatabaseConfig& setDatabaseName(const QString& name) { databaseName = name; return *this; }
    DatabaseConfig& setUserName(const QString& u)        { userName = u; return *this; }
    DatabaseConfig& setPassword(const QString& p)        { password = p; return *this; }
    DatabaseConfig& setConnectionName(const QString& name) { connectionName = name; return *this; }
};

// ===================================================================
// WhereCondition - 灵活查询条件
// ===================================================================
/**
 * @brief 查询条件结构体，支持多种运算符
 *
 * 运算符类型：
 *   Equal         =  等于
 *   NotEqual      != 不等于
 *   Greater       >  大于
 *   Less          <  小于
 *   GreaterOrEqual >= 大于等于
 *   LessOrEqual   <= 小于等于
 *   Like          LIKE 模糊查询
 *   In            IN  包含查询
 *
 * 使用示例：
 *   WhereCondition("age", 25, Greater)                 // age > 25
 *   WhereCondition("name", "%张%", Like)               // name LIKE '%张%'
 *   WhereCondition("id", QVariantList{1,2,3}, In)     // id IN (1,2,3)
 *
 * 安全说明：值会被参数绑定，自动防注入
 */
struct SQZ_FRAMEWORK_API WhereCondition {
    enum Operator {
        Equal = 0,
        NotEqual,
        Greater,
        Less,
        GreaterOrEqual,
        LessOrEqual,
        Like,
        In
    };

    QString field;       ///< 字段名
    QVariant value;      ///< 值（In 时需要 QVariantList）
    Operator op;         ///< 运算符
    QString customOp;    ///< 自定义运算符（可选，覆盖 op）

    WhereCondition() : op(Equal) {}
    WhereCondition(const QString& f, const QVariant& v, Operator o = Equal)
        : field(f), value(v), op(o) {}
    WhereCondition(const QString& f, const QVariant& v, const QString& custom)
        : field(f), value(v), op(Equal), customOp(custom) {}
};

// ===================================================================
// SqzDbMgr - 数据库管理器
// ===================================================================
class SQZ_FRAMEWORK_API SqzDbMgr : public QObject
{
    Q_OBJECT

public:
    // ---------- 实例管理 ----------
    static SqzDbMgr& instance();
    static SqzDbMgr& instance(const QString& name);
    static void destroyInstance(const QString& name);
    static void destroyAll();

    // ---------- 配置 ----------
    void configure(const DatabaseConfig& config);
    QString lastError() const;

    // ---------- 连接池（一般用 ScopedConnection） ----------
    QSqlDatabase getConnection();
    void releaseConnection(const QSqlDatabase& db);

    // ---------- 健康检查 ----------
    /**
     * @brief 检测当前实例的所有连接是否存活
     * @return true表示所有连接正常
     * @note 内部对所有池中连接执行 SELECT 1，失败则尝试重连
     */
    bool ping();

    /**
     * @brief 设置自动重连标志（默认关闭）
     * 开启后，每次 getConnection() 会先检查连接，失效则自动重连
     */
    void setAutoReconnect(bool enabled);
    bool autoReconnect() const;

    // ---------- 底层 SQL ----------
    QVector<QVariantMap> executeQuery(const QString& sql,
                                      const QMap<QString, QVariant>& bindings = {});
    int executeNonQuery(const QString& sql,
                        const QMap<QString, QVariant>& bindings = {});
    QVariant executeInsert(const QString& sql,
                           const QMap<QString, QVariant>& bindings = {});

    // ---------- 事务 ----------
    bool transaction(std::function<bool(QSqlDatabase& db)> func);

    // ---------- 零SQL业务接口 ----------
    bool insert(const QString& table, const QVariantMap& values);
    QVector<QVariantMap> select(const QString& table,
                                const QStringList& fields = {"*"},
                                const QVariantMap& where = {});
    bool update(const QString& table,
                const QVariantMap& values,
                const QVariantMap& where);
    bool deleteRow(const QString& table, const QVariantMap& where);
    int count(const QString& table, const QVariantMap& where = {});
    bool exists(const QString& table, const QVariantMap& where);
    bool createTable(const QString& table, const QMap<QString, QString>& columns);
    bool dropTable(const QString& table);

    // ========== 新增功能 1：JSON 互转 ==========

    /**
     * @brief 将查询结果转换为 JSON 数组
     * @param rows select() 返回的格式
     * @return QJsonArray，可直接用于网络通信
     */
    QJsonArray toJson(const QVector<QVariantMap>& rows) const;

    /**
     * @brief 从 JSON 对象插入一条记录
     * @param table 表名
     * @param jsonObj JSON 对象，字段名对应列名
     * @return true表示成功
     * @note JSON 中的字段如果表中不存在，会被自动忽略
     */
    bool insertFromJson(const QString& table, const QJsonObject& jsonObj);

    /**
     * @brief 从 JSON 数组批量插入（自动事务）
     * @param table 表名
     * @param jsonArray JSON 对象数组
     * @return 成功插入的数量，-1表示整体失败
     */
    int insertBatchFromJson(const QString& table, const QJsonArray& jsonArray);

    // ========== 新增功能 2：分页查询 ==========

    /**
     * @brief 分页查询，同时返回总记录数和当前页数据
     * @param table 表名
     * @param page 页码（从1开始）
     * @param pageSize 每页条数
     * @param fields 查询字段
     * @param where 条件（灵活条件列表）
     * @param orderBy 排序，如 "id DESC"
     * @return QVariantMap 包含 "total" (int) 和 "rows" (QVariantList/QVector<QVariantMap>)
     *         调用示例：auto page = selectPage("users", 1, 10, {"*"}, {}, "id DESC");
     *                   int total = page["total"].toInt();
     *                   auto rows = page["rows"].value<QVector<QVariantMap>>();
     */
    QVariantMap selectPage(const QString& table,
                           int page,
                           int pageSize,
                           const QStringList& fields = {"*"},
                           const QVector<WhereCondition>& where = {},
                           const QString& orderBy = "");

    // ========== 新增功能 3：批量插入 ==========

    /**
     * @brief 批量插入（内部使用事务，全部成功或全部回滚）
     * @param table 表名
     * @param valuesList 多条记录的键值对列表
     * @return 成功插入的数量，-1表示失败
     * @note 字段取第一条的键，后续必须一致
     */
    int insertBatch(const QString& table, const QVector<QVariantMap>& valuesList);

    // ========== 新增功能 4：灵活条件查询 ==========

    /**
     * @brief 使用灵活条件查询数据
     * @param table 表名
     * @param fields 字段列表
     * @param where 条件列表（支持 =, >, <, LIKE, IN 等）
     * @param orderBy 排序，如 "name ASC"
     * @return 结果集
     * @example
     *   QVector<WhereCondition> conds;
     *   conds << WhereCondition("age", 18, WhereCondition::GreaterOrEqual)
     *         << WhereCondition("name", "%张%", WhereCondition::Like);
     *   auto rows = select("users", {"*"}, conds);
     */
    QVector<QVariantMap> select(const QString& table,
                                const QStringList& fields,
                                const QVector<WhereCondition>& where,
                                const QString& orderBy = "");

    /**
     * @brief 带灵活条件和排序的计数
     */
    int count(const QString& table, const QVector<WhereCondition>& where);

    /**
     * @brief 带灵活条件的更新
     */
    bool update(const QString& table,
                const QVariantMap& values,
                const QVector<WhereCondition>& where);

    /**
     * @brief 带灵活条件的删除
     */
    bool deleteRow(const QString& table, const QVector<WhereCondition>& where);

private:
    explicit SqzDbMgr(const QString& name);
    ~SqzDbMgr();
    SqzDbMgr(const SqzDbMgr&) = delete;
    SqzDbMgr& operator=(const SqzDbMgr&) = delete;

    QSqlDatabase createConnection(const QString& connectionName);
    bool bindValuesToQuery(QSqlQuery& query, const QMap<QString, QVariant>& bindings);
    QString buildWhereClause(const QVariantMap& where, QMap<QString, QVariant>& outBindings);
    QString buildWhereClause(const QVector<WhereCondition>& where, QMap<QString, QVariant>& outBindings);
    void cleanup();

    QString m_instanceName;
    DatabaseConfig m_config;
    QString m_baseConnectionName;
    QMutex m_mutex;
    QQueue<QString> m_connectionPool;
    QString m_lastError;
    bool m_configured = false;
    bool m_autoReconnect = false;

    static const int MAX_POOL_SIZE = 10;
    static QMap<QString, SqzDbMgr*> s_instances;
    static QMutex s_instanceMutex;
};

// ========== ScopedConnection 内联实现 ==========
inline ScopedConnection::ScopedConnection(SqzDbMgr& mgr)
    : m_mgr(mgr), m_db(mgr.getConnection())
{}
inline ScopedConnection::~ScopedConnection() {
    if (m_db.isValid()) {
        m_mgr.releaseConnection(m_db);
    }
}
inline QSqlDatabase& ScopedConnection::db() { return m_db; }
inline bool ScopedConnection::isValid() const { return m_db.isValid(); }
}
#endif // SQZDBMGR_H
