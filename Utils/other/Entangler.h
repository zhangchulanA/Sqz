#ifndef ENTANGLER_H
#define ENTANGLER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QMetaObject>
#include <QMetaProperty>
#include <QDebug>
#include <functional>

/**
 * 量子纠缠同步引擎
 * 让两个对象自动保持同步
 */
namespace Sqz {
class Entangler : public QObject
{
    Q_OBJECT

public:
    static Entangler* Instance();

    /**
     * 纠缠两个对象（同步所有属性）
     */
    template<typename T>
    bool Entangle(T *obj1, T *obj2) {
        return EntangleObjects(obj1, obj2, [](QObject *obj) {
            return GetAllProperties(obj);
        });
    }

    /**
     * 纠缠两个对象（仅同步指定属性）
     */
    template<typename T>
    bool Entangle(T *obj1, T *obj2, const QStringList &props) {
        return EntangleObjects(obj1, obj2, [props](QObject *obj) {
            QMap<QString, QVariant> map;
            for (const QString &name : props) {
                map[name] = obj->property(name.toUtf8());
            }
            return map;
        });
    }

    /**
     * 纠缠两个对象（自定义同步逻辑）
     */
    bool Entangle(QObject *obj1, QObject *obj2,
                  std::function<void(QObject*, QObject*)> syncFunc);

    /**
     * 观测：打破纠缠，返回独立副本
     */
    template<typename T>
    T* Observe(T *obj) {
        disconnect(obj, nullptr, this, nullptr);
        m_links.remove(obj);
        m_links.remove(obj);
        qDebug() << "👁️ 观测导致纠缠坍缩";
        return new T(*obj);
    }

    /**
     * 检查是否已纠缠
     */
    bool IsEntangled(QObject *obj1, QObject *obj2);

    /**
     * 获取所有纠缠对
     */
    QList<QPair<QObject*, QObject*>> AllLinks();

signals:
    void LinkCreated(QObject *obj1, QObject *obj2);
    void LinkBroken(QObject *obj1, QObject *obj2);
    void Synced(QObject *source, QObject *target, const QString &prop);

private:
    explicit Entangler(QObject *parent = nullptr);

    bool EntangleObjects(QObject *obj1, QObject *obj2,
                         std::function<QMap<QString, QVariant>(QObject*)> getState);

    void SyncObjects(QObject *source, QObject *target,
                     std::function<QMap<QString, QVariant>(QObject*)> getState);

    static QMap<QString, QVariant> GetAllProperties(QObject *obj);
    QString LinkId(QObject *obj1, QObject *obj2);

    QSet<QString> m_syncing;
    QMap<QObject*, QList<QString>> m_links;
    QMap<QString, QPair<QObject*, QObject*>> m_pairs;

    static Entangler *m_instance;
};

// 便捷宏
#define ENTANGLE(obj1, obj2) Entangler::Instance()->Entangle(obj1, obj2)
#define ENTANGLE_PROPS(obj1, obj2, ...) Entangler::Instance()->Entangle(obj1, obj2, QStringList() __VA_ARGS__)
}
#endif // ENTANGLER_H
