#ifndef SQLBUILDER_H
#define SQLBUILDER_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <functional>
#include <QDateTime>

/**
 * @brief 链式SQL构建器
 *
 * 用法示例:
 * @code
 * SqlBuilder()
 *     .Select({"name", "age"})
 *     .From("users")
 *     .Where("age > 18")
 *     .OrderBy("age", "DESC")
 *     .Limit(10)
 *     .Exec();   // 使用默认数据库连接
 * @endcode
 */
class SqlBuilder
{
public:
    SqlBuilder();

    // ==================== SELECT 相关 ====================

    /** @brief 指定查询字段（多个） */
    SqlBuilder& Select(const QStringList &fields);

    /** @brief 指定查询字段（单个） */
    SqlBuilder& Select(const QString &field);

    /** @brief 去重查询 */
    SqlBuilder& SelectDistinct(const QStringList &fields);

    // ==================== FROM 相关 ====================

    /** @brief 指定数据表 */
    SqlBuilder& From(const QString &table);

    /** @brief 指定数据表并起别名 */
    SqlBuilder& From(const QString &table, const QString &alias);

    // ==================== JOIN 相关 ====================

    /** @brief 内连接（可简写为JOIN） */
    SqlBuilder& Join(const QString &table, const QString &condition);

    /** @brief 左连接 */
    SqlBuilder& LeftJoin(const QString &table, const QString &condition);

    /** @brief 右连接 */
    SqlBuilder& RightJoin(const QString &table, const QString &condition);

    /** @brief 内连接（显式） */
    SqlBuilder& InnerJoin(const QString &table, const QString &condition);

    // ==================== WHERE 条件相关 ====================

    /** @brief 手写WHERE条件（不推荐，建议用下面的参数化方法） */
    SqlBuilder& WhereRaw(const QString &condition);

    /** @brief WHERE 字段 = 值 */
    SqlBuilder& WhereEq(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 != 值 */
    SqlBuilder& WhereNe(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 > 值 */
    SqlBuilder& WhereGt(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 >= 值 */
    SqlBuilder& WhereGe(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 < 值 */
    SqlBuilder& WhereLt(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 <= 值 */
    SqlBuilder& WhereLe(const QString &field, const QVariant &value);

    /** @brief WHERE 字段 LIKE 模式 */
    SqlBuilder& WhereLike(const QString &field, const QString &pattern);

    /** @brief WHERE 字段 IN (值列表) */
    SqlBuilder& WhereIn(const QString &field, const QVariantList &values);

    /** @brief WHERE 字段 NOT IN (值列表) */
    SqlBuilder& WhereNotIn(const QString &field, const QVariantList &values);

    /** @brief WHERE 字段 BETWEEN 值1 AND 值2 */
    SqlBuilder& WhereBetween(const QString &field, const QVariant &start, const QVariant &end);

    /** @brief WHERE 字段 IS NULL */
    SqlBuilder& WhereNull(const QString &field);

    /** @brief WHERE 字段 IS NOT NULL */
    SqlBuilder& WhereNotNull(const QString &field);

    /** @brief WHERE EXISTS (子查询) */
    SqlBuilder& WhereExists(const SqlBuilder &subQuery);

    /** @brief WHERE NOT EXISTS (子查询) */
    SqlBuilder& WhereNotExists(const SqlBuilder &subQuery);

    // ==================== OR 条件包装 ====================

    /** @brief 开始一个OR条件组 */
    SqlBuilder& OrBegin();

    /** @brief 结束OR条件组 */
    SqlBuilder& OrEnd();

    // ==================== GROUP BY 相关 ====================

    /** @brief 分组（多个字段） */
    SqlBuilder& GroupBy(const QStringList &fields);

    /** @brief 分组（单个字段） */
    SqlBuilder& GroupBy(const QString &field);

    // ==================== HAVING 相关 ====================

    /** @brief HAVING条件（分组后过滤） */
    SqlBuilder& Having(const QString &condition);

    /** @brief HAVING 字段 = 值 */
    SqlBuilder& HavingEq(const QString &field, const QVariant &value);

    // ==================== ORDER BY 相关 ====================

    /** @brief 排序（指定方向） */
    SqlBuilder& OrderBy(const QString &field, const QString &direction = "ASC");

    /** @brief 升序排序 */
    SqlBuilder& OrderByAsc(const QString &field);

    /** @brief 降序排序 */
    SqlBuilder& OrderByDesc(const QString &field);

    // ==================== LIMIT / OFFSET 相关 ====================

    /** @brief 限制返回行数 */
    SqlBuilder& Limit(int limit);

    /** @brief 偏移行数 */
    SqlBuilder& Offset(int offset);

    /** @brief 分页（页码从1开始） */
    SqlBuilder& Page(int page, int pageSize);

    // ==================== INSERT 相关 ====================

    /** @brief 插入数据到表 */
    SqlBuilder& InsertInto(const QString &table);

    /** @brief 插入单行数据 */
    SqlBuilder& Values(const QVariantMap &data);

    /** @brief 插入多行数据（批量插入） */
    SqlBuilder& Values(const QList<QVariantMap> &dataList);

    // ==================== UPDATE 相关 ====================

    /** @brief 更新表 */
    SqlBuilder& Update(const QString &table);

    /** @brief 设置单个字段 */
    SqlBuilder& Set(const QString &field, const QVariant &value);

    /** @brief 设置多个字段 */
    SqlBuilder& Set(const QVariantMap &data);

    // ==================== DELETE 相关 ====================

    /** @brief 删除数据（需配合Where使用） */
    SqlBuilder& Delete(const QString &table = "");

    /** @brief 从表删除数据（同Delete） */
    SqlBuilder& DeleteFrom(const QString &table);

    // ==================== 子查询支持 ====================

    /** @brief 创建子查询对象（用于嵌套） */
    static SqlBuilder SubQuery();

    /** @brief 将当前查询作为子查询 */
    QString ToSubQuery() const;

    // ==================== UNION 支持 ====================

    /** @brief UNION（合并两个查询结果，去重） */
    SqlBuilder& Union(const SqlBuilder &other);

    /** @brief UNION ALL（合并两个查询结果，不去重） */
    SqlBuilder& UnionAll(const SqlBuilder &other);

    // ==================== 执行相关（修复：使用 const 引用） ====================

    /** @brief 构建最终SQL语句 */
    QString Build() const;

    /** @brief 获取参数绑定值列表 */
    QVariantList GetBindValues() const { return m_bindValues; }

    /** @brief 执行SQL并返回QSqlQuery（使用默认数据库连接） */
    QSqlQuery Exec() const;

    /** @brief 执行SQL并返回QSqlQuery（使用指定数据库连接） */
    QSqlQuery Exec(const QSqlDatabase &db) const;

    /** @brief 执行SQL并返回是否成功（使用默认数据库连接） */
    bool ExecQuery() const;

    /** @brief 执行SQL并返回是否成功（使用指定数据库连接） */
    bool ExecQuery(const QSqlDatabase &db) const;

    /** @brief 执行查询并返回结果集（使用默认数据库连接） */
    QVariantList FetchAll() const;

    /** @brief 执行查询并返回结果集（使用指定数据库连接） */
    QVariantList FetchAll(const QSqlDatabase &db) const;

    /** @brief 执行查询并返回第一行（使用默认数据库连接） */
    QVariantMap FetchOne() const;

    /** @brief 执行查询并返回第一行（使用指定数据库连接） */
    QVariantMap FetchOne(const QSqlDatabase &db) const;

    /** @brief 执行查询并返回单个值（使用默认数据库连接） */
    QVariant FetchScalar() const;

    /** @brief 执行查询并返回单个值（使用指定数据库连接） */
    QVariant FetchScalar(const QSqlDatabase &db) const;

    // ==================== 工具方法 ====================

    /** @brief 清空当前构建状态（复用对象） */
    SqlBuilder& Clear();

    /** @brief 打印调试信息 */
    void Debug() const;

    /** @brief 获取当前SQL类型（SELECT/INSERT/UPDATE/DELETE） */
    QString GetSqlType() const;

private:
    // ===== 辅助方法 =====
    void AddWhere(const QString &condition);
    void AddBindValue(const QVariant &value);
    QSqlQuery DoExec(const QSqlDatabase &db) const;

    // ===== SQL类型枚举 =====
    enum class SqlType {
        Select,
        Insert,
        Update,
        Delete,
        Union,
        None
    };

    SqlType m_type = SqlType::None;

    // ===== SELECT 部分 =====
    QStringList m_selectFields;
    bool m_distinct = false;

    // ===== FROM 部分 =====
    QString m_fromTable;
    QString m_fromAlias;

    // ===== JOIN 部分 =====
    struct JoinClause {
        QString type;      // "INNER", "LEFT", "RIGHT"
        QString table;
        QString condition;
    };
    QList<JoinClause> m_joins;

    // ===== WHERE 部分 =====
    QStringList m_whereClauses;
    mutable QVariantList m_bindValues;  // mutable 允许在 const 方法中修改
    int m_whereLevel = 0;

    // ===== GROUP BY 部分 =====
    QStringList m_groupByFields;

    // ===== HAVING 部分 =====
    QString m_havingClause;

    // ===== ORDER BY 部分 =====
    QStringList m_orderByClauses;

    // ===== LIMIT/OFFSET =====
    int m_limit = -1;
    int m_offset = -1;

    // ===== INSERT 部分 =====
    QString m_insertTable;
    QList<QVariantMap> m_insertData;

    // ===== UPDATE 部分 =====
    QString m_updateTable;
    QVariantMap m_updateData;

    // ===== DELETE 部分 =====
    QString m_deleteTable;

    // ===== UNION 部分 =====
    QStringList m_unionQueries;
    bool m_unionAll = false;

    // ===== 构建方法 =====
    QString BuildSelect() const;
    QString BuildInsert() const;
    QString BuildUpdate() const;
    QString BuildDelete() const;
    QString BuildUnion() const;
};

#endif // SQLBUILDER_H
