#ifndef SERIALIZE_H
#define SERIALIZE_H

// ============================================================================
//  SERIALIZE.h — 轻量级 Qt JSON 序列化/反序列化模板库（仅头文件）
//
//  特性：
//    - 一行宏声明字段即可生成 toJson()/fromJson()/toByteArray()/fromByteArray()
//    - 基础类型经 QVariant 统一处理；QByteArray 走 Base64（二进制安全，双向对称）
//    - 支持自定义类型（拥有 toJson()/fromJson(QJsonObject)）
//    - 顺序容器：QList / QVector / std::vector / QSet / std::list
//    - 关联容器：QMap / QHash / std::map / std::unordered_map
//    - 二元组：QPair / std::pair（序列化为长度 2 的数组）
//    - 反序列化逐字段安全校验，单点失败返回 false 并打印定位日志
//
//  兼容：Qt 5.12+，C++14（不依赖 C++17 折叠表达式）
//
//  用法示例：
//    struct User {
//        int id;
//        QString name;
//        QByteArray avatar;
//        QList<int> scores;
//        SERIALIZE(id, name, avatar, scores)   // 生成四个序列化方法
//    };
//
//  语义说明：
//    - 序列化：写出所有声明字段。
//    - 反序列化：字段缺失视为“可选项缺失”跳过（保持旧行为）；
//                字段存在但解析失败则置 parseOk=false 并返回 false。
//              若需“必填”语义，可在自定义 fromJson 中手写 DESERIALIZE_FIELD_REQUIRED。
// ============================================================================

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QVariant>
#include <QList>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QUuid>
#include <QUrl>
#include <QDebug>

#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <utility>
#include <type_traits>
#include <typeinfo>

// ==================== 全局配置宏 ====================
// 1 = QByteArray 序列化为数字数组 [0,1,2,...,255]；0 = Base64 字符串
// 注意：两种模式均可双向往返；切换会改变线上格式，需收发两端一致
// 可在包含本头文件前用 -D 或 #define 覆盖默认值（原版为无条件定义，无法覆盖）
#ifndef SERIALIZE_BYTEARRAY_AS_ARRAY
#define SERIALIZE_BYTEARRAY_AS_ARRAY 0
#endif

// ==================== 类型特征 ====================
// 判定是否为可直接交给 QVariant 处理的基础 JSON 类型。
// （QByteArray 不列入，因其走专属 Base64/数组分支，避免 QVariant 编解码歧义）
template<typename T>
struct is_basic_json_type : std::false_type {};
template<> struct is_basic_json_type<bool>            : std::true_type {};
template<> struct is_basic_json_type<int>             : std::true_type {};
template<> struct is_basic_json_type<unsigned int>    : std::true_type {};
template<> struct is_basic_json_type<long>            : std::true_type {};
template<> struct is_basic_json_type<unsigned long>   : std::true_type {};
template<> struct is_basic_json_type<short>            : std::true_type {};
template<> struct is_basic_json_type<unsigned short>   : std::true_type {};
template<> struct is_basic_json_type<float>           : std::true_type {};
template<> struct is_basic_json_type<double>          : std::true_type {};
// qint64/quint64 经 QVariant 会落到 JSON double，>2^53 有精度损失（JSON 数字固有限制）
template<> struct is_basic_json_type<qint64>          : std::true_type {};
template<> struct is_basic_json_type<quint64>         : std::true_type {};
template<> struct is_basic_json_type<QString>         : std::true_type {};
template<> struct is_basic_json_type<QDateTime>       : std::true_type {};
template<> struct is_basic_json_type<QDate>           : std::true_type {};
template<> struct is_basic_json_type<QTime>           : std::true_type {};
template<> struct is_basic_json_type<QUuid>           : std::true_type {};
template<> struct is_basic_json_type<QUrl>            : std::true_type {};
template<> struct is_basic_json_type<QJsonObject>     : std::true_type {};
template<> struct is_basic_json_type<QJsonArray>      : std::true_type {};
template<> struct is_basic_json_type<QJsonValue>      : std::true_type {};

// 检测自定义类型是否拥有 toJson()，且返回值可隐式转为 QJsonValue
template<typename T, typename = void>
struct has_to_json : std::false_type {};
template<typename T>
struct has_to_json<T, std::void_t<decltype(std::declval<const T>().toJson())>>
    : std::is_convertible<decltype(std::declval<const T>().toJson()), QJsonValue> {};

// 检测自定义类型是否拥有 fromJson(QJsonObject)，且返回值可隐式转为 bool
template<typename T, typename = void>
struct has_from_json : std::false_type {};
template<typename T>
struct has_from_json<T, std::void_t<decltype(std::declval<T>().fromJson(std::declval<QJsonObject>()))>>
    : std::is_convertible<decltype(std::declval<T>().fromJson(std::declval<QJsonObject>())), bool> {};

// 辅助：是否为已由非模板重载显式处理的类型（避免与基础/自定义模板分支产生歧义）
template<typename T>
struct is_specialized_value
    : std::integral_constant<bool,
        std::is_same<T, QByteArray>::value || std::is_same<T, std::string>::value> {};

// ==================== 序列化重载 ====================
// ----- 基础类型：经 QVariant 统一处理 -----
template<typename T>
std::enable_if_t<is_basic_json_type<T>::value, QJsonValue>
toJsonValue(const T& value) {
    return QJsonValue::fromVariant(QVariant::fromValue(value));
}

// ----- QByteArray：Base64 或数字数组（与反序列化严格对称）-----
#if SERIALIZE_BYTEARRAY_AS_ARRAY
// 数字数组模式：每个字节作为 0~255 的整数
inline QJsonValue toJsonValue(const QByteArray& value) {
    QJsonArray arr;
    for (const char c : value) {
        arr.append(static_cast<int>(static_cast<unsigned char>(c)));
    }
    return arr;
}
#else
// Base64 模式：二进制安全，可跨语言/跨平台往返
inline QJsonValue toJsonValue(const QByteArray& value) {
    return QJsonValue(QString::fromLatin1(value.toBase64()));
}
#endif

// ----- std::string：以 UTF-8 与 QString 互转 -----
inline QJsonValue toJsonValue(const std::string& value) {
    return QJsonValue(QString::fromStdString(value));
}

// ----- 自定义类型（拥有 toJson()）-----
template<typename T>
std::enable_if_t<!is_basic_json_type<T>::value && !is_specialized_value<T>::value
                 && has_to_json<T>::value, QJsonValue>
toJsonValue(const T& value) {
    return value.toJson();
}

// ----- 顺序容器：QList / QVector / std::vector / QSet / std::list -----
template<typename T>
QJsonValue toJsonValue(const QList<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}
template<typename T>
QJsonValue toJsonValue(const QVector<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}
template<typename T>
QJsonValue toJsonValue(const std::vector<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}
template<typename T>
QJsonValue toJsonValue(const QSet<T>& set) {
    QJsonArray arr;
    for (const auto& item : set) {
        arr.append(toJsonValue(item));
    }
    return arr;
}
template<typename T>
QJsonValue toJsonValue(const std::list<T>& list) {
    QJsonArray arr;
    for (const auto& item : list) {
        arr.append(toJsonValue(item));
    }
    return arr;
}

// ----- 二元组：QPair / std::pair -> 长度为 2 的数组 -----
template<typename T1, typename T2>
QJsonValue toJsonValue(const QPair<T1, T2>& p) {
    QJsonArray arr;
    arr.append(toJsonValue(p.first));
    arr.append(toJsonValue(p.second));
    return arr;
}
template<typename T1, typename T2>
QJsonValue toJsonValue(const std::pair<T1, T2>& p) {
    QJsonArray arr;
    arr.append(toJsonValue(p.first));
    arr.append(toJsonValue(p.second));
    return arr;
}

// ----- 关联容器：QMap / QHash / std::map / std::unordered_map -----
// JSON 对象键只能是字符串，非字符串键经 QVariant 转为其字符串形式
template<typename K, typename V>
QJsonValue toJsonValue(const QMap<K, V>& map) {
    QJsonObject obj;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        obj[QVariant::fromValue(it.key()).toString()] = toJsonValue(it.value());
    }
    return obj;
}
template<typename K, typename V>
QJsonValue toJsonValue(const QHash<K, V>& hash) {
    QJsonObject obj;
    for (auto it = hash.constBegin(); it != hash.constEnd(); ++it) {
        obj[QVariant::fromValue(it.key()).toString()] = toJsonValue(it.value());
    }
    return obj;
}
template<typename K, typename V>
QJsonValue toJsonValue(const std::map<K, V>& map) {
    QJsonObject obj;
    for (const auto& kv : map) {
        obj[QVariant::fromValue(kv.first).toString()] = toJsonValue(kv.second);
    }
    return obj;
}
template<typename K, typename V>
QJsonValue toJsonValue(const std::unordered_map<K, V>& map) {
    QJsonObject obj;
    for (const auto& kv : map) {
        obj[QVariant::fromValue(kv.first).toString()] = toJsonValue(kv.second);
    }
    return obj;
}

// ==================== 反序列化重载 ====================
// ----- 基础类型：经 QVariant 统一处理（保留宽松转换行为，向后兼容）-----
template<typename T>
std::enable_if_t<is_basic_json_type<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (value.isNull() || value.isUndefined()) {
        qWarning() << "[SERIALIZE] Json value is null/undefined";
        return false;
    }
    const QVariant var = value.toVariant();
    if (!var.canConvert<T>()) {
        qWarning() << "[SERIALIZE] Type convert fail, target:" << typeid(T).name();
        return false;
    }
    out = var.value<T>();
    return true;
}

// ----- QByteArray：与序列化严格对称 -----
#if SERIALIZE_BYTEARRAY_AS_ARRAY
// 数字数组模式：每个元素必须是 0~255 的整数
inline bool fromJsonValue(const QJsonValue& value, QByteArray& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QByteArray";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isDouble()) {
            qWarning() << "[SERIALIZE] QByteArray byte is not a number";
            return false;
        }
        const int byte = v.toInt();
        if (byte < 0 || byte > 255) {
            qWarning() << "[SERIALIZE] QByteArray byte out of range [0,255]:" << byte;
            return false;
        }
        out.append(static_cast<char>(byte));
    }
    return true;
}
#else
// Base64 模式
inline bool fromJsonValue(const QJsonValue& value, QByteArray& out) {
    if (value.isNull() || value.isUndefined()) {
        qWarning() << "[SERIALIZE] Json value is null/undefined for QByteArray";
        return false;
    }
    if (!value.isString()) {
        qWarning() << "[SERIALIZE] Expect base64 string for QByteArray";
        return false;
    }
    out = QByteArray::fromBase64(value.toString().toLatin1());
    return true;
}
#endif

// ----- std::string -----
inline bool fromJsonValue(const QJsonValue& value, std::string& out) {
    if (value.isNull() || value.isUndefined()) {
        qWarning() << "[SERIALIZE] Json value is null/undefined for std::string";
        return false;
    }
    if (!value.isString()) {
        qWarning() << "[SERIALIZE] Expect json string for std::string";
        return false;
    }
    out = value.toString().toStdString();
    return true;
}

// ----- 自定义类型（拥有 fromJson(QJsonObject)）-----
template<typename T>
std::enable_if_t<!is_basic_json_type<T>::value && !is_specialized_value<T>::value
                 && has_from_json<T>::value, bool>
fromJsonValue(const QJsonValue& value, T& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Custom type need json object:" << typeid(T).name();
        return false;
    }
    const bool ok = out.fromJson(value.toObject());
    if (!ok) {
        qWarning() << "[SERIALIZE] Custom type parse failed:" << typeid(T).name();
    }
    return ok;
}

// ----- 顺序容器反序列化：QList / QVector / std::vector / QSet / std::list -----
template<typename T>
bool fromJsonValue(const QJsonValue& value, QList<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QList";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] QList item parse fail at index" << idx;
            return false;
        }
        out.append(std::move(t));
        ++idx;
    }
    return true;
}
template<typename T>
bool fromJsonValue(const QJsonValue& value, QVector<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QVector";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] QVector item parse fail at index" << idx;
            return false;
        }
        out.append(std::move(t));
        ++idx;
    }
    return true;
}
template<typename T>
bool fromJsonValue(const QJsonValue& value, std::vector<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for std::vector";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    out.reserve(static_cast<size_t>(arr.size()));
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] std::vector item parse fail at index" << idx;
            return false;
        }
        out.push_back(std::move(t));
        ++idx;
    }
    return true;
}
template<typename T>
bool fromJsonValue(const QJsonValue& value, QSet<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for QSet";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    out.reserve(arr.size());
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] QSet item parse fail at index" << idx;
            return false;
        }
        out.insert(std::move(t));
        ++idx;
    }
    return true;
}
template<typename T>
bool fromJsonValue(const QJsonValue& value, std::list<T>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array for std::list";
        return false;
    }
    const QJsonArray arr = value.toArray();
    out.clear();
    int idx = 0;
    for (const auto& item : arr) {
        T t;
        if (!fromJsonValue(item, t)) {
            qWarning() << "[SERIALIZE] std::list item parse fail at index" << idx;
            return false;
        }
        out.push_back(std::move(t));
        ++idx;
    }
    return true;
}

// ----- 二元组反序列化：QPair / std::pair（数组长度必须为 2）-----
template<typename T1, typename T2>
bool fromJsonValue(const QJsonValue& value, QPair<T1, T2>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array(2) for QPair";
        return false;
    }
    const QJsonArray arr = value.toArray();
    if (arr.size() != 2) {
        qWarning() << "[SERIALIZE] QPair array size != 2:" << arr.size();
        return false;
    }
    if (!fromJsonValue(arr.at(0), out.first)) {
        qWarning() << "[SERIALIZE] QPair first parse fail";
        return false;
    }
    if (!fromJsonValue(arr.at(1), out.second)) {
        qWarning() << "[SERIALIZE] QPair second parse fail";
        return false;
    }
    return true;
}
template<typename T1, typename T2>
bool fromJsonValue(const QJsonValue& value, std::pair<T1, T2>& out) {
    if (!value.isArray()) {
        qWarning() << "[SERIALIZE] Expect json array(2) for std::pair";
        return false;
    }
    const QJsonArray arr = value.toArray();
    if (arr.size() != 2) {
        qWarning() << "[SERIALIZE] std::pair array size != 2:" << arr.size();
        return false;
    }
    if (!fromJsonValue(arr.at(0), out.first)) {
        qWarning() << "[SERIALIZE] std::pair first parse fail";
        return false;
    }
    if (!fromJsonValue(arr.at(1), out.second)) {
        qWarning() << "[SERIALIZE] std::pair second parse fail";
        return false;
    }
    return true;
}

// ----- 关联容器反序列化：QMap / QHash / std::map / std::unordered_map -----
// JSON 键恒为字符串，非字符串键经 QVariant 转回目标类型
// 限制：键类型需可由 QString 经 QVariant 转换（如 QString/数值类型），
//       std::string 等类型 QVariant 无法识别，将返回失败
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QMap<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for QMap";
        return false;
    }
    const QJsonObject obj = value.toObject();
    out.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] QMap key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        const K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] QMap value parse fail, key:" << it.key();
            return false;
        }
        out[key] = std::move(val);
    }
    return true;
}
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, QHash<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for QHash";
        return false;
    }
    const QJsonObject obj = value.toObject();
    out.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] QHash key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        const K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] QHash value parse fail, key:" << it.key();
            return false;
        }
        out[key] = std::move(val);
    }
    return true;
}
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, std::map<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for std::map";
        return false;
    }
    const QJsonObject obj = value.toObject();
    out.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] std::map key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        const K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] std::map value parse fail, key:" << it.key();
            return false;
        }
        out[key] = std::move(val);
    }
    return true;
}
template<typename K, typename V>
bool fromJsonValue(const QJsonValue& value, std::unordered_map<K, V>& out) {
    if (!value.isObject()) {
        qWarning() << "[SERIALIZE] Expect json object for std::unordered_map";
        return false;
    }
    const QJsonObject obj = value.toObject();
    out.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QVariant keyVar(it.key());
        if (!keyVar.canConvert<K>()) {
            qWarning() << "[SERIALIZE] std::unordered_map key convert fail, json key:" << it.key()
                       << " target type:" << typeid(K).name();
            return false;
        }
        const K key = keyVar.value<K>();
        V val;
        if (!fromJsonValue(it.value(), val)) {
            qWarning() << "[SERIALIZE] std::unordered_map value parse fail, key:" << it.key();
            return false;
        }
        out[key] = std::move(val);
    }
    return true;
}

// ==================== 预处理可变参数宏（扩展至 20 字段） ====================
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N
#define PP_RSEQ_N() 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

// 序列化：写字段。使用 QLatin1String 字面量避免逐次构造 QString
// 自终止表达式语句（带尾分号），便于在 SERIALIZE_N 梯中无分号拼接
#define SERIALIZE_FIELD(obj, field) \
    obj[QLatin1String(#field)] = toJsonValue(field);

// 可选字段反序列化：缺失则跳过；存在但解析失败则标记错误
// 用 do{...}while(0); 包裹以避免悬挂 else 等卫生问题，并自终止（带尾分号）
#define DESERIALIZE_FIELD_SAFE(obj, field, parseOkFlag) \
    do { \
        if (obj.contains(QLatin1String(#field))) { \
            if (!fromJsonValue(obj[QLatin1String(#field)], field)) { \
                qWarning() << "[SERIALIZE] Optional field parse failed:" << #field; \
                parseOkFlag = false; \
            } \
        } \
    } while (0);

// 必填字段反序列化：缺失或解析失败均标记错误（供手写 fromJson 使用）
#define DESERIALIZE_FIELD_REQUIRED(obj, field, parseOkFlag) \
    do { \
        if (!obj.contains(QLatin1String(#field))) { \
            qWarning() << "[SERIALIZE] Required field missing:" << #field; \
            parseOkFlag = false; \
        } else if (!fromJsonValue(obj[QLatin1String(#field)], field)) { \
            qWarning() << "[SERIALIZE] Required field parse failed:" << #field; \
            parseOkFlag = false; \
        } \
    } while (0);

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
#define SERIALIZE_11(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) SERIALIZE_10(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10) SERIALIZE_FIELD(obj,f11)
#define SERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) SERIALIZE_11(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) SERIALIZE_FIELD(obj,f12)
#define SERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) SERIALIZE_12(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) SERIALIZE_FIELD(obj,f13)
#define SERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) SERIALIZE_13(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) SERIALIZE_FIELD(obj,f14)
#define SERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) SERIALIZE_14(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) SERIALIZE_FIELD(obj,f15)
#define SERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) SERIALIZE_15(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) SERIALIZE_FIELD(obj,f16)
#define SERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) SERIALIZE_16(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) SERIALIZE_FIELD(obj,f17)
#define SERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) SERIALIZE_17(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) SERIALIZE_FIELD(obj,f18)
#define SERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) SERIALIZE_18(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) SERIALIZE_FIELD(obj,f19)
#define SERIALIZE_20(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20) SERIALIZE_19(obj,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) SERIALIZE_FIELD(obj,f20)

// ==================== DESERIALIZE_N 反序列化分支（携带 ok 标记，1~20） ====================
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

// ==================== 顶层序列化宏（生成四个方法，解析状态与错误打印齐备） ====================
// 展开为成员函数 toJson / fromJson / toByteArray / fromByteArray
#define SERIALIZE(...) \
    QJsonObject toJson() const { \
        QJsonObject obj; \
        CONCAT(SERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__) \
        return obj; \
    } \
    bool fromJson(const QJsonObject& obj) { \
        bool serializeParseOk = true; \
        CONCAT(DESERIALIZE_, PP_NARG(__VA_ARGS__))(obj, __VA_ARGS__, serializeParseOk) \
        return serializeParseOk; \
    } \
    QByteArray toByteArray() const { \
        return QJsonDocument(toJson()).toJson(QJsonDocument::Compact); \
    } \
    bool fromByteArray(const QByteArray& data) { \
        QJsonParseError parseError; \
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError); \
        if (parseError.error != QJsonParseError::NoError) { \
            qWarning() << "[SERIALIZE] Json parse error:" << parseError.errorString() \
                       << " offset:" << parseError.offset; \
            return false; \
        } \
        if (!doc.isObject()) { \
            qWarning() << "[SERIALIZE] Root json is not object"; \
            return false; \
        } \
        return fromJson(doc.object()); \
    }

#endif // SERIALIZE_H
