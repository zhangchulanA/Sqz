#ifndef RF_H
#define RF_H

#include <QObject>
#include <QVariant>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaMethod>
#include <QMetaEnum>
#include <QMetaType>
#include <QVector>
#include <QString>
#include <functional>
#include <type_traits>
#include "SqzGlobal.h"

namespace Sqz {
#define llog (qDebug()<<"["<<__LINE__<<__FUNCTION__<<"]")

inline QString LogData(const QByteArray& data){
    QString text;
    for(int i=0;i<data.size();i++){
        text += QString::number(data[i],16)+',';
    }
    return  text;
}



// 用法: VIEW_MODE(类型, 名称, 默认值)
// 示例: VIEW_MODE(int, age, 0)
#define VIEW_MODE(type, name)                         \
    Q_PROPERTY(type m_##name READ get##name WRITE set##name NOTIFY name##Changed) \
    public:                                                              \
    type get##name() const { return m_##name; }                            \
    void set##name(type val) {                                           \
    if (m_##name != val) {                                               \
    m_##name = std::move(val);                                           \
    emit name##Changed(m_##name);                                        \
}                                                                        \
}                                                                        \
    Q_SIGNALS:                                                           \
    void name##Changed(type val);                                        \
    private:                                                             \
    type m_##name;                                                       \
    public:

// ==================== 获取当前类的全部属性 ====================
// 自动生成 JSON 序列化（仅包含当前类定义的属性，自动排除父类）
#define ENABLE_JSON \
    public: \
    QJsonObject toJson() const { \
    QJsonObject obj; \
    const QMetaObject* mo = metaObject(); \
    int startIdx = mo->propertyOffset();  /* 当前类第一个属性的索引 */ \
    for (int i = startIdx; i < mo->propertyCount(); ++i) { \
    QMetaProperty prop = mo->property(i); \
    if (prop.isReadable() && prop.isValid()) { \
    obj[QLatin1String(prop.name())] = QJsonValue::fromVariant(prop.read(this)); \
} \
} \
    return obj; \
} \
    void fromJson(const QJsonObject& obj) { \
    const QMetaObject* mo = metaObject(); \
    int startIdx = mo->propertyOffset(); \
    for (int i = startIdx; i < mo->propertyCount(); ++i) { \
    QMetaProperty prop = mo->property(i); \
    if (!prop.isWritable()) continue; \
    QLatin1String name(prop.name()); \
    if (obj.contains(name)) { \
    QJsonValue val = obj[name]; \
    prop.write(this, val.toVariant()); \
} \
} \
}



// ==================== 跨线程调用 ====================
#define INVOKE_THREAD(obj, method, ...) \
    do { \
    using ObjRaw = std::remove_pointer_t<decltype(obj)>; \
    QPointer<ObjRaw> safeInvokeObj = obj; \
    QMetaObject::invokeMethod(safeInvokeObj, [safeInvokeObj, ##__VA_ARGS__]() { \
    if (!safeInvokeObj) { \
    qWarning() << "[INVOKE_THREAD] Object already destroyed, skip call:" << #method; \
    return; \
} \
    safeInvokeObj->method(__VA_ARGS__); \
}, Qt::QueuedConnection); \
} while(false)
// ==================== 枚举字符串互转 ====================
#define ENUM_STR_CLASS(EnumType, EnumName) \
    static QString EnumName##ToString(EnumType value) { \
    const QMetaObject* mo = &staticMetaObject; \
    int idx = mo->indexOfEnumerator(#EnumName); \
    if (idx >= 0) { \
    return QString::fromLatin1(mo->enumerator(idx).valueToKey(static_cast<int>(value))); \
} \
    return QString(); \
} \
    static EnumType EnumName##FromString(const QString& str, bool* ok = nullptr) { \
    const QMetaObject* mo = &staticMetaObject; \
    int idx = mo->indexOfEnumerator(#EnumName); \
    if (idx >= 0) { \
    QMetaEnum me = mo->enumerator(idx); \
    int v = me.keyToValue(str.toLatin1().constData(), ok); \
    return static_cast<EnumType>(v); \
} \
    if (ok) *ok = false; \
    return static_cast<EnumType>(0); \
}

#define ENUM_STR_GLOBAL(EnumType, EnumName) \
    Q_ENUM_NS(EnumType) \
    inline QString EnumName##ToString(EnumType value) { \
    return QMetaEnum::fromType<EnumType>().valueToKey(static_cast<int>(value)); \
} \
    inline EnumType EnumName##FromString(const QString& str, bool* ok = nullptr) { \
    int v = QMetaEnum::fromType<EnumType>().keyToValue(str.toLatin1().constData(), ok); \
    return static_cast<EnumType>(v); \
}

// ==================== 样式表快捷 ====================
#define QSS_STYLE(selector, properties) \
    QStringLiteral(selector " { " properties " }")

//侵入式转换宏
#define ENABLE_QVARIANT_CONVERSION(Type) \
    QVariant toVariant() const { return QVariant::fromValue(*static_cast<const Type*>(this)); } \
    static Type fromVariant(const QVariant &v) { return v.value<Type>(); }\
    operator QVariant(){ return toVariant();}
}



#endif // RF_H
