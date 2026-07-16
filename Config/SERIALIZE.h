#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>
#include <QList>
#include <QVector>
#include <QMap>
#include <QHash>
#include <vector>
#include <map>
#include <type_traits>

// ==================== 类型特征 ====================

template<typename T>
struct is_basic_json_type : std::false_type {};

template<> struct is_basic_json_type<int>               : std::true_type {};
template<> struct is_basic_json_type<double>            : std::true_type {};
template<> struct is_basic_json_type<bool>              : std::true_type {};
template<> struct is_basic_json_type<QString>           : std::true_type {};
template<> struct is_basic_json_type<QByteArray>        : std::true_type {};
template<> struct is_basic_json_type<QDateTime>         : std::true_type {};
template<> struct is_basic_json_type<QDate>             : std::true_type {};
template<> struct is_basic_json_type<QTime>             : std::true_type {};
template<> struct is_basic_json_type<QUuid>             : std::true_type {};
template<> struct is_basic_json_type<QUrl>              : std::true_type {};

template<typename T, typename = void>
struct has_to_json : std::false_type {};

template<typename T>
struct has_to_json<T, std::void_t<decltype(std::declval<const T>().toJson())>>
    : std::true_type {};

template<typename T, typename = void>
struct has_from_json : std::false_type {};

template<typename T>
struct has_from_json<T, std::void_t<decltype(std::declval<T>().fromJson(std::declval<QJsonObject>()))>>
    : std::true_type {};

// ==================== 序列化重载 ====================

// ----- 基础类型 -----
template<typename T>
std::enable_if_t<is_basic_json_type<T>::value, QJsonValue>
toJsonValue(const T& value) {
    return QJsonValue::fromVariant(value);
}

// ----- 自定义类型（有 toJson）-----
template<typename T>
std::enable_if_t<!is_basic_json_type<T>::value && has_to_json<T>::value, QJsonValue>
toJsonValue(const T& value) {
    return value.toJson();
}

// ----- 容器：QList -----
template<typename T>
QJsonValue toJsonValue(const QList<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}

// ----- 容器：QVector -----
template<typename T>
QJsonValue toJsonValue(const QVector<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}

// ----- 容器：std::vector -----
template<typename T>
QJsonValue toJsonValue(const std::vector<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}

// ----- 容器：QMap -----
template<typename K, typename V>
QJsonValue toJsonValue(const QMap<K, V>& map) {
    QJsonObject obj;
    for (auto it = map.begin(); it != map.end(); ++it) {
        obj[QVariant(it.key()).toString()] = toJsonValue(it.value());
    }
    return obj;
}

// ----- 容器：QHash -----
template<typename K, typename V>
QJsonValue toJsonValue(const QHash<K, V>& hash) {
    QJsonObject obj;
    for (auto it = hash.begin(); it != hash.end(); ++it) {
        obj[QVariant(it.key()).toString()] = toJsonValue(it.value());
    }
    return obj;
}

// ==================== 反序列化重载 ====================

// ----- 基础类型 -----
template<typename T>
std::enable_if_t<is_basic_json_type<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (value.isNull() || value.isUndefined()) return false;
    out = value.toVariant().value<T>();
    return true;
}

// ----- 自定义类型（有 fromJson）-----
template<typename T>
std::enable_if_t<!is_basic_json_type<T>::value && has_from_json<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (!value.isObject()) return false;
    return out.fromJson(value.toObject());
}

// ----- 容器：QList -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, QList<T>& out) {
    if (!value.isArray()) return false;
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) return false;
        out.append(t);
    }
    return true;
}

// ----- 容器：QVector -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, QVector<T>& out) {
    if (!value.isArray()) return false;
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) return false;
        out.append(t);
    }
    return true;
}

// ----- 容器：std::vector -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, std::vector<T>& out) {
    if (!value.isArray()) return false;
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) return false;
        out.push_back(t);
    }
    return true;
}

// ----- 容器：QMap -----
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QMap<K, V>& out) {
    if (!value.isObject()) return false;
    auto obj = value.toObject();
    out.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        K key = QVariant(it.key()).value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) return false;
        out[key] = val;
    }
    return true;
}

// ----- 容器：QHash -----
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QHash<K, V>& out) {
    if (!value.isObject()) return false;
    auto obj = value.toObject();
    out.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        K key = QVariant(it.key()).value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) return false;
        out[key] = val;
    }
    return true;
}

// ==================== 宏定义（支持最多 10 个字段）====================

#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define PP_RSEQ_N() 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

#define SERIALIZE_FIELD(obj, field) obj[#field] = toJsonValue(field);
#define DESERIALIZE_FIELD(obj, field) if (obj.contains(#field)) { fromJsonValue(obj[#field], field); }

#define SERIALIZE_1(obj, f1) SERIALIZE_FIELD(obj, f1)
#define SERIALIZE_2(obj, f1, f2) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2)
#define SERIALIZE_3(obj, f1, f2, f3) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3)
#define SERIALIZE_4(obj, f1, f2, f3, f4) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4)
#define SERIALIZE_5(obj, f1, f2, f3, f4, f5) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5)
#define SERIALIZE_6(obj, f1, f2, f3, f4, f5, f6) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5) SERIALIZE_FIELD(obj, f6)
#define SERIALIZE_7(obj, f1, f2, f3, f4, f5, f6, f7) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5) SERIALIZE_FIELD(obj, f6) SERIALIZE_FIELD(obj, f7)
#define SERIALIZE_8(obj, f1, f2, f3, f4, f5, f6, f7, f8) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5) SERIALIZE_FIELD(obj, f6) SERIALIZE_FIELD(obj, f7) SERIALIZE_FIELD(obj, f8)
#define SERIALIZE_9(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5) SERIALIZE_FIELD(obj, f6) SERIALIZE_FIELD(obj, f7) SERIALIZE_FIELD(obj, f8) SERIALIZE_FIELD(obj, f9)
#define SERIALIZE_10(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10) SERIALIZE_FIELD(obj, f1) SERIALIZE_FIELD(obj, f2) SERIALIZE_FIELD(obj, f3) SERIALIZE_FIELD(obj, f4) SERIALIZE_FIELD(obj, f5) SERIALIZE_FIELD(obj, f6) SERIALIZE_FIELD(obj, f7) SERIALIZE_FIELD(obj, f8) SERIALIZE_FIELD(obj, f9) SERIALIZE_FIELD(obj, f10)

#define DESERIALIZE_1(obj, f1) DESERIALIZE_FIELD(obj, f1)
#define DESERIALIZE_2(obj, f1, f2) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2)
#define DESERIALIZE_3(obj, f1, f2, f3) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3)
#define DESERIALIZE_4(obj, f1, f2, f3, f4) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4)
#define DESERIALIZE_5(obj, f1, f2, f3, f4, f5) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5)
#define DESERIALIZE_6(obj, f1, f2, f3, f4, f5, f6) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5) DESERIALIZE_FIELD(obj, f6)
#define DESERIALIZE_7(obj, f1, f2, f3, f4, f5, f6, f7) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5) DESERIALIZE_FIELD(obj, f6) DESERIALIZE_FIELD(obj, f7)
#define DESERIALIZE_8(obj, f1, f2, f3, f4, f5, f6, f7, f8) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5) DESERIALIZE_FIELD(obj, f6) DESERIALIZE_FIELD(obj, f7) DESERIALIZE_FIELD(obj, f8)
#define DESERIALIZE_9(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5) DESERIALIZE_FIELD(obj, f6) DESERIALIZE_FIELD(obj, f7) DESERIALIZE_FIELD(obj, f8) DESERIALIZE_FIELD(obj, f9)
#define DESERIALIZE_10(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10) DESERIALIZE_FIELD(obj, f1) DESERIALIZE_FIELD(obj, f2) DESERIALIZE_FIELD(obj, f3) DESERIALIZE_FIELD(obj, f4) DESERIALIZE_FIELD(obj, f5) DESERIALIZE_FIELD(obj, f6) DESERIALIZE_FIELD(obj, f7) DESERIALIZE_FIELD(obj, f8) DESERIALIZE_FIELD(obj, f9) DESERIALIZE_FIELD(obj, f10)

#define SERIALIZE(...) \
    QJsonObject toJson() const { \
        QJsonObject obj; \
        CONCAT(SERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__) \
        return obj; \
    } \
    bool fromJson(const QJsonObject& obj) { \
        CONCAT(DESERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__) \
        return true; \
    } \
    QByteArray toByteArray() const { \
        return QJsonDocument(toJson()).toJson(); \
    } \
    bool fromByteArray(const QByteArray& data) { \
        QJsonParseError err; \
        auto doc = QJsonDocument::fromJson(data, &err); \
        if (err.error != QJsonParseError::NoError || !doc.isObject()) \
            return false; \
        return fromJson(doc.object()); \
    }

#endif // SERIALIZE_H
