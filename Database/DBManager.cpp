#include "DBManager.h"
#include <QSqlError>
#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace Sqz {

// ==================== 静态成员初始化 ====================
QMap<QString, DBManager*> DBManager::s_instances;
QMutex DBManager::s_instanceMutex;

// ==================== 实例管理 ====================
DBManager& DBManager::instance() {
    return instance("default");
}

DBManager& DBManager::instance(const QString& name) {
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instances.contains(name)) {
        s_instances[name] = new DBManager(name);
    }
    return *s_instances[name];
}

void DBManager::destroyInstance(const QString& name) {
    QMutexLocker locker(&s_instanceMutex);
    if (s_instances.contains(name)) {
        s_instances[name]->cleanup();
        delete s_instances[name];
        s_instances.remove(name);
    }
}

void DBManager::destroyAll() {
    QMutexLocker locker(&s_instanceMutex);
    for (auto it = s_instances.begin(); it != s_instances.end(); ++it) {
        it.value()->cleanup();
        delete it.value();
    }
    s_instances.clear();
}

// ==================== 构造函数 ====================
DBManager::DBManager(const QString& name)
    : QObject(nullptr), m_instanceName(name), m_baseConnectionName("db_" + name)
{
}

DBManager::~DBManager() {
    cleanup();
}

// ==================== 配置 ====================
void DBManager::configure(const Configuration& config) {
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

    // 预创建连接池
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
QSqlDatabase DBManager::getConnection() {
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

void DBManager::releaseConnection(const QSqlDatabase& db) {
    if (!db.isValid() || !m_configured) return;
    QMutexLocker locker(&m_mutex);
    m_connectionPool.enqueue(db.connectionName());
}

QSqlDatabase DBManager::createConnection(const QString& connectionName) {
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

void DBManager::cleanup() {
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

QString DBManager::lastError() const {
    return m_lastError;
}

// ==================== 健康检查 ====================
bool DBManager::ping() {
    QMutexLocker locker(&m_mutex);
    if (!m_configured) {
        m_lastError = "Not configured";
        return false;
    }

    QStringList failed;
    QVector<QString> tempList = m_connectionPool.toVector();
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

void DBManager::setAutoReconnect(bool enabled) {
    m_autoReconnect = enabled;
}

bool DBManager::autoReconnect() const {
    return m_autoReconnect;
}

// ==================== 底层 SQL ====================
bool DBManager::bindValuesToQuery(QSqlQuery& query, const QMap<QString, QVariant>& bindings) {
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

QVector<QVariantMap> DBManager::executeQuery(const QString& sql,
                                             const QMap<QString, QVariant>& bindings) {
    ConnectionGuard conn(*this);
    if (!conn.isValid()) return {};

    QSqlQuery query(conn.database());
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

int DBManager::executeNonQuery(const QString& sql,
                               const QMap<QString, QVariant>& bindings) {
    ConnectionGuard conn(*this);
    if (!conn.isValid()) return -1;

    QSqlQuery query(conn.database());
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

QVariant DBManager::executeInsert(const QString& sql,
                                  const QMap<QString, QVariant>& bindings) {
    ConnectionGuard conn(*this);
    if (!conn.isValid()) return {};

    QSqlQuery query(conn.database());
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
bool DBManager::executeTransaction(std::function<bool(QSqlDatabase& db)> func) {
    ConnectionGuard conn(*this);
    if (!conn.isValid()) {
        m_lastError = "transaction: no connection";
        return false;
    }

    QSqlDatabase& db = conn.database();
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

// ==================== CRUD - QVariantMap 版本 ====================
bool DBManager::insertRecord(const QString& table, const QVariantMap& values) {
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

QVector<QVariantMap> DBManager::selectRecords(const QString& table,
                                              const QStringList& fields,
                                              const QVariantMap& where) {
    QVector<Condition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << Condition(it.key(), it.value(), Condition::Equal);
    }
    return selectRecords(table, fields, conds);
}

bool DBManager::updateRecords(const QString& table,
                              const QVariantMap& values,
                              const QVariantMap& where) {
    QVector<Condition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << Condition(it.key(), it.value(), Condition::Equal);
    }
    return updateRecords(table, values, conds);
}

bool DBManager::deleteRecords(const QString& table, const QVariantMap& where) {
    QVector<Condition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << Condition(it.key(), it.value());
    }
    return deleteRecords(table, conds);
}

int DBManager::countRecords(const QString& table, const QVariantMap& where) {
    QVector<Condition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << Condition(it.key(), it.value());
    }
    return countRecords(table, conds);
}

bool DBManager::recordExists(const QString& table, const QVariantMap& where) {
    QVector<Condition> conds;
    for (auto it = where.begin(); it != where.end(); ++it) {
        conds << Condition(it.key(), it.value());
    }
    return recordExists(table, conds);
}

bool DBManager::createTable(const QString& table, const QMap<QString, QString>& columns) {
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

bool DBManager::dropTable(const QString& table) {
    QString sql = QString("DROP TABLE IF EXISTS %1").arg(table);
    return executeNonQuery(sql) >= 0;
}

// ==================== CRUD - CondList 版本（初始化列表） ====================
QVector<QVariantMap> DBManager::selectRecords(const QString& table,
                                              const QStringList& fields,
                                              CondList conditions,
                                              const QString& orderBy) {
    QVector<Condition> conds = QVector<Condition>::fromList(QList<Condition>(conditions));
    return selectRecords(table, fields, conds, orderBy);
}

bool DBManager::updateRecords(const QString& table,
                              const QVariantMap& values,
                              CondList conditions) {
    QVector<Condition> conds = QVector<Condition>::fromList(QList<Condition>(conditions));
    return updateRecords(table, values, conds);
}

bool DBManager::deleteRecords(const QString& table, CondList conditions) {
    QVector<Condition> conds = QVector<Condition>::fromList(QList<Condition>(conditions));
    return deleteRecords(table, conds);
}

int DBManager::countRecords(const QString& table, CondList conditions) {
    QVector<Condition> conds = QVector<Condition>::fromList(QList<Condition>(conditions));
    return countRecords(table, conds);
}

bool DBManager::recordExists(const QString& table, CondList conditions) {
    QVector<Condition> conds = QVector<Condition>::fromList(QList<Condition>(conditions));
    return recordExists(table, conds);
}

// ==================== CRUD - QVector<Condition> 版本 ====================
QVector<QVariantMap> DBManager::selectRecords(const QString& table,
                                              const QStringList& fields,
                                              const QVector<Condition>& where,
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

bool DBManager::updateRecords(const QString& table,
                              const QVariantMap& values,
                              const QVector<Condition>& where) {
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

bool DBManager::deleteRecords(const QString& table, const QVector<Condition>& where) {
    if (where.isEmpty()) {
        m_lastError = "delete: where cannot be empty";
        return false;
    }
    QMap<QString, QVariant> bindings;
    QString sql = QString("DELETE FROM %1 WHERE %2")
                      .arg(table, buildWhereClause(where, bindings));
    return executeNonQuery(sql, bindings) >= 0;
}

int DBManager::countRecords(const QString& table, const QVector<Condition>& where) {
    QString sql = QString("SELECT COUNT(*) AS cnt FROM %1").arg(table);
    QMap<QString, QVariant> bindings;
    if (!where.isEmpty()) {
        sql += " WHERE " + buildWhereClause(where, bindings);
    }
    auto results = executeQuery(sql, bindings);
    return results.isEmpty() ? -1 : results.first()["cnt"].toInt();
}

bool DBManager::recordExists(const QString& table, const QVector<Condition>& where) {
    return countRecords(table, where) > 0;
}

// ==================== JSON 互转 ====================
QJsonArray DBManager::toJson(const QVector<QVariantMap>& rows) const {
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

bool DBManager::insertRecordFromJson(const QString& table, const QJsonObject& jsonObject) {
    QVariantMap data;
    for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it) {
        data[it.key()] = it.value().toVariant();
    }
    return insertRecord(table, data);
}

int DBManager::insertBatchFromJson(const QString& table, const QJsonArray& jsonArray) {
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

// ==================== 分页查询 ====================
QVariantMap DBManager::selectPage(const QString& table,
                                  int page,
                                  int pageSize,
                                  const QStringList& fields,
                                  const QVector<Condition>& where,
                                  const QString& orderBy) {
    QVariantMap result;
    int total = countRecords(table, where);
    result["total"] = total;

    if (total < 1 || page < 1 || pageSize < 1) {
        result["rows"] = QVariant::fromValue(QVector<QVariantMap>());
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

// ==================== 批量插入 ====================
int DBManager::insertBatch(const QString& table, const QVector<QVariantMap>& valuesList) {
    if (valuesList.isEmpty()) {
        m_lastError = "insertBatch: empty list";
        return 0;
    }

    bool ok = executeTransaction([&](QSqlDatabase& db) {
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
QString DBManager::buildWhereClause(const QVariantMap& where,
                                    QMap<QString, QVariant>& outBindings) {
    QStringList conditions;
    for (auto it = where.begin(); it != where.end(); ++it) {
        QString paramName = ":where_" + it.key();
        conditions << QString("%1 = %2").arg(it.key(), paramName);
        outBindings[paramName] = it.value();
    }
    return conditions.join(" AND ");
}

QString DBManager::buildWhereClause(const QVector<Condition>& where,
                                    QMap<QString, QVariant>& outBindings) {
    QStringList conditions;
    int idx = 0;
    for (const auto& cond : where) {
        QString paramName = QString(":w_%1").arg(idx++);
        QString opStr;

        if (!cond.customOperator.isEmpty()) {
            opStr = cond.customOperator;
        } else {
            switch (cond.op) {
            case Condition::Equal:          opStr = "="; break;
            case Condition::NotEqual:       opStr = "!="; break;
            case Condition::Greater:        opStr = ">"; break;
            case Condition::Less:           opStr = "<"; break;
            case Condition::GreaterOrEqual: opStr = ">="; break;
            case Condition::LessOrEqual:    opStr = "<="; break;
            case Condition::Like:           opStr = "LIKE"; break;
            case Condition::In: {
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

} // namespace Sqz
