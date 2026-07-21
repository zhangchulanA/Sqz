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
#include <typeinfo>
#include <QDebug>
// ==================== 全局配置宏 ====================
// 1 = QByteArray 序列化为数字数组 [0,1,2]；0 = Base64 字符串
#define SERIALIZE_BYTEARRAY_AS_ARRAY 0

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

// QByteArray 自定义分支：支持数字数组/Base64切换
template<>
QJsonValue toJsonValue(const QByteArray& value) {
#if SERIALIZE_BYTEARRAY_AS_ARRAY
    QJsonArray arr;
    for (char c : value) {
        arr.append(static_cast<int>(static_cast<unsigned char>(c)));
    }
    return arr;
#else
    return QJsonValue::fromVariant(value);
#endif
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

// ==================== 反序列化重载（全修复校验逻辑） ====================
// ----- 基础类型 -----
template<typename T>
std::enable_if_t<is_basic_json_type<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (value.isNull() || value.isUndefined()) {
        qWarning() << "[SERIALIZE] Json value is null/undefined";
        return false;
    }
    QVariant var = value.toVariant();
    if (!var.canConvert<T>()) {
        qWarning() << "[SERIALIZE] Type convert fail, target:" << typeid(T).name();
        return false;
    }
    out = var.value<T>();
    return true;
}

// ----- 自定义类型（有 fromJson）-----
template<typename T>
std::enable_if_t<!is_basic_json_type<T>::value && has_from_json<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Custom type need json object";
        return false;
    }
    bool ok = out.fromJson(value.toObject());
    if (!ok) {
        qWarning() << "[SERIALIZE] Custom type parse failed:" << typeid(T).name();
    }
    return ok;
}

// ----- 容器：QList -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, QList<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QList";
        return false;
    }
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] QList item parse fail at index" << idx;
            return false;
        }
        out.append(t);
        idx++;
    }
    return true;
}

// ----- 容器：QVector -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, QVector<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QVector";
        return false;
    }
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] QVector item parse fail at index" << idx;
            return false;
        }
        out.append(t);
        idx++;
    }
    return true;
}

// ----- 容器：std::vector -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, std::vector<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for std::vector";
        return false;
    }
    auto arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] std::vector item parse fail at index" << idx;
            return false;
        }
        out.push_back(t);
        idx++;
    }
    return true;
}

// ----- 容器：QMap（修复Key转换丢失数据）-----
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QMap<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for QMap";
        return false;
    }
    auto obj = value.toObject();
    out.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] QMap key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] QMap value parse fail, key:" << it.key();
            return false;
        }
        out[key] = val;
    }
    return true;
}

// ----- 容器：QHash（修复Key转换丢失数据）-----
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QHash<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for QHash";
        return false;
    }
    auto obj = value.toObject();
    out.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] QHash key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] QHash value parse fail, key:" << it.key();
            return false;
        }
        out[key] = val;
    }
    return true;
}

// ==================== 预处理可变参数宏（扩展至20字段） ====================
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N
#define PP_RSEQ_N() 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

// 序列化基础字段写入
#define SERIALIZE_FIELD(obj, field) obj[#field] = toJsonValue(field);

// 安全反序列化：可选字段，解析失败标记状态
#define DESERIALIZE_FIELD_SAFE(obj, field, parseOkFlag) \
if (obj.contains(#field)) { \
    if (!fromJsonValue(obj[#field], field)) { \
        qWarning() << "[SERIALIZE] Optional field parse failed:" << #field; \
        parseOkFlag = false; \
    } \
}

// 必填字段：不存在/解析失败均标记错误
#define DESERIALIZE_FIELD_REQUIRED(obj, field, parseOkFlag) \
if (!obj.contains(#field)) { \
    qWarning() << "[SERIALIZE] Required field missing:" << #field; \
    parseOkFlag = false; \
} else if (!fromJsonValue(obj[#field], field)) { \
    qWarning() << "[SERIALIZE] Required field parse failed:" << #field; \
    parseOkFlag = false; \
}

// ==================== SERIALIZE_N 序列化分支（1~20） ====================
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
#define SERIALIZE_11(obj, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) SERIALIZE_10(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10) SERIALIZE_FIELD(obj,f11)
#define SERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) SERIALIZE_11(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) SERIALIZE_FIELD(obj,f12)
#define SERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) SERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) SERIALIZE_FIELD(obj,f13)
#define SERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) SERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) SERIALIZE_FIELD(obj,f14)
#define SERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) SERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) SERIALIZE_FIELD(obj,f15)
#define SERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) SERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) SERIALIZE_FIELD(obj,f16)
#define SERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) SERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) SERIALIZE_FIELD(obj,f17)
#define SERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) SERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) SERIALIZE_FIELD(obj,f18)
#define SERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) SERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) SERIALIZE_FIELD(obj,f19)
#define SERIALIZE_20(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20) SERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) SERIALIZE_FIELD(obj,f20)

// ==================== DESERIALIZE_N 反序列化分支（携带ok标记，1~20） ====================
#define DESERIALIZE_1(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok)
#define DESERIALIZE_2(obj, f1, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok)
#define DESERIALIZE_3(obj, f1, f2, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok)
#define DESERIALIZE_4(obj, f1, f2, f3, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok)
#define DESERIALIZE_5(obj, f1, f2, f3, f4, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok)
#define DESERIALIZE_6(obj, f1, f2, f3, f4, f5, f6, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f6, ok)
#define DESERIALIZE_7(obj, f1, f2, f3, f4, f5, f6, f7, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f6, ok) DESERIALIZE_FIELD_SAFE(obj, f7, ok)
#define DESERIALIZE_8(obj, f1, f2, f3, f4, f5, f6, f7, f8, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f6, ok) DESERIALIZE_FIELD_SAFE(obj, f7, ok) DESERIALIZE_FIELD_SAFE(obj, f8, ok)
#define DESERIALIZE_9(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f6, ok) DESERIALIZE_FIELD_SAFE(obj, f7, ok) DESERIALIZE_FIELD_SAFE(obj, f8, ok) DESERIALIZE_FIELD_SAFE(obj, f9, ok)
#define DESERIALIZE_10(obj, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, ok) DESERIALIZE_FIELD_SAFE(obj, f1, ok) DESERIALIZE_FIELD_SAFE(obj, f2, ok) DESERIALIZE_FIELD_SAFE(obj, f3, ok) DESERIALIZE_FIELD_SAFE(obj, f4, ok) DESERIALIZE_FIELD_SAFE(obj, f5, ok) DESERIALIZE_FIELD_SAFE(obj, f6, ok) DESERIALIZE_FIELD_SAFE(obj, f7, ok) DESERIALIZE_FIELD_SAFE(obj, f8, ok) DESERIALIZE_FIELD_SAFE(obj, f9, ok) DESERIALIZE_FIELD_SAFE(obj, f10, ok)
#define DESERIALIZE_11(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,ok) DESERIALIZE_10(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,ok) DESERIALIZE_FIELD_SAFE(obj,f11,ok)
#define DESERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,ok) DESERIALIZE_11(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,ok) DESERIALIZE_FIELD_SAFE(obj,f12,ok)
#define DESERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,ok) DESERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,ok) DESERIALIZE_FIELD_SAFE(obj,f13,ok)
#define DESERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,ok) DESERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,ok) DESERIALIZE_FIELD_SAFE(obj,f14,ok)
#define DESERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,ok) DESERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,ok) DESERIALIZE_FIELD_SAFE(obj,f15,ok)
#define DESERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,ok) DESERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,ok) DESERIALIZE_FIELD_SAFE(obj,f16,ok)
#define DESERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,ok) DESERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,ok) DESERIALIZE_FIELD_SAFE(obj,f17,ok)
#define DESERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,ok) DESERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,ok) DESERIALIZE_FIELD_SAFE(obj,f18,ok)
#define DESERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,ok) DESERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,ok) DESERIALIZE_FIELD_SAFE(obj,f19,ok)
#define DESERIALIZE_20(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,ok) DESERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,ok) DESERIALIZE_FIELD_SAFE(obj,f20,ok)

// ==================== 顶层序列化宏（修复解析状态、打印解析错误） ====================
#define SERIALIZE(...) \
    QJsonObject toJson() const { \
        QJsonObject obj; \
        CONCAT(SERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__) \
        return obj; \
    } \
    bool fromJson(const QJsonObject& obj) { \
        bool _parseAllOk = true; \
        CONCAT(DESERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__, _parseAllOk) \
        return _parseAllOk; \
    } \
    QByteArray toByteArray() const { \
        return QJsonDocument(toJson()).toJson(QJsonDocument::Compact); \
    } \
    bool fromByteArray(const QByteArray& data) { \
        QJsonParseError err; \
        auto doc = QJsonDocument::fromJson(data, &err); \
        if (err.error != QJsonParseError::NoError) { \
            qWarning() << "[SERIALIZE] Json parse error:" << err.errorString() \
                       << " offset:" << err.offset; \
            return false; \
        } \
        if (!doc.isObject()) { \
            qWarning() << "[SERIALIZE] Root json is not object"; \
            return false; \
        } \
        return fromJson(doc.object()); \
    }

#endif // SERIALIZE_H
