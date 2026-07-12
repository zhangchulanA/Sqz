// SqzProp.h
#pragma once

/**
 * @class SqzProp
 * @brief Qt对象全局注册、属性反射与查询基类
 *
 * 维护线程安全的全局对象池（QPointer防野指针），支持：
 * - 对象注册/注销（通常由SqzHub自动管理）
 * - 按类名、属性值、继承关系查找
 * - 获取/设置对象所有Qt属性
 * - 调试转储
 *
 * 所有公开方法均为线程安全（读写锁保护）。
 */
#include <QObject>
#include <QHash>
#include <QReadWriteLock>
#include <QPointer>
#include <QMetaProperty>
#include "Logger.h"
#include "RF.h"
#include "SqzGlobal.h"

namespace Sqz {

class SQZ_FRAMEWORK_API SqzProp : public QObject
{
    Q_OBJECT
    friend class SqzHub;

public:
    explicit SqzProp(QObject* parent = nullptr);
    ~SqzProp();

    // ========== 注册/注销 ==========
    /** 注册对象到全局池（若已存在则忽略） */
    void Reg(QObject* obj);
    /** 从池中移除对象（若不存在则忽略） */
    void UnReg(QObject* obj);

    // ========== 查询 ==========
    /** 返回所有有效对象的列表（自动过滤空指针） */
    QList<QObject*> All() const;

    /** 按确切类名查找（使用QLatin1String避免临时分配） */
    QList<QObject*> FindByClass(const QString& className) const;

    // ========== 属性操作 ==========
    /** 获取对象所有可读属性的键值对 */
    QHash<QString, QVariant> Props(QObject* obj) const;

    /** 按属性值查找匹配的对象 */
    QList<QObject*> FindByProp(const QString& name, const QVariant& value) const;

    /** 设置对象属性（对象须已注册），成功返回true */
    bool SetProp(QObject* obj, const QString& name, const QVariant& value);

    // ========== 调试 ==========
    /** 输出所有对象地址、类名及全部属性至日志 */
    void Dump() const;

private:
    mutable QReadWriteLock m_lock;                     // 保护对象池
    QHash<QObject*, QPointer<QObject>> m_objects;      // 对象池（QPointer自动失效）
};
}
