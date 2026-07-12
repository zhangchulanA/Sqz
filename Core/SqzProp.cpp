// SqzProp.cpp
#include "SqzProp.h"
#include <QDebug>
namespace Sqz {
// ==================== 构造/析构 ====================

SqzProp::SqzProp(QObject* parent) : QObject(parent) {}
SqzProp::~SqzProp() {}

// ==================== 注册/注销 ====================

void SqzProp::Reg(QObject* obj)
{
    if (!obj) return;
    QWriteLocker locker(&m_lock);
    if (!m_objects.contains(obj))
        m_objects.insert(obj, obj);
}

void SqzProp::UnReg(QObject* obj)
{
    if (!obj) return;
    QWriteLocker locker(&m_lock);
    m_objects.remove(obj);
}

// ==================== 查询 ====================

QList<QObject*> SqzProp::All() const
{
    QReadLocker locker(&m_lock);
    QList<QObject*> list;
    for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (!it.value().isNull())
            list.append(it.value());
    }
    return list;
}

QList<QObject*> SqzProp::FindByClass(const QString& className) const
{
    QReadLocker locker(&m_lock);
    QList<QObject*> res;
    for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
        QObject* obj = it.value();
        if (obj && QLatin1String(obj->metaObject()->className()) == className)
            res.append(obj);
    }
    return res;
}

// ==================== 属性操作 ====================

QHash<QString, QVariant> SqzProp::Props(QObject* obj) const
{
    QHash<QString, QVariant> result;
    if (!obj) return result;
    const QMetaObject* mo = obj->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        QMetaProperty prop = mo->property(i);
        if (prop.isReadable())
            result[QLatin1String(prop.name())] = prop.read(obj);
    }
    return result;
}

QList<QObject*> SqzProp::FindByProp(const QString& name, const QVariant& value) const
{
    QReadLocker locker(&m_lock);
    QList<QObject*> res;
    for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
        QObject* obj = it.value();
        if (obj && obj->property(name.toUtf8()) == value)
            res.append(obj);
    }
    return res;
}

bool SqzProp::SetProp(QObject* obj, const QString& name, const QVariant& value)
{
    QReadLocker locker(&m_lock);
    if (!m_objects.contains(obj))
        return false;
    return obj->setProperty(name.toUtf8(), value);
}

// ==================== 调试 ====================

void SqzProp::Dump() const
{
    QReadLocker locker(&m_lock);
    logdebug << "=== SqzProp dump ===";
    for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
        QObject* obj = it.value();
        if (!obj) {
            logdebug << "  (null)";
            continue;
        }
        logdebug << obj << "class:" << obj->metaObject()->className();
        auto propMap = Props(obj);
        for (auto pit = propMap.begin(); pit != propMap.end(); ++pit)
            logdebug << "    " << pit.key() << ":" << pit.value();
    }
}
}
