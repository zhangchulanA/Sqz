#include "SqzDbMgr.h"
#include <QSqlError>
#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
namespace Sqz{
// ==================== 静态成员初始化 ====================
QMap<QString, SqzDbMgr*> SqzDbMgr::s_instances;
QMutex SqzDbMgr::s_instanceMutex;

// ==================== 实例管理 ====================
SqzDbMgr& SqzDbMgr::instance() {
    return instance("default");
}

SqzDbMgr& SqzDbMgr::instance(const QString& name) {
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instances.contains(name)) {
        s_instances[name] = new SqzDbMgr(name);
    }
    return *s_instances[name];
}

void SqzDbMgr::destroyInstance(const QString& name) {
    QMutexLocker locker(&s_instanceMutex);
    if (s_instances.contains(name)) {
        s_instances[name]->cleanup();
        delete s_instances[name];
        s_instances.remove(name);
    }
}

void SqzDbMgr::destroyAll() {
    QMutexLocker locker(&s_instanceMutex);
    for (auto it = s_instances.begin(); it != s_instances.end(); ++it) {
        it.value()->cleanup();
        delete it.value();
    }
    s_instances.clear();
}

// ==================== 构造函数 ====================
SqzDbMgr::SqzDbMgr(const QString& name)
    : QObject(nullptr), m_instanceName(name), m_baseConnectionName("db_" + name)
{
}

SqzDbMgr::~SqzDbMgr() {
    cleanup();
}

// ==================== 配置 ====================
void SqzDbMgr::configure(const DatabaseConfig& config) {
    QMutexLocker locker(&m_mutex);
    if (m_configured) {
        cleanup();
    }

    m_config = config;
    if (!config.connectionName.isEmpty()) {
        m_baseConnectionName = config.connectionName;
    }

    if (m_config.type.isEmpty() || m_config.databaseName.isEmpty()) {
        m_lastError = "Database type and name cannot be empty";
        return;
    }
    if (m_config.type != "QSQLITE" && m_config.host.isEmpty()) {
        m_lastError = "Host cannot be empty for " + m_config.type;
        return;
    }

    for (int i = 0; i < MAX_POOL_SIZE; ++i) {
        QString connName = QString("%1_pool_%2").arg(m_baseConnectionName).arg(i);
        if (QSqlDatabase::contains(connName)) {
            QSqlDatabase::removeDatabase(connName);
        }
        QSqlDatabase db = createConnection(connName);
        if (db.isValid() && db.isOpen()) {
            m_connectionPool.enqueue(connName);
        }
    }
    m_configured = true;
}

// ==================== 连接池 ====================
QSqlDatabase SqzDbMgr::getConnection() {
    QMutexLocker locker(&m_mutex);
    if (!m_configured) {
        m_lastError = "Database not configured. Call configure() first.";
        return QSqlDatabase();
    }
    if (m_connectionPool.isEmpty()) {
        m_lastError = "Connection pool exhausted. Increase MAX_POOL_SIZE.";
        return QSqlDatabase();
    }
    QString connName = m_connectionPool.dequeue();
    QSqlDatabase db = QSqlDatabase::database(connName);
    if (!db.isOpen()) {
        // 自动重连逻辑
        if (m_autoReconnect) {
            db.open();
            if (!db.isOpen()) {
                m_lastError = "Reconnect failed: " + db.lastError().text();
                return QSqlDatabase();
            }
        } else {
            m_lastError = "Connection " + connName + " is not open.";
            return QSqlDatabase();
        }
    }
    return db;
}

void SqzDbMgr::releaseConnection(const QSqlDatabase& db) {
    if (!db.isValid() || !m_configured) return;
    QMutexLocker locker(&m_mutex);
    m_connectionPool.enqueue(db.connectionName());
}

QSqlDatabase SqzDbMgr::createConnection(const QString& connectionName) {
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(m_config.type, connectionName);
    if (m_config.type == "QSQLITE") {
        db.setDatabaseName(m_config.databaseName);
    } else {
        db.setHostName(m_config.host);
        if (m_config.port > 0) db.setPort(m_config.port);
        db.setDatabaseName(m_config.databaseName);
        db.setUserName(m_config.userName);
        db.setPassword(m_config.password);
    }
    if (!db.open()) {
        m_lastError = "Failed to open: " + db.lastError().text();
    }
    return db;
}

void SqzDbMgr::cleanup() {
    QMutexLocker locker(&m_mutex);
    while (!m_connectionPool.isEmpty()) {
        QString connName = m_connectionPool.dequeue();
        {
            QSqlDatabase db = QSqlDatabase::database(connName);
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }
    m_configured = false;
}

QString SqzDbMgr::lastError() const {
    return m_lastError;
}

// ==================== 健康检查 ====================
bool SqzDbMgr::ping() {
    QMutexLocker locker(&m_mutex);
    if (!m_configured) {
        m_lastError = "Not configured";
        return false;
    }

    // 遍历池中所有连接，执行 SELECT 1
    QStringList failed;
    QVector<QString> tempList = m_connectionPool.toVector(); // 拷贝一份（修正）
    for (const QString& connName : tempList) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (!db.isOpen()) {
            failed << connName;
            continue;
        }
        QSqlQuery q(db);
        if (!q.exec("SELECT 1")) {
            failed << connName;
        }
    }

    // 对失败的连接尝试重连
    for (const QString& connName : failed) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen()) db.close();
        if (!db.open()) {
            m_lastError = "Ping failed, unable to reconnect: " + connName;
            return false;
        }
    }
    return true;
}

void SqzDbMgr::setAutoReconnect(bool enabled) {
    m_autoReconnect = enabled;
}

bool SqzDbMgr::autoReconnect() const {
    return m_autoReconnect;
}

// ==================== 底层 SQL ====================
bool SqzDbMgr::bindValuesToQuery(QSqlQuery& query, const QMap<QString, QVariant>& bindings) {
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        if (it.key().startsWith(':')) {
            query.bindValue(it.key(), it.value());
        } else {
            bool ok;
            int pos = it.key().toInt(&ok);
            if (ok) query.bindValue(pos, it.value());
        }
    }
    return true;
}

QVector<QVariantMap> SqzDbMgr::executeQuery(const QString& sql,
                                                    const QMap<QString, QVariant>& bindings) {
    ScopedConnection conn(*this);
    if (!conn.isValid()) return {};

    QSqlQuery query(conn.db());
    if (!query.prepare(sql)) {
        m_lastError = "Prepare failed: " + query.lastError().text();
        return {};
    }
    bindValuesToQuery(query, bindings);
    if (!query.exec()) {
        m_lastError = "Exec failed: " + query.lastError().text();
        return {};
    }

    QVector<QVariantMap> results;
    while (query.next()) {
        QVariantMap row;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            row[record.fieldName(i)] = record.value(i);
        }
        results.append(row);
    }
    return results;
}

int SqzDbMgr::executeNonQuery(const QString& sql,
                                     const QMap<QString, QVariant>& bindings) {
    ScopedConnection conn(*this);
    if (!conn.isValid()) return -1;

    QSqlQuery query(conn.db());
    if (!query.prepare(sql)) {
        m_lastError = "Prepare failed: " + query.lastError().text();
        return -1;
    }
    bindValuesToQuery(query, bindings);
    if (!query.exec()) {
        m_lastError = "Exec failed: " + query.lastError().text();
        return -1;
    }
    return query.numRowsAffected();
}

QVariant SqzDbMgr::executeInsert(const QString& sql,
                                         const QMap<QString, QVariant>& bindings) {
    ScopedConnection conn(*this);
    if (!conn.isValid()) return {};

    QSqlQuery query(conn.db());
    if (!query.prepare(sql)) {
        m_lastError = "Prepare failed: " + query.lastError().text();
        return {};
    }
    bindValuesToQuery(query, bindings);
    if (!query.exec()) {
        m_lastError = "Exec failed: " + query.lastError().text();
        return {};
    }
    return query.lastInsertId();
}

// ==================== 事务 ====================
bool SqzDbMgr::transaction(std::function<bool(QSqlDatabase& db)> func) {
    ScopedConnection conn(*this);
    if (!conn.isValid()) {
        m_lastError = "transaction: no connection";
        return false;
    }

    QSqlDatabase& db = conn.db();
    if (!db.transaction()) {
        m_lastError = "transaction: begin failed: " + db.lastError().text();
        return false;
    }

    bool success = false;
    try {
        success = func(db);
    } catch (const std::exception& e) {
        m_lastError = QString("transaction exception: %1").arg(e.what());
        success = false;
    } catch (...) {
        m_lastError = "transaction: unknown exception";
        success = false;
    }

    if (success) {
        if (!db.commit()) {
            m_lastError = "transaction: commit failed: " + db.lastError().text();
            db.rollback();
            return false;
        }
        return true;
    } else {
        if (!m_lastError.contains("transaction")) {
            m_lastError = "transaction: lambda returned false, rolling back";
        }
        db.rollback();
        return false;
    }
}

// ==================== 简便接口 ====================
bool SqzDbMgr::insert(const QString& table, const QVariantMap& values) {
    if (values.isEmpty()) {
        m_lastError = "insert: values empty";
        return false;
    }
    QStringList cols, holders;
    QMap<QString, QVariant> bindings;
    for (auto it = values.begin(); it != values.end(); ++it) {
        cols << it.key();
        QString h = ":" + it.key();
        holders << h;
        bindings[h] = it.value();
    }
    QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                      .arg(table, cols.join(", "), holders.join(", "));
    return executeNonQuery(sql, bindings) > 0;
}

QVector<QVariantMap> SqzDbMgr::select(const QString& table,
                                              const QStringList& fields,
                                              const QVariantMap& where) {
    // 保持向后兼容：内部调用灵活条件版本
    QVector<WhereCondition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << WhereCondition(it.key(), it.value(), WhereCondition::Equal);
    }
    return select(table, fields, conds);
}

// 灵活条件重载
QVector<QVariantMap> SqzDbMgr::select(const QString& table,
                                              const QStringList& fields,
                                              const QVector<WhereCondition>& where,
                                              const QString& orderBy) {
    QString f = fields.isEmpty() ? "*" : fields.join(", ");
    QString sql = QString("SELECT %1 FROM %2").arg(f, table);
    QMap<QString, QVariant> bindings;
    if (!where.isEmpty()) {
        sql += " WHERE " + buildWhereClause(where, bindings);
    }
    if (!orderBy.isEmpty()) {
        sql += " ORDER BY " + orderBy;
    }
    return executeQuery(sql, bindings);
}

bool SqzDbMgr::update(const QString& table,
                             const QVariantMap& values,
                             const QVariantMap& where) {
    QVector<WhereCondition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << WhereCondition(it.key(), it.value(), WhereCondition::Equal);
    }
    return update(table, values, conds);
}

bool SqzDbMgr::update(const QString& table,
                             const QVariantMap& values,
                             const QVector<WhereCondition>& where) {
    if (values.isEmpty() || where.isEmpty()) {
        m_lastError = "update: values and where cannot be empty";
        return false;
    }
    QStringList sets;
    QMap<QString, QVariant> bindings;
    for (auto it = values.begin(); it != values.end(); ++it) {
        QString p = ":set_" + it.key();
        sets << QString("%1 = %2").arg(it.key(), p);
        bindings[p] = it.value();
    }
    QString sql = QString("UPDATE %1 SET %2 WHERE %3")
                      .arg(table, sets.join(", "), buildWhereClause(where, bindings));
    return executeNonQuery(sql, bindings) >= 0;
}

bool SqzDbMgr::deleteRow(const QString& table, const QVariantMap& where) {
    QVector<WhereCondition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << WhereCondition(it.key(), it.value());
    }
    return deleteRow(table, conds);
}

bool SqzDbMgr::deleteRow(const QString& table, const QVector<WhereCondition>& where) {
    if (where.isEmpty()) {
        m_lastError = "deleteRow: where cannot be empty";
        return false;
    }
    QMap<QString, QVariant> bindings;
    QString sql = QString("DELETE FROM %1 WHERE %2")
                      .arg(table, buildWhereClause(where, bindings));
    return executeNonQuery(sql, bindings) >= 0;
}

int SqzDbMgr::count(const QString& table, const QVariantMap& where) {
    QVector<WhereCondition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << WhereCondition(it.key(), it.value());
    }
    return count(table, conds);
}

int SqzDbMgr::count(const QString& table, const QVector<WhereCondition>& where) {
    QString sql = QString("SELECT COUNT(*) AS cnt FROM %1").arg(table);
    QMap<QString, QVariant> bindings;
    if (!where.isEmpty()) {
        sql += " WHERE " + buildWhereClause(where, bindings);
    }
    auto results = executeQuery(sql, bindings);
    return results.isEmpty() ? -1 : results.first()["cnt"].toInt();
}

bool SqzDbMgr::exists(const QString& table, const QVariantMap& where) {
    return count(table, where) > 0;
}

bool SqzDbMgr::createTable(const QString& table, const QMap<QString, QString>& columns) {
    if (columns.isEmpty()) {
        m_lastError = "createTable: columns empty";
        return false;
    }
    QStringList colDefs;
    for (auto it = columns.begin(); it != columns.end(); ++it) {
        colDefs << QString("%1 %2").arg(it.key(), it.value());
    }
    QString sql = QString("CREATE TABLE IF NOT EXISTS %1 (%2)").arg(table, colDefs.join(", "));
    return executeNonQuery(sql) >= 0;
}

bool SqzDbMgr::dropTable(const QString& table) {
    QString sql = QString("DROP TABLE IF EXISTS %1").arg(table);
    return executeNonQuery(sql) >= 0;
}

// ==================== 新增功能 1：JSON 互转 ====================
QJsonArray SqzDbMgr::toJson(const QVector<QVariantMap>& rows) const {
    QJsonArray arr;
    for (const auto& row : rows) {
        QJsonObject obj;
        for (auto it = row.begin(); it != row.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        arr.append(obj);
    }
    return arr;
}

bool SqzDbMgr::insertFromJson(const QString& table, const QJsonObject& jsonObj) {
    QVariantMap data;
    for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
        data[it.key()] = it.value().toVariant();
    }
    return insert(table, data);
}

int SqzDbMgr::insertBatchFromJson(const QString& table, const QJsonArray& jsonArray) {
    QVector<QVariantMap> list;
    for (const auto& val : jsonArray) {
        QVariantMap map;
        QJsonObject obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            map[it.key()] = it.value().toVariant();
        }
        list.append(map);
    }
    return insertBatch(table, list);
}

// ==================== 新增功能 2：分页查询 ====================
QVariantMap SqzDbMgr::selectPage(const QString& table,
                                        int page,
                                        int pageSize,
                                        const QStringList& fields,
                                        const QVector<WhereCondition>& where,
                                        const QString& orderBy) {
    QVariantMap result;
    int total = count(table, where);
    result["total"] = total;

    if (total < 1 || page < 1 || pageSize < 1) {
        result["rows"] = QVariant::fromValue(QVector<QVariantMap>());  // 修正
        return result;
    }

    int offset = (page - 1) * pageSize;
    QString f = fields.isEmpty() ? "*" : fields.join(", ");
    QString sql = QString("SELECT %1 FROM %2").arg(f, table);
    QMap<QString, QVariant> bindings;
    if (!where.isEmpty()) {
        sql += " WHERE " + buildWhereClause(where, bindings);
    }
    if (!orderBy.isEmpty()) {
        sql += " ORDER BY " + orderBy;
    }
    sql += QString(" LIMIT %1 OFFSET %2").arg(pageSize).arg(offset);

    auto rows = executeQuery(sql, bindings);
    result["rows"] = QVariant::fromValue(rows);
    return result;
}

// ==================== 新增功能 3：批量插入 ====================
int SqzDbMgr::insertBatch(const QString& table, const QVector<QVariantMap>& valuesList) {
    if (valuesList.isEmpty()) {
        m_lastError = "insertBatch: empty list";
        return 0;
    }

    bool ok = transaction([&](QSqlDatabase& db) {
        for (const auto& vals : valuesList) {
            QStringList cols, holders;
            QMap<QString, QVariant> bindings;
            for (auto it = vals.begin(); it != vals.end(); ++it) {
                cols << it.key();
                QString h = ":" + it.key();
                holders << h;
                bindings[h] = it.value();
            }
            QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                              .arg(table, cols.join(", "), holders.join(", "));

            QSqlQuery query(db);
            if (!query.prepare(sql)) {
                m_lastError = "Batch prepare failed: " + query.lastError().text();
                return false;
            }
            for (auto it = bindings.begin(); it != bindings.end(); ++it) {
                query.bindValue(it.key(), it.value());
            }
            if (!query.exec()) {
                m_lastError = "Batch exec failed: " + query.lastError().text();
                return false;
            }
        }
        return true;
    });

    return ok ? valuesList.size() : -1;
}

// ==================== 内部辅助函数 ====================
QString SqzDbMgr::buildWhereClause(const QVariantMap& where,
                                          QMap<QString, QVariant>& outBindings) {
    QStringList conditions;
    for (auto it = where.begin(); it != where.end(); ++it) {
        QString paramName = ":where_" + it.key();
        conditions << QString("%1 = %2").arg(it.key(), paramName);
        outBindings[paramName] = it.value();
    }
    return conditions.join(" AND ");
}

QString SqzDbMgr::buildWhereClause(const QVector<WhereCondition>& where,
                                          QMap<QString, QVariant>& outBindings) {
    QStringList conditions;
    int idx = 0;
    for (const auto& cond : where) {
        QString paramName = QString(":w_%1").arg(idx++);
        QString opStr;

        if (!cond.customOp.isEmpty()) {
            opStr = cond.customOp;
        } else {
            switch (cond.op) {
            case WhereCondition::Equal:          opStr = "="; break;
            case WhereCondition::NotEqual:       opStr = "!="; break;
            case WhereCondition::Greater:        opStr = ">"; break;
            case WhereCondition::Less:           opStr = "<"; break;
            case WhereCondition::GreaterOrEqual: opStr = ">="; break;
            case WhereCondition::LessOrEqual:    opStr = "<="; break;
            case WhereCondition::Like:           opStr = "LIKE"; break;
            case WhereCondition::In: {
                QVariantList list = cond.value.toList();
                if (list.isEmpty()) {
                    conditions << "1=0";
                    continue;
                }
                QStringList placeholders;
                for (int i = 0; i < list.size(); ++i) {
                    QString p = QString(":w_in_%1_%2").arg(idx).arg(i);
                    placeholders << p;
                    outBindings[p] = list[i];
                }
                conditions << QString("%1 IN (%2)").arg(cond.field, placeholders.join(", "));
                continue;
            }
            }
        }

        conditions << QString("%1 %2 %3").arg(cond.field, opStr, paramName);
        outBindings[paramName] = cond.value;
    }
    return conditions.join(" AND ");
}
}
