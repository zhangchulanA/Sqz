#include "Entangler.h"
#include <QUuid>
#include <QTimer>

Entangler* Entangler::m_instance = nullptr;

Entangler* Entangler::Instance()
{
    if (!m_instance) {
        m_instance = new Entangler();
    }
    return m_instance;
}

Entangler::Entangler(QObject *parent)
    : QObject(parent)
{
    // 定时清理已销毁的对象
    auto timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, &QTimer::timeout, [this]() {
        QList<QObject*> dead;
        for (auto obj : m_links.keys()) {
            if (!obj) dead.append(obj);
        }
        for (auto obj : dead) {
            m_links.remove(obj);
        }
    });
    timer->start();
}

QString Entangler::LinkId(QObject *obj1, QObject *obj2)
{
    return QString("%1_%2").arg((quintptr)obj1).arg((quintptr)obj2);
}

QMap<QString, QVariant> Entangler::GetAllProperties(QObject *obj)
{
    QMap<QString, QVariant> props;
    const QMetaObject *meta = obj->metaObject();

    for (int i = 0; i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        if (prop.isReadable() && prop.isWritable()) {
            props[prop.name()] = prop.read(obj);
        }
    }
    return props;
}

bool Entangler::EntangleObjects(
    QObject *obj1,
    QObject *obj2,
    std::function<QMap<QString, QVariant>(QObject*)> getState)
{
    if (!obj1 || !obj2 || obj1 == obj2) {
        qWarning() << "⚠️ 无效对象";
        return false;
    }

    if (IsEntangled(obj1, obj2)) {
        qWarning() << "⚠️ 已纠缠";
        return false;
    }

    QString id = LinkId(obj1, obj2);

    // 对象销毁时自动清理
    connect(obj1, &QObject::destroyed, [this, obj1, obj2]() {
        m_links.remove(obj1);
        m_links.remove(obj2);
    });
    connect(obj2, &QObject::destroyed, [this, obj1, obj2]() {
        m_links.remove(obj1);
        m_links.remove(obj2);
    });

    // 定时同步（50ms间隔）
    auto timer = new QTimer(this);
    timer->setInterval(50);

    connect(timer, &QTimer::timeout, [this, obj1, obj2, getState]() {
        QString id = LinkId(obj1, obj2);
        if (m_syncing.contains(id)) return;
        m_syncing.insert(id);

        SyncObjects(obj1, obj2, getState);
        SyncObjects(obj2, obj1, getState);

        m_syncing.remove(id);
    });

    timer->start();

    // 记录
    m_links[obj1].append(id);
    m_links[obj2].append(id);
    m_pairs[id] = {obj1, obj2};
    obj1->setProperty("__entangler_timer", QVariant::fromValue(timer));

    // 首次同步
    SyncObjects(obj1, obj2, getState);
    SyncObjects(obj2, obj1, getState);

    emit LinkCreated(obj1, obj2);
    qDebug() << "🔗 纠缠:" << obj1->objectName() << "↔" << obj2->objectName();

    return true;
}

void Entangler::SyncObjects(
    QObject *source,
    QObject *target,
    std::function<QMap<QString, QVariant>(QObject*)> getState)
{
    if (!source || !target) return;

    auto srcState = getState(source);
    auto tgtState = getState(target);

    for (auto it = srcState.begin(); it != srcState.end(); ++it) {
        const QString &key = it.key();
        const QVariant &val = it.value();

        if (tgtState.contains(key) && tgtState[key] != val) {
            target->setProperty(key.toUtf8(), val);
            emit Synced(source, target, key);
            qDebug() << "🌀 同步:" << source->objectName()
                     << "." << key << "→" << target->objectName();
        }
    }
}

bool Entangler::Entangle(
    QObject *obj1,
    QObject *obj2,
    std::function<void(QObject*, QObject*)> syncFunc)
{
    if (!obj1 || !obj2 || obj1 == obj2) {
        qWarning() << "⚠️ 无效对象";
        return false;
    }

    if (IsEntangled(obj1, obj2)) {
        qWarning() << "⚠️ 已纠缠";
        return false;
    }

    QString id = LinkId(obj1, obj2);

    connect(obj1, &QObject::destroyed, [this, obj1, obj2]() {
        m_links.remove(obj1);
        m_links.remove(obj2);
    });
    connect(obj2, &QObject::destroyed, [this, obj1, obj2]() {
        m_links.remove(obj1);
        m_links.remove(obj2);
    });

    auto timer = new QTimer(this);
    timer->setInterval(50);

    connect(timer, &QTimer::timeout, [this, obj1, obj2, syncFunc]() {
        QString id = LinkId(obj1, obj2);
        if (m_syncing.contains(id)) return;
        m_syncing.insert(id);

        syncFunc(obj1, obj2);

        m_syncing.remove(id);
    });

    timer->start();

    m_links[obj1].append(id);
    m_links[obj2].append(id);
    m_pairs[id] = {obj1, obj2};

    syncFunc(obj1, obj2);

    emit LinkCreated(obj1, obj2);
    qDebug() << "🔗 纠缠:" << obj1->objectName() << "↔" << obj2->objectName();

    return true;
}

bool Entangler::IsEntangled(QObject *obj1, QObject *obj2)
{
    if (!obj1 || !obj2) return false;
    return m_pairs.contains(LinkId(obj1, obj2));
}

QList<QPair<QObject*, QObject*>> Entangler::AllLinks()
{
    QList<QPair<QObject*, QObject*>> result;
    for (auto it = m_pairs.begin(); it != m_pairs.end(); ++it) {
        result.append(it.value());
    }
    return result;
}
