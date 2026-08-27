#ifndef BASEMODEL_H
#define BASEMODEL_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMetaProperty>
#include <QVariant>
#include <QDebug>

// ============================================================
// 1. 核心基类（纯数据载体，无虚函数）
// ============================================================
class BaseModel
{
public:
    virtual ~BaseModel() = default;

    // ---------- 序列化 ----------
    QJsonObject toJson() const {
        QJsonObject obj;
        const QMetaObject *mo = this->metaObject();
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
            QMetaProperty prop = mo->property(i);
            obj[prop.name()] = QJsonValue::fromVariant(prop.read(this));
        }
        return obj;
    }

    void fromJson(const QJsonObject &obj) {
        const QMetaObject *mo = this->metaObject();
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
            QMetaProperty prop = mo->property(i);
            if (obj.contains(prop.name())) {
                prop.write(this, obj[prop.name()].toVariant());
            }
        }
    }

    // ---------- 本地存储（文件存在则覆盖） ----------
    bool saveToFile(const QString &path) const {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to save:" << path;
            return false;
        }
        file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
        return true;
    }

    bool loadFromFile(const QString &path) {
        if (!QFile::exists(path)) {
            return false;  // 不存在则保留当前数据
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to load:" << path;
            return false;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << err.errorString();
            return false;
        }
        fromJson(doc.object());
        return true;
    }

    // ---------- 调试辅助 ----------
    void debugPrint() const {
        const QMetaObject *mo = this->metaObject();
        qDebug().noquote() << "=== " << mo->className() << " ===";
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
            QMetaProperty prop = mo->property(i);
            qDebug() << prop.name() << ":" << prop.read(this).toString();
        }
    }

protected:
    // 让子类通过宏获得元对象支持
    BaseModel() = default;
};

// ============================================================
// 2. 配套宏定义（让子类零代码填充数据）
// ============================================================

// 宏1：定义一个字段（属性 + 成员变量 + 默认值）
#define MODEL_FIELD(Type, Name, DefaultValue) \
    Q_PROPERTY(Type Name MEMBER Name) \
    Type Name = DefaultValue;

// 宏2：开始定义 Model 子类（自动包含 Q_GADGET）
#define BEGIN_MODEL(ClassName) \
    class ClassName : public BaseModel { \
        Q_GADGET \
    public:

// 宏3：结束定义
#define END_MODEL() \
    };

// 可选：批量定义简化宏（针对同一类型多个字段）
#define MODEL_FIELDS(Type, ...) \
    /* 此宏需要展开多个字段，为简化可逐个使用 MODEL_FIELD */

#endif // BASEMODEL_H

//#include "BaseModel.h"

//BEGIN_MODEL(UserModel)
//    MODEL_FIELD(int, id, 0)
//    MODEL_FIELD(QString, name, "unknown")
//    MODEL_FIELD(QStringList, roles, QStringList())
//    MODEL_FIELD(double, score, 0.0)
//END_MODEL()
