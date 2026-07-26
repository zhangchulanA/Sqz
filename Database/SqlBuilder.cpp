#include "SqlBuilder.h"
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================
// 构造函数
// ============================================================

SqlBuilder::SqlBuilder()
{
}

// ============================================================
// 添加WHERE条件（内部辅助）
// ============================================================

void SqlBuilder::AddWhere(const QString &condition)
{
    if (!condition.isEmpty()) {
        m_whereClauses.append(condition);
    }
}

// ============================================================
// 添加绑定值（内部辅助）
// ============================================================

void SqlBuilder::AddBindValue(const QVariant &value)
{
    m_bindValues.append(value);
}

// ============================================================
// SELECT 相关
// ============================================================

SqlBuilder& SqlBuilder::Select(const QStringList &fields)
{
    m_type = SqlType::Select;
    m_selectFields = fields;
    return *this;
}

SqlBuilder& SqlBuilder::Select(const QString &field)
{
    return Select(QStringList{field});
}

SqlBuilder& SqlBuilder::SelectDistinct(const QStringList &fields)
{
    m_type = SqlType::Select;
    m_selectFields = fields;
    m_distinct = true;
    return *this;
}

// ============================================================
// FROM 相关
// ============================================================

SqlBuilder& SqlBuilder::From(const QString &table)
{
    m_fromTable = table;
    m_fromAlias.clear();
    return *this;
}

SqlBuilder& SqlBuilder::From(const QString &table, const QString &alias)
{
    m_fromTable = table;
    m_fromAlias = alias;
    return *this;
}

// ============================================================
// JOIN 相关
// ============================================================

SqlBuilder& SqlBuilder::Join(const QString &table, const QString &condition)
{
    m_joins.append({"", table, condition});
    return *this;
}

SqlBuilder& SqlBuilder::LeftJoin(const QString &table, const QString &condition)
{
    m_joins.append({"LEFT", table, condition});
    return *this;
}

SqlBuilder& SqlBuilder::RightJoin(const QString &table, const QString &condition)
{
    m_joins.append({"RIGHT", table, condition});
    return *this;
}

SqlBuilder& SqlBuilder::InnerJoin(const QString &table, const QString &condition)
{
    m_joins.append({"INNER", table, condition});
    return *this;
}

// ============================================================
// WHERE 相关（手写条件）
// ============================================================

SqlBuilder& SqlBuilder::WhereRaw(const QString &condition)
{
    AddWhere(condition);
    return *this;
}

// ============================================================
// WHERE 相关（参数化查询 - 防SQL注入）
// ============================================================

SqlBuilder& SqlBuilder::WhereEq(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 = ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereNe(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 != ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereGt(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 > ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereGe(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 >= ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereLt(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 < ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereLe(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    AddWhere(QString("%1 <= ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereLike(const QString &field, const QString &pattern)
{
    AddBindValue(pattern);
    AddWhere(QString("%1 LIKE ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereIn(const QString &field, const QVariantList &values)
{
    if (values.isEmpty()) {
        AddWhere("1=0");  // 空集合永假，返回空结果
        return *this;
    }

    QStringList placeholders;
    for (const auto &v : values) {
        AddBindValue(v);
        placeholders.append("?");
    }
    AddWhere(QString("%1 IN (%2)").arg(field).arg(placeholders.join(", ")));
    return *this;
}

SqlBuilder& SqlBuilder::WhereNotIn(const QString &field, const QVariantList &values)
{
    if (values.isEmpty()) {
        return *this;
    }

    QStringList placeholders;
    for (const auto &v : values) {
        AddBindValue(v);
        placeholders.append("?");
    }
    AddWhere(QString("%1 NOT IN (%2)").arg(field).arg(placeholders.join(", ")));
    return *this;
}

SqlBuilder& SqlBuilder::WhereBetween(const QString &field, const QVariant &start, const QVariant &end)
{
    AddBindValue(start);
    AddBindValue(end);
    AddWhere(QString("%1 BETWEEN ? AND ?").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereNull(const QString &field)
{
    AddWhere(QString("%1 IS NULL").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereNotNull(const QString &field)
{
    AddWhere(QString("%1 IS NOT NULL").arg(field));
    return *this;
}

SqlBuilder& SqlBuilder::WhereExists(const SqlBuilder &subQuery)
{
    AddWhere(QString("EXISTS (%1)").arg(subQuery.Build()));
    m_bindValues.append(subQuery.GetBindValues());
    return *this;
}

SqlBuilder& SqlBuilder::WhereNotExists(const SqlBuilder &subQuery)
{
    AddWhere(QString("NOT EXISTS (%1)").arg(subQuery.Build()));
    m_bindValues.append(subQuery.GetBindValues());
    return *this;
}

// ============================================================
// OR 条件包装
// ============================================================

SqlBuilder& SqlBuilder::OrBegin()
{
    m_whereClauses.append("(");
    m_whereLevel++;
    return *this;
}

SqlBuilder& SqlBuilder::OrEnd()
{
    if (m_whereLevel > 0) {
        if (!m_whereClauses.isEmpty()) {
            QString last = m_whereClauses.takeLast();
            m_whereClauses.append("(" + last + ")");
        }
        m_whereLevel--;
    }
    return *this;
}

// ============================================================
// GROUP BY 相关
// ============================================================

SqlBuilder& SqlBuilder::GroupBy(const QStringList &fields)
{
    m_groupByFields = fields;
    return *this;
}

SqlBuilder& SqlBuilder::GroupBy(const QString &field)
{
    return GroupBy(QStringList{field});
}

// ============================================================
// HAVING 相关
// ============================================================

SqlBuilder& SqlBuilder::Having(const QString &condition)
{
    m_havingClause = condition;
    return *this;
}

SqlBuilder& SqlBuilder::HavingEq(const QString &field, const QVariant &value)
{
    AddBindValue(value);
    m_havingClause = QString("%1 = ?").arg(field);
    return *this;
}

// ============================================================
// ORDER BY 相关
// ============================================================

SqlBuilder& SqlBuilder::OrderBy(const QString &field, const QString &direction)
{
    QString dir = direction.toUpper();
    if (dir != "ASC" && dir != "DESC") dir = "ASC";
    m_orderByClauses.append(QString("%1 %2").arg(field).arg(dir));
    return *this;
}

SqlBuilder& SqlBuilder::OrderByAsc(const QString &field)
{
    return OrderBy(field, "ASC");
}

SqlBuilder& SqlBuilder::OrderByDesc(const QString &field)
{
    return OrderBy(field, "DESC");
}

// ============================================================
// LIMIT / OFFSET / PAGE
// ============================================================

SqlBuilder& SqlBuilder::Limit(int limit)
{
    m_limit = limit;
    return *this;
}

SqlBuilder& SqlBuilder::Offset(int offset)
{
    m_offset = offset;
    return *this;
}

SqlBuilder& SqlBuilder::Page(int page, int pageSize)
{
    if (page < 1) page = 1;
    m_limit = pageSize;
    m_offset = (page - 1) * pageSize;
    return *this;
}

// ============================================================
// INSERT 相关
// ============================================================

SqlBuilder& SqlBuilder::InsertInto(const QString &table)
{
    m_type = SqlType::Insert;
    m_insertTable = table;
    return *this;
}

SqlBuilder& SqlBuilder::Values(const QVariantMap &data)
{
    m_insertData.append(data);
    return *this;
}

SqlBuilder& SqlBuilder::Values(const QList<QVariantMap> &dataList)
{
    m_insertData.append(dataList);
    return *this;
}

// ============================================================
// UPDATE 相关
// ============================================================

SqlBuilder& SqlBuilder::Update(const QString &table)
{
    m_type = SqlType::Update;
    m_updateTable = table;
    return *this;
}

SqlBuilder& SqlBuilder::Set(const QString &field, const QVariant &value)
{
    m_updateData[field] = value;
    return *this;
}

SqlBuilder& SqlBuilder::Set(const QVariantMap &data)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        m_updateData[it.key()] = it.value();
    }
    return *this;
}

// ============================================================
// DELETE 相关
// ============================================================

SqlBuilder& SqlBuilder::Delete(const QString &table)
{
    m_type = SqlType::Delete;
    if (!table.isEmpty()) {
        m_deleteTable = table;
    }
    return *this;
}

SqlBuilder& SqlBuilder::DeleteFrom(const QString &table)
{
    return Delete(table);
}

// ============================================================
// 子查询支持
// ============================================================

SqlBuilder SqlBuilder::SubQuery()
{
    return SqlBuilder();
}

QString SqlBuilder::ToSubQuery() const
{
    return "(" + Build() + ")";
}

// ============================================================
// UNION 支持
// ============================================================

SqlBuilder& SqlBuilder::Union(const SqlBuilder &other)
{
    if (m_type == SqlType::None) {
        m_type = SqlType::Union;
    }
    m_unionQueries.append(other.Build());
    m_unionAll = false;
    return *this;
}

SqlBuilder& SqlBuilder::UnionAll(const SqlBuilder &other)
{
    if (m_type == SqlType::None) {
        m_type = SqlType::Union;
    }
    m_unionQueries.append(other.Build());
    m_unionAll = true;
    return *this;
}

// ============================================================
// 构建SQL（核心方法 - const）
// ============================================================

QString SqlBuilder::Build() const
{
    switch (m_type) {
        case SqlType::Select: return BuildSelect();
        case SqlType::Insert: return BuildInsert();
        case SqlType::Update: return BuildUpdate();
        case SqlType::Delete: return BuildDelete();
        case SqlType::Union: return BuildUnion();
        default: {
            qWarning() << "SqlBuilder: 未指定SQL类型，返回空字符串";
            return QString();
        }
    }
}

// ============================================================
// 构建 SELECT 语句
// ============================================================

QString SqlBuilder::BuildSelect() const
{
    if (m_fromTable.isEmpty()) {
        qWarning() << "SqlBuilder: SELECT 缺少 FROM 表名";
        return QString();
    }

    QString sql = "SELECT ";

    if (m_distinct) sql += "DISTINCT ";

    if (m_selectFields.isEmpty()) {
        sql += "*";
    } else {
        sql += m_selectFields.join(", ");
    }

    sql += " FROM " + m_fromTable;
    if (!m_fromAlias.isEmpty()) {
        sql += " AS " + m_fromAlias;
    }

    // JOIN
    for (const auto &join : m_joins) {
        if (!join.type.isEmpty()) {
            sql += " " + join.type + " JOIN ";
        } else {
            sql += " JOIN ";
        }
        sql += join.table + " ON " + join.condition;
    }

    // WHERE
    if (!m_whereClauses.isEmpty()) {
        sql += " WHERE " + m_whereClauses.join(" AND ");
    }

    // GROUP BY
    if (!m_groupByFields.isEmpty()) {
        sql += " GROUP BY " + m_groupByFields.join(", ");
    }

    // HAVING
    if (!m_havingClause.isEmpty()) {
        sql += " HAVING " + m_havingClause;
    }

    // ORDER BY
    if (!m_orderByClauses.isEmpty()) {
        sql += " ORDER BY " + m_orderByClauses.join(", ");
    }

    // LIMIT
    if (m_limit > 0) {
        sql += " LIMIT " + QString::number(m_limit);
    }

    // OFFSET
    if (m_offset > 0) {
        sql += " OFFSET " + QString::number(m_offset);
    }

    return sql;
}

// ============================================================
// 构建 INSERT 语句
// ============================================================

QString SqlBuilder::BuildInsert() const
{
    if (m_insertTable.isEmpty()) {
        qWarning() << "SqlBuilder: INSERT 缺少表名";
        return QString();
    }

    if (m_insertData.isEmpty()) {
        qWarning() << "SqlBuilder: INSERT 数据为空";
        return QString();
    }

    QString sql = "INSERT INTO " + m_insertTable + " ";

    // 取第一条数据获取字段名
    auto firstRow = m_insertData.first();
    QStringList fields = firstRow.keys();

    sql += "(" + fields.join(", ") + ") VALUES ";

    QStringList rowPlaceholders;
    for (int i = 0; i < m_insertData.size(); ++i) {
        QStringList placeholders;
        for (int j = 0; j < fields.size(); ++j) {
            placeholders.append("?");
        }
        rowPlaceholders.append("(" + placeholders.join(", ") + ")");
    }

    sql += rowPlaceholders.join(", ");

    return sql;
}

// ============================================================
// 构建 UPDATE 语句
// ============================================================

QString SqlBuilder::BuildUpdate() const
{
    if (m_updateTable.isEmpty()) {
        qWarning() << "SqlBuilder: UPDATE 缺少表名";
        return QString();
    }

    if (m_updateData.isEmpty()) {
        qWarning() << "SqlBuilder: UPDATE 数据为空";
        return QString();
    }

    QString sql = "UPDATE " + m_updateTable + " SET ";

    QStringList setClauses;
    for (auto it = m_updateData.begin(); it != m_updateData.end(); ++it) {
        setClauses.append(it.key() + " = ?");
    }

    sql += setClauses.join(", ");

    if (!m_whereClauses.isEmpty()) {
        sql += " WHERE " + m_whereClauses.join(" AND ");
    }

    return sql;
}

// ============================================================
// 构建 DELETE 语句
// ============================================================

QString SqlBuilder::BuildDelete() const
{
    if (m_deleteTable.isEmpty()) {
        qWarning() << "SqlBuilder: DELETE 缺少表名";
        return QString();
    }

    QString sql = "DELETE FROM " + m_deleteTable;

    if (!m_whereClauses.isEmpty()) {
        sql += " WHERE " + m_whereClauses.join(" AND ");
    }

    return sql;
}

// ============================================================
// 构建 UNION 语句
// ============================================================

QString SqlBuilder::BuildUnion() const
{
    if (m_unionQueries.isEmpty()) {
        qWarning() << "SqlBuilder: UNION 缺少子查询";
        return QString();
    }

    QString keyword = m_unionAll ? " UNION ALL " : " UNION ";
    return m_unionQueries.join(keyword);
}

// ============================================================
// 执行相关（const + const QSqlDatabase&）
// ============================================================

QSqlQuery SqlBuilder::DoExec(const QSqlDatabase &db) const
{
    QString sql = Build();
    if (sql.isEmpty()) {
        return QSqlQuery();
    }

    // 收集所有绑定值
    QVariantList bindValues = m_bindValues;

    // INSERT 批量数据绑定
    if (m_type == SqlType::Insert) {
        for (const auto &row : m_insertData) {
            for (auto it = row.begin(); it != row.end(); ++it) {
                bindValues.append(it.value());
            }
        }
    }

    // UPDATE 数据绑定
    if (m_type == SqlType::Update) {
        for (auto it = m_updateData.begin(); it != m_updateData.end(); ++it) {
            bindValues.append(it.value());
        }
    }

    QSqlQuery query(db);
    query.prepare(sql);

    for (int i = 0; i < bindValues.size(); ++i) {
        query.bindValue(i, bindValues[i]);
    }

    if (!query.exec()) {
        qWarning() << "SqlBuilder 执行失败:" << query.lastError().text();
        qWarning() << "SQL:" << sql;
    }

    return query;
}

QSqlQuery SqlBuilder::Exec() const
{
    return DoExec(QSqlDatabase::database());
}

QSqlQuery SqlBuilder::Exec(const QSqlDatabase &db) const
{
    return DoExec(db);
}

bool SqlBuilder::ExecQuery() const
{
    return ExecQuery(QSqlDatabase::database());
}

bool SqlBuilder::ExecQuery(const QSqlDatabase &db) const
{
    auto query = DoExec(db);
    return query.isValid() && query.lastError().type() == QSqlError::NoError;
}

QVariantList SqlBuilder::FetchAll() const
{
    return FetchAll(QSqlDatabase::database());
}

QVariantList SqlBuilder::FetchAll(const QSqlDatabase &db) const
{
    QVariantList results;
    QSqlQuery query = DoExec(db);

    if (!query.isValid()) return results;

    while (query.next()) {
        QVariantMap row;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            row[record.fieldName(i)] = query.value(i);
        }
        results.append(row);
    }

    return results;
}

QVariantMap SqlBuilder::FetchOne() const
{
    return FetchOne(QSqlDatabase::database());
}

QVariantMap SqlBuilder::FetchOne(const QSqlDatabase &db) const
{
    QVariantMap result;
    QSqlQuery query = DoExec(db);

    if (!query.isValid()) return result;

    if (query.next()) {
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            result[record.fieldName(i)] = query.value(i);
        }
    }

    return result;
}

QVariant SqlBuilder::FetchScalar() const
{
    return FetchScalar(QSqlDatabase::database());
}

QVariant SqlBuilder::FetchScalar(const QSqlDatabase &db) const
{
    QSqlQuery query = DoExec(db);

    if (!query.isValid()) return QVariant();

    if (query.next()) {
        return query.value(0);
    }

    return QVariant();
}

// ============================================================
// 工具方法
// ============================================================

SqlBuilder& SqlBuilder::Clear()
{
    m_type = SqlType::None;
    m_selectFields.clear();
    m_distinct = false;
    m_fromTable.clear();
    m_fromAlias.clear();
    m_joins.clear();
    m_whereClauses.clear();
    m_bindValues.clear();
    m_whereLevel = 0;
    m_groupByFields.clear();
    m_havingClause.clear();
    m_orderByClauses.clear();
    m_limit = -1;
    m_offset = -1;
    m_insertTable.clear();
    m_insertData.clear();
    m_updateTable.clear();
    m_updateData.clear();
    m_deleteTable.clear();
    m_unionQueries.clear();
    m_unionAll = false;
    return *this;
}

void SqlBuilder::Debug() const
{
    qDebug() << "========================================";
    qDebug() << "🔍 SQL Builder Debug:";

    QString typeStr;
    switch (m_type) {
        case SqlType::Select: typeStr = "SELECT"; break;
        case SqlType::Insert: typeStr = "INSERT"; break;
        case SqlType::Update: typeStr = "UPDATE"; break;
        case SqlType::Delete: typeStr = "DELETE"; break;
        case SqlType::Union: typeStr = "UNION"; break;
        default: typeStr = "NONE"; break;
    }
    qDebug() << "  Type:" << typeStr;

    if (m_type == SqlType::Select) {
        qDebug() << "  SELECT:" << m_selectFields.join(", ");
        qDebug() << "  DISTINCT:" << m_distinct;
        qDebug() << "  FROM:" << m_fromTable << (m_fromAlias.isEmpty() ? "" : "AS " + m_fromAlias);
        qDebug() << "  JOIN:" << m_joins.size() << "个";
        qDebug() << "  WHERE:" << m_whereClauses.join(" AND ");
        qDebug() << "  GROUP BY:" << m_groupByFields.join(", ");
        qDebug() << "  HAVING:" << m_havingClause;
        qDebug() << "  ORDER BY:" << m_orderByClauses.join(", ");
        qDebug() << "  LIMIT:" << m_limit;
        qDebug() << "  OFFSET:" << m_offset;
    } else if (m_type == SqlType::Insert) {
        qDebug() << "  INSERT INTO:" << m_insertTable;
        qDebug() << "  DATA:" << m_insertData.size() << "行";
        if (!m_insertData.isEmpty()) {
            qDebug() << "  FIELDS:" << m_insertData.first().keys().join(", ");
        }
    } else if (m_type == SqlType::Update) {
        qDebug() << "  UPDATE:" << m_updateTable;
        qDebug() << "  SET:" << m_updateData.keys().join(", ");
        qDebug() << "  WHERE:" << m_whereClauses.join(" AND ");
    } else if (m_type == SqlType::Delete) {
        qDebug() << "  DELETE FROM:" << m_deleteTable;
        qDebug() << "  WHERE:" << m_whereClauses.join(" AND ");
    } else if (m_type == SqlType::Union) {
        qDebug() << "  UNION:" << m_unionQueries.size() << "个";
        qDebug() << "  ALL:" << m_unionAll;
    }

    qDebug() << "  Bind Values:" << m_bindValues.size() << "个";
    if (!m_bindValues.isEmpty()) {
        for (int i = 0; i < m_bindValues.size(); ++i) {
            qDebug() << "    [" << i << "] =" << m_bindValues[i];
        }
    }

    qDebug() << "  SQL:" << Build();
    qDebug() << "========================================";
}

QString SqlBuilder::GetSqlType() const
{
    switch (m_type) {
        case SqlType::Select: return "SELECT";
        case SqlType::Insert: return "INSERT";
        case SqlType::Update: return "UPDATE";
        case SqlType::Delete: return "DELETE";
        case SqlType::Union: return "UNION";
        default: return "NONE";
    }
}
