#include "FlexData.h"
#include <QSharedData>
#include <QHash>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDataStream>
#include <QBuffer>
#include <QSettings>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QDebug>
#include <QMutexLocker>
#include <climits>

// 自动注册元类型，支持跨线程信号槽传递
static int flexDataMetaTypeId = qRegisterMetaType<FlexData>();

// ============================= 私有数据类 FlexDataPrivate =============================
class FlexDataPrivate : public QSharedData
{
public:
    enum Type {
        TypeNull, TypeBool, TypeInt, TypeDouble, TypeString,
        TypeByteArray, TypeDateTime, TypeUuid,
        TypeMap, TypeArray
    };

    Type type;
    // 联合体存储具体数据（复杂类型用指针）
    union {
        bool b;
        int i;
        double d;
        QString* s;
        QByteArray* ba;
        QDateTime* dt;
        QUuid* uuid;
        QHash<QString, FlexData>* map;
        QList<FlexData>* list;
    };

    // 构造函数（每个只初始化自己对应的成员）
    FlexDataPrivate() : type(TypeNull) {}
    explicit FlexDataPrivate(bool value) : type(TypeBool), b(value) {}
    explicit FlexDataPrivate(int value) : type(TypeInt), i(value) {}
    explicit FlexDataPrivate(double value) : type(TypeDouble), d(value) {}
    explicit FlexDataPrivate(const QString& value) : type(TypeString), s(new QString(value)) {}
    explicit FlexDataPrivate(const QByteArray& value) : type(TypeByteArray), ba(new QByteArray(value)) {}
    explicit FlexDataPrivate(const QDateTime& value) : type(TypeDateTime), dt(new QDateTime(value)) {}
    explicit FlexDataPrivate(const QUuid& value) : type(TypeUuid), uuid(new QUuid(value)) {}
    explicit FlexDataPrivate(const QHash<QString, FlexData>& value) : type(TypeMap), map(new QHash<QString, FlexData>(value)) {}
    explicit FlexDataPrivate(const QList<FlexData>& value) : type(TypeArray), list(new QList<FlexData>(value)) {}

    // 拷贝构造（深拷贝指针内容）
    FlexDataPrivate(const FlexDataPrivate& other) : QSharedData(other), type(other.type) {
        switch (type) {
        case TypeBool: b = other.b; break;
        case TypeInt: i = other.i; break;
        case TypeDouble: d = other.d; break;
        case TypeString: s = new QString(*other.s); break;
        case TypeByteArray: ba = new QByteArray(*other.ba); break;
        case TypeDateTime: dt = new QDateTime(*other.dt); break;
        case TypeUuid: uuid = new QUuid(*other.uuid); break;
        case TypeMap: map = new QHash<QString, FlexData>(*other.map); break;
        case TypeArray: list = new QList<FlexData>(*other.list); break;
        default: break;
        }
    }

    // 析构函数：释放动态分配的内存
    ~FlexDataPrivate() {
        switch (type) {
        case TypeString: delete s; break;
        case TypeByteArray: delete ba; break;
        case TypeDateTime: delete dt; break;
        case TypeUuid: delete uuid; break;
        case TypeMap: delete map; break;
        case TypeArray: delete list; break;
        default: break;
        }
    }

    // 赋值运算符已移除：QSharedDataPointer 仅使用拷贝构造，无需对象级赋值
    // 避免 placement new 带来的维护风险与引用计数异常
};

// ============================= FlexData 构造函数 / 析构函数 =============================
FlexData::FlexData() : d(new FlexDataPrivate) {}
FlexData::FlexData(bool v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(int v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(double v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(const QString& v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(const char* v) : d(new FlexDataPrivate(QString::fromUtf8(v))) {}
FlexData::FlexData(const QByteArray& v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(const QDateTime& v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(const QUuid& v) : d(new FlexDataPrivate(v)) {}
FlexData::FlexData(const QHash<QString, FlexData>& map) : d(new FlexDataPrivate(map)) {}
FlexData::FlexData(const QList<FlexData>& list) : d(new FlexDataPrivate(list)) {}
FlexData::FlexData(const FlexData& other)
    : d(other.d)
    , m_pathCache(other.m_pathCache)
    // m_pathCacheMutex 不拷贝，每个对象拥有独立的锁
{
}

FlexData::~FlexData() = default;
// 拷贝赋值：拷贝数据与缓存，互斥锁保持自身独立
FlexData& FlexData::operator=(const FlexData& other)
{
    if (this != &other) {
        d = other.d;
        m_pathCache = other.m_pathCache;
        // m_pathCacheMutex 不赋值，保留自身锁实例
    }
    return *this;
}

// ============================= 类型判断 =============================
FlexData::Type FlexData::type() const { return static_cast<Type>(d->type); }
bool FlexData::isNull() const { return d->type == FlexDataPrivate::TypeNull; }
bool FlexData::isBool() const { return d->type == FlexDataPrivate::TypeBool; }
bool FlexData::isInt() const { return d->type == FlexDataPrivate::TypeInt; }
bool FlexData::isDouble() const { return d->type == FlexDataPrivate::TypeDouble; }
bool FlexData::isString() const { return d->type == FlexDataPrivate::TypeString; }
bool FlexData::isByteArray() const { return d->type == FlexDataPrivate::TypeByteArray; }
bool FlexData::isDateTime() const { return d->type == FlexDataPrivate::TypeDateTime; }
bool FlexData::isUuid() const { return d->type == FlexDataPrivate::TypeUuid; }
bool FlexData::isMap() const { return d->type == FlexDataPrivate::TypeMap; }
bool FlexData::isArray() const { return d->type == FlexDataPrivate::TypeArray; }

// ============================= 安全类型转换 =============================
bool FlexData::toBool(bool defaultValue) const {
    switch (d->type) {
    case FlexDataPrivate::TypeBool: return d->b;
    case FlexDataPrivate::TypeInt: return d->i != 0;
    case FlexDataPrivate::TypeDouble: return d->d != 0.0;
    case FlexDataPrivate::TypeString: return !d->s->isEmpty();
    case FlexDataPrivate::TypeByteArray: return !d->ba->isEmpty();
    case FlexDataPrivate::TypeDateTime: return d->dt->isValid();
    case FlexDataPrivate::TypeUuid: return !d->uuid->isNull();
    default: return defaultValue;
    }
}

int FlexData::toInt(int defaultValue) const {
    switch (d->type) {
    case FlexDataPrivate::TypeBool: return d->b ? 1 : 0;
    case FlexDataPrivate::TypeInt: return d->i;
    case FlexDataPrivate::TypeDouble: return static_cast<int>(d->d);
    case FlexDataPrivate::TypeString: return d->s->toInt();
    default: return defaultValue;
    }
}

double FlexData::toDouble(double defaultValue) const {
    switch (d->type) {
    case FlexDataPrivate::TypeBool: return d->b ? 1.0 : 0.0;
    case FlexDataPrivate::TypeInt: return static_cast<double>(d->i);
    case FlexDataPrivate::TypeDouble: return d->d;
    case FlexDataPrivate::TypeString: return d->s->toDouble();
    default: return defaultValue;
    }
}

QString FlexData::toString(const QString& defaultValue) const {
    switch (d->type) {
    case FlexDataPrivate::TypeBool: return d->b ? "true" : "false";
    case FlexDataPrivate::TypeInt: return QString::number(d->i);
    case FlexDataPrivate::TypeDouble: return QString::number(d->d);
    case FlexDataPrivate::TypeString: return *d->s;
    case FlexDataPrivate::TypeByteArray: return QString::fromUtf8(d->ba->toBase64());
    case FlexDataPrivate::TypeDateTime: return d->dt->toString(Qt::ISODate);
    case FlexDataPrivate::TypeUuid: return d->uuid->toString();
    default: return defaultValue;
    }
}

QByteArray FlexData::toByteArray(const QByteArray& defaultValue) const {
    if (isByteArray()) return *d->ba;
    if (isString()) return d->s->toUtf8();
    return defaultValue;
}

QDateTime FlexData::toDateTime(const QDateTime& defaultValue) const {
    if (isDateTime()) return *d->dt;
    if (isString()) return QDateTime::fromString(*d->s, Qt::ISODate);
    return defaultValue;
}

QUuid FlexData::toUuid(const QUuid& defaultValue) const {
    if (isUuid()) return *d->uuid;
    if (isString()) return QUuid(*d->s);
    return defaultValue;
}

QHash<QString, FlexData> FlexData::toMap() const {
    if (isMap()) return *d->map;
    return QHash<QString, FlexData>();
}

QList<FlexData> FlexData::toArray() const {
    if (isArray()) return *d->list;
    return QList<FlexData>();
}

// ============================= Map / Array 辅助 =============================
void FlexData::ensureMapType() {
    if (isMap()) return;
    d = QSharedDataPointer<FlexDataPrivate>(new FlexDataPrivate(QHash<QString, FlexData>()));
}

void FlexData::ensureArrayType() {
    if (isArray()) return;
    d = QSharedDataPointer<FlexDataPrivate>(new FlexDataPrivate(QList<FlexData>()));
}

void FlexData::toMapType() { ensureMapType(); }
void FlexData::toArrayType() { ensureArrayType(); }

FlexData& FlexData::operator[](const QString& key) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureMapType();
    return (*d->map)[key];
}

const FlexData FlexData::operator[](const QString& key) const {
    if (isMap() && d->map->contains(key))
        return d->map->value(key);
    return FlexData();
}

void FlexData::insert(const QString& key, const FlexData& value) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureMapType();
    d->map->insert(key, value);
}

void FlexData::remove(const QString& key) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    if (isMap()) d->map->remove(key);
}

bool FlexData::contains(const QString& key) const {
    return isMap() && d->map->contains(key);
}

QStringList FlexData::keys() const {
    return isMap() ? d->map->keys() : QStringList();
}

int FlexData::mapSize() const {
    return isMap() ? d->map->size() : 0;
}

FlexData& FlexData::operator[](int index) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureArrayType();
    if (index < 0) {
        Q_ASSERT_X(false, "FlexData::operator[]", "negative index is not allowed");
        index = 0;
    }
    if (index >= d->list->size()) {
        for (int i = d->list->size(); i <= index; ++i) {
            d->list->append(FlexData());
        }
    }
    return (*d->list)[index];
}

const FlexData FlexData::operator[](int index) const {
    if (isArray() && index >= 0 && index < d->list->size())
        return d->list->at(index);
    return FlexData();
}

void FlexData::append(const FlexData& value) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureArrayType();
    d->list->append(value);
}

void FlexData::insert(int index, const FlexData& value) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureArrayType();
    if (index < 0) index = 0;
    if (index > d->list->size()) index = d->list->size();
    d->list->insert(index, value);
}

void FlexData::removeAt(int index) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    if (isArray() && index >= 0 && index < d->list->size())
        d->list->removeAt(index);
}

int FlexData::arraySize() const {
    return isArray() ? d->list->size() : 0;
}

void FlexData::setValue(const QString& key, const FlexData& value) { insert(key, value); }
FlexData FlexData::value(const QString& key) const { return operator[](key); }

// ============================= 路径访问实现 =============================
QStringList FlexData::splitPath(const QString& path) const {
    QMutexLocker locker(&m_pathCacheMutex); // 线程安全保护缓存
    // 检查缓存
    auto it = m_pathCache.find(path);
    if (it != m_pathCache.end())
        return *it;

    QStringList parts;
    QString trimmed = path;
    if (trimmed.startsWith('/')) trimmed = trimmed.mid(1);
    if (trimmed.endsWith('/')) trimmed.chop(1);
    if (!trimmed.isEmpty()) {
        // 使用 QRegularExpression 实现负向后顾，支持转义斜杠（Qt 5.12 原生支持）
        QRegularExpression sep("(?<!\\\\)/");
        QStringList raw = trimmed.split(sep, QString::SkipEmptyParts);
        for (QString part : raw) {
            part.replace("\\/", "/"); // 还原转义
            // 处理数组索引 [0]
            if (part.startsWith('[') && part.endsWith(']')) {
                QString idxStr = part.mid(1, part.length() - 2);
                parts.append(idxStr);
            } else {
                parts.append(part);
            }
        }
    }
    m_pathCache[path] = parts;
    return parts;
}

FlexData FlexData::get(const QString& path) const {
    QStringList parts = splitPath(path);
    FlexData current = *this;
    for (const QString& part : parts) {
        if (current.isMap()) {
            if (!current.contains(part)) return FlexData();
            current = current[part];
        } else if (current.isArray()) {
            bool ok;
            int idx = part.toInt(&ok);
            if (!ok || idx < 0 || idx >= current.arraySize()) return FlexData();
            current = current[idx];
        } else {
            return FlexData();
        }
    }
    return current;
}

void FlexData::set(const QString& path, const FlexData& value) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    QStringList parts = splitPath(path);
    if (parts.isEmpty()) {
        *this = value;
        return;
    }
    FlexData* node = this;
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString& part = parts[i];
        if (node->isNull()) node->toMapType();
        if (node->isMap()) {
            if (!node->contains(part)) node->insert(part, FlexData());
            node = &(*node)[part];
        } else if (node->isArray()) {
            bool ok;
            int idx = part.toInt(&ok);
            if (!ok) {
                qWarning() << "FlexData::set: invalid array index" << part << "in path" << path;
                return;
            }
            if (idx >= node->arraySize()) {
                for (int j = node->arraySize(); j <= idx; ++j)
                    node->append(FlexData());
            }
            node = &(*node)[idx];
        } else {
            qWarning() << "FlexData::set: cannot set path" << path << ", intermediate node is not map or array";
            return;
        }
    }
    const QString& last = parts.last();
    if (node->isNull()) node->toMapType();
    if (node->isMap()) {
        (*node)[last] = value;
    } else if (node->isArray()) {
        bool ok;
        int idx = last.toInt(&ok);
        if (ok) {
            if (idx >= node->arraySize())
                node->operator[](idx);
            (*node)[idx] = value;
        } else {
            qWarning() << "FlexData::set: invalid array index" << last << "in path" << path;
        }
    }
}

bool FlexData::has(const QString& path) const {
    return get(path).type() != Null;
}

void FlexData::removePath(const QString& path) {
    d.detach(); // 写前分离，确保隐式共享语义正确
    QStringList parts = splitPath(path);
    if (parts.isEmpty()) {
        *this = FlexData();
        return;
    }
    FlexData parent = get(parts.mid(0, parts.size() - 1).join('/'));
    if (parent.isNull()) return;
    const QString& last = parts.last();
    if (parent.isMap()) {
        parent.remove(last);
        set(parts.mid(0, parts.size() - 1).join('/'), parent);
    } else if (parent.isArray()) {
        bool ok;
        int idx = last.toInt(&ok);
        if (ok && idx >= 0 && idx < parent.arraySize()) {
            parent.removeAt(idx);
            set(parts.mid(0, parts.size() - 1).join('/'), parent);
        }
    }
}

FlexData FlexData::value(const QString &path, const FlexData &defaultValue) const
{
    FlexData val = get(path);
    return val.isNull() ? defaultValue : val;
}

void FlexData::reserveMap(int capacity)
{
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureMapType();
    d->map->reserve(capacity);
}

void FlexData::reserveArray(int capacity)
{
    d.detach(); // 写前分离，确保隐式共享语义正确
    ensureArrayType();
    d->list->reserve(capacity);
}

// ============================= Schema 校验 =============================
bool FlexData::validate(const FlexData& schema, QStringList* errors) const {
    if (!schema.isMap()) {
        if (errors) errors->append("Schema must be a Map");
        return false;
    }
    bool valid = true;
    for (const QString& key : schema.keys()) {
        FlexData rule = schema[key];
        // 简写模式：直接指定类型字符串
        if (!rule.isMap()) {
            QString expectedType = rule.toString();
            FlexData val = value(key); // 修复：校验当前字段而非整个对象
            if (expectedType == "string" && !val.isString()) {
                valid = false; if (errors) errors->append(key + ": expected string");
            } else if (expectedType == "int" && !val.isInt()) {
                valid = false; if (errors) errors->append(key + ": expected int");
            } else if (expectedType == "double" && !val.isDouble()) {
                valid = false; if (errors) errors->append(key + ": expected double");
            } else if (expectedType == "bool" && !val.isBool()) {
                valid = false; if (errors) errors->append(key + ": expected bool");
            }
            continue;
        }

        QString type = rule["type"].toString();
        bool required = rule["required"].toBool(false);

        if (!contains(key)) {
            if (required) {
                valid = false;
                if (errors) errors->append(key + " is required");
            }
            continue;
        }

        FlexData val = (*this)[key];
        if (type == "string" && !val.isString()) { valid=false; if(errors) errors->append(key+": must be string"); }
        else if (type == "int" && !val.isInt()) { valid=false; if(errors) errors->append(key+": must be int"); }
        else if (type == "double" && !val.isDouble()) { valid=false; if(errors) errors->append(key+": must be double"); }
        else if (type == "bool" && !val.isBool()) { valid=false; if(errors) errors->append(key+": must be bool"); }

        if (type == "int") {
            int min = rule["min"].toInt();
            int max = rule["max"].toInt();
            int v = val.toInt();
            if (v < min || v > max) { valid=false; if(errors) errors->append(key+": out of range"); }
        } else if (type == "double") {
            double min = rule["min"].toDouble();
            double max = rule["max"].toDouble();
            double v = val.toDouble();
            if (v < min || v > max) { valid=false; if(errors) errors->append(key+": out of range"); }
        }

        if (rule.contains("enum")) {
            FlexData enumArr = rule["enum"];
            if (enumArr.isArray()) {
                bool found = false;
                for (int i=0; i<enumArr.arraySize(); ++i) {
                    if (val.toString() == enumArr[i].toString()) { found=true; break; }
                }
                if (!found) { valid=false; if(errors) errors->append(key+": invalid enum value"); }
            }
        }

        if (rule.contains("regex")) {
            QRegularExpression re(rule["regex"].toString());
            if (!re.match(val.toString()).hasMatch()) { valid=false; if(errors) errors->append(key+": regex mismatch"); }
        }
    }
    return valid;
}

FlexData FlexData::applyDefaults(const FlexData& schema) const {
    FlexData result = *this;
    if (!schema.isMap()) return result;
    for (const QString& key : schema.keys()) {
        FlexData rule = schema[key];
        if (!result.contains(key) && rule.isMap() && rule.contains("default")) {
            result.insert(key, rule["default"]);
        }
    }
    return result;
}

// ============================= 合并与差量 =============================
FlexData FlexData::merge(const FlexData& other) const {
    if (isNull()) return other;
    if (other.isNull()) return *this;
    if (type() != other.type()) return other;
    if (isMap()) {
        FlexData merged = *this;
        for (const QString& key : other.keys()) {
            if (merged.contains(key))
                merged.insert(key, merged[key].merge(other[key]));
            else
                merged.insert(key, other[key]);
        }
        return merged;
    } else if (isArray()) {
        FlexData merged = *this;
        for (int i=0; i<other.arraySize(); ++i)
            merged.append(other[i]);
        return merged;
    } else {
        return other;
    }
}

bool operator==(const FlexData& a, const FlexData& b) {
    if (a.type() != b.type()) return false;
    if (a.isNull()) return true;
    if (a.isBool()) return a.toBool() == b.toBool();
    if (a.isInt()) return a.toInt() == b.toInt();
    if (a.isDouble()) return a.toDouble() == b.toDouble();
    if (a.isString()) return a.toString() == b.toString();
    if (a.isByteArray()) return a.toByteArray() == b.toByteArray();
    if (a.isDateTime()) return a.toDateTime() == b.toDateTime();
    if (a.isUuid()) return a.toUuid() == b.toUuid();
    if (a.isMap()) {
        if (a.mapSize() != b.mapSize()) return false;
        for (const QString& key : a.keys()) {
            if (!b.contains(key)) return false;
            if (!(a[key] == b[key])) return false;
        }
        return true;
    }
    if (a.isArray()) {
        if (a.arraySize() != b.arraySize()) return false;
        for (int i=0; i<a.arraySize(); ++i) {
            if (!(a[i] == b[i])) return false;
        }
        return true;
    }
    return false;
}

FlexData FlexData::diff(const FlexData& target) const {
    FlexData patch(Array);
    if (*this == target) return patch;
    if (type() != target.type() || (!isMap() && !isArray())) {
        FlexData op(Map);
        op["op"] = "replace";
        op["path"] = "";
        op["value"] = target;
        patch.append(op);
        return patch;
    }
    if (isMap()) {
        QHash<QString, FlexData> thisMap = toMap();
        QHash<QString, FlexData> targetMap = target.toMap();
        for (const QString& key : thisMap.keys()) {
            if (!targetMap.contains(key)) {
                FlexData op(Map);
                op["op"] = "remove";
                op["path"] = "/" + key;
                patch.append(op);
            }
        }
        for (const QString& key : targetMap.keys()) {
            if (!thisMap.contains(key)) {
                FlexData op(Map);
                op["op"] = "add";
                op["path"] = "/" + key;
                op["value"] = targetMap[key];
                patch.append(op);
            } else if (!(thisMap[key] == targetMap[key])) {
                FlexData subPatch = thisMap[key].diff(targetMap[key]);
                for (int i=0; i<subPatch.arraySize(); ++i) {
                    FlexData subOp = subPatch[i];
                    QString subPath = subOp["path"].toString();
                    subOp["path"] = "/" + key + subPath;
                    patch.append(subOp);
                }
            }
        }
        return patch;
    }
    if (isArray()) {
        FlexData op(Map);
        op["op"] = "replace";
        op["path"] = "";
        op["value"] = target;
        patch.append(op);
        return patch;
    }
    return patch;
}

FlexData FlexData::patch(const FlexData& patch) const {
    FlexData result = *this;
    if (!patch.isArray()) return result;
    for (int i=0; i<patch.arraySize(); ++i) {
        FlexData op = patch[i];
        if (!op.isMap()) continue;
        QString operation = op["op"].toString();
        QString path = op["path"].toString();
        if (operation == "replace" || operation == "add") {
            result.set(path, op["value"]);
        } else if (operation == "remove") {
            result.removePath(path);
        }
    }
    return result;
}

// ============================= 并行快照 =============================
FlexData FlexData::clone() const {
    // 直接调用私有数据拷贝构造实现深拷贝，性能远高于序列化往返
    return FlexData(new FlexDataPrivate(*d));
}

FlexData FlexData::freeze() const { return clone(); }

// ============================= JSON 序列化 =============================
static QJsonValue toJsonValue(const FlexData& data) {
    switch (data.type()) {
    case FlexData::Null: return QJsonValue();
    case FlexData::Bool: return QJsonValue(data.toBool());
    case FlexData::Int: return QJsonValue(data.toInt());
    case FlexData::Double: return QJsonValue(data.toDouble());
    case FlexData::String: return QJsonValue(data.toString());
    case FlexData::ByteArray: return QJsonValue(QString::fromUtf8(data.toByteArray().toBase64()));
    case FlexData::DateTime: return QJsonValue(data.toDateTime().toString(Qt::ISODate));
    case FlexData::Uuid: return QJsonValue(data.toUuid().toString());
    case FlexData::Map: {
        QJsonObject obj;
        for (auto it = data.toMap().begin(); it != data.toMap().end(); ++it)
            obj[it.key()] = toJsonValue(it.value());
        return obj;
    }
    case FlexData::Array: {
        QJsonArray arr;
        for (const auto& item : data.toArray())
            arr.append(toJsonValue(item));
        return arr;
    }
    }
    return QJsonValue();
}

static FlexData fromJsonValue(const QJsonValue& val) {
    switch (val.type()) {
    case QJsonValue::Null: return FlexData();
    case QJsonValue::Bool: return FlexData(val.toBool());
    case QJsonValue::Double: {
        double v = val.toDouble();
        // 整数还原：值为整数且在 int 范围内时恢复为 Int 类型
        if (v == qRound(v) && v >= INT_MIN && v <= INT_MAX) {
            return FlexData(static_cast<int>(v));
        }
        return FlexData(v);
    }
    case QJsonValue::String: return FlexData(val.toString());
    case QJsonValue::Object: {
        QHash<QString, FlexData> map;
        QJsonObject obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            map[it.key()] = fromJsonValue(it.value());
        return FlexData(map);
    }
    case QJsonValue::Array: {
        QList<FlexData> list;
        QJsonArray arr = val.toArray();
        for (const auto& item : arr)
            list.append(fromJsonValue(item));
        return FlexData(list);
    }
    }
    return FlexData();
}

QByteArray FlexData::toJson(bool compact) const {
    QJsonDocument doc;
    switch (type()) {
    case Map: {
        QJsonObject obj = toJsonValue(*this).toObject();
        doc = QJsonDocument(obj);
        break;
    }
    case Array: {
        QJsonArray arr = toJsonValue(*this).toArray();
        doc = QJsonDocument(arr);
        break;
    }
    default: {
        QJsonValue val = toJsonValue(*this);
        if (val.isObject()) {
            doc = QJsonDocument(val.toObject());
        } else if (val.isArray()) {
            doc = QJsonDocument(val.toArray());
        } else {
            // 顶层为基础类型：包裹为数组序列化后提取纯值内容
            QJsonArray arr;
            arr.append(val);
            QJsonDocument arrDoc(arr);
            QByteArray arrJson = compact ? arrDoc.toJson(QJsonDocument::Compact) : arrDoc.toJson();
            arrJson = arrJson.trimmed();
            if (arrJson.startsWith('[') && arrJson.endsWith(']')) {
                arrJson = arrJson.mid(1, arrJson.size() - 2).trimmed();
            }
            return arrJson;
        }
        break;
    }
    }
    return compact ? doc.toJson(QJsonDocument::Compact) : doc.toJson();
}

bool FlexData::fromJson(const QByteArray& json) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        // 解析失败：尝试作为顶层值处理，包裹为数组格式再解析
        QByteArray wrappedJson = "[" + json + "]";
        doc = QJsonDocument::fromJson(wrappedJson, &err);
        if (err.error != QJsonParseError::NoError) return false;
        if (doc.isArray() && doc.array().size() == 1) {
            *this = fromJsonValue(doc.array().first());
            return true;
        }
        return false;
    }
    *this = fromJsonValue(doc.isObject() ? QJsonValue(doc.object()) :
                          doc.isArray()  ? QJsonValue(doc.array()) : QJsonValue());
    return true;
}

// ============================= XML 序列化 =============================
static void writeXmlValue(QXmlStreamWriter& writer, const FlexData& data, const QString& tagName) {
    if (data.isMap()) {
        writer.writeStartElement(tagName);
        writer.writeAttribute("type", "map");
        for (auto it = data.toMap().begin(); it != data.toMap().end(); ++it) {
            writeXmlValue(writer, it.value(), it.key());
        }
        writer.writeEndElement();
    } else if (data.isArray()) {
        writer.writeStartElement(tagName);
        writer.writeAttribute("type", "array");
        for (const auto& item : data.toArray()) {
            writeXmlValue(writer, item, "item");
        }
        writer.writeEndElement();
    } else {
        writer.writeStartElement(tagName);
        // 写入类型属性，保证反序列化时类型不丢失
        QString typeStr;
        switch (data.type()) {
        case FlexData::Bool: typeStr = "bool"; break;
        case FlexData::Int: typeStr = "int"; break;
        case FlexData::Double: typeStr = "double"; break;
        case FlexData::String: typeStr = "string"; break;
        case FlexData::ByteArray: typeStr = "bytearray"; break;
        case FlexData::DateTime: typeStr = "datetime"; break;
        case FlexData::Uuid: typeStr = "uuid"; break;
        default: typeStr = "null"; break;
        }
        writer.writeAttribute("type", typeStr);
        writer.writeCharacters(data.toString());
        writer.writeEndElement();
    }
}

QByteArray FlexData::toXml(const QString& rootName) const {
    QByteArray out;
    QXmlStreamWriter writer(&out);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writeXmlValue(writer, *this, rootName);
    writer.writeEndDocument();
    return out;
}

static FlexData readXmlValue(QXmlStreamReader& reader) {
    if (reader.tokenType() != QXmlStreamReader::StartElement)
        return FlexData();
    QString name = reader.name().toString();
    QString typeStr = reader.attributes().value("type").toString(); // 读取类型属性
    reader.readNext();

    // 叶子节点：根据 type 属性还原原始类型
    if (reader.tokenType() == QXmlStreamReader::Characters && !reader.isWhitespace()) {
        QString text = reader.text().toString();
        reader.readNext(); // 推进到 EndElement
        if (typeStr == "bool") {
            return FlexData(text == "true");
        } else if (typeStr == "int") {
            bool ok;
            int v = text.toInt(&ok);
            return ok ? FlexData(v) : FlexData(text);
        } else if (typeStr == "double") {
            bool ok;
            double v = text.toDouble(&ok);
            return ok ? FlexData(v) : FlexData(text);
        } else if (typeStr == "bytearray") {
            return FlexData(QByteArray::fromBase64(text.toUtf8()));
        } else if (typeStr == "datetime") {
            return FlexData(QDateTime::fromString(text, Qt::ISODate));
        } else if (typeStr == "uuid") {
            return FlexData(QUuid(text));
        } else {
            return FlexData(text); // 默认字符串兼容旧格式
        }
    }

    QHash<QString, FlexData> map;
    QList<FlexData> list;
    bool isArrayCandidate = (typeStr == "array"); // 优先以 type 属性为准
    while (!(reader.tokenType() == QXmlStreamReader::EndElement && reader.name() == name)) {
        if (reader.tokenType() == QXmlStreamReader::StartElement) {
            FlexData child = readXmlValue(reader);
            QString childName = reader.name().toString();
            if (childName == "item") {
                list.append(child);
                isArrayCandidate = true;
            } else {
                map[childName] = child;
            }
        }
        reader.readNext();
    }
    if (isArrayCandidate && map.isEmpty())
        return FlexData(list);
    else
        return FlexData(map);
}

bool FlexData::fromXml(const QByteArray& xml) {
    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.isStartElement())
        reader.readNext();
    if (reader.isStartElement()) {
        *this = readXmlValue(reader);
        return true;
    }
    return false;
}

// ============================= INI 序列化 =============================
void FlexData::flattenToIni(QHash<QString, QHash<QString, QString> > &out, const FlexData &data, const QString &prefix)
{
    if (data.isMap()) {
        for (const QString& key : data.keys()) {
            QString newPrefix = prefix.isEmpty() ? key : prefix + "/" + key;
            flattenToIni(out, data[key], newPrefix);
        }
    } else if (data.isArray()) {
        qWarning() << "FlexData::toIni: array type is not supported in INI format, will be ignored";
    } else {
        // 叶子节点：确定 section 和 iniKey
        QString section = prefix.section('/', 0, 0);
        QString iniKey = prefix.section('/', 1);
        if (iniKey.isEmpty()) iniKey = "value";   // 默认键名
        // 将 FlexData 值转为字符串
        QString valStr;
        if (data.isByteArray()) {
            valStr = data.toByteArray().toBase64();
        } else if (data.isDateTime()) {
            valStr = data.toDateTime().toString(Qt::ISODate);
        } else if (data.isUuid()) {
            valStr = data.toUuid().toString();
        } else {
            valStr = data.toString();
        }
        out[section][iniKey] = valStr;
    }
}

QString FlexData::toIni() const {
    if (!isMap()) return QString();
    QHash<QString, QHash<QString, QString>> flat;
    flattenToIni(flat, *this, "");
    QString result;
    for (auto it = flat.begin(); it != flat.end(); ++it) {
        result += "[" + it.key() + "]\n";
        for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
            result += it2.key() + "=" + it2.value() + "\n";
        }
        result += "\n";
    }
    return result;
}

bool FlexData::fromIni(const QString& iniData) {
    QTemporaryFile tmpFile;
    if (!tmpFile.open()) return false;
    tmpFile.write(iniData.toUtf8());
    tmpFile.flush();
    tmpFile.close(); // 关闭文件句柄，解除 Windows 下的独占锁定
    QSettings iniSettings(tmpFile.fileName(), QSettings::IniFormat);
    iniSettings.setIniCodec("UTF-8"); // 统一 UTF-8 编码，解决中文乱码与跨平台一致性

    QHash<QString, FlexData> resultMap;
    for (const QString& group : iniSettings.childGroups()) {
        iniSettings.beginGroup(group);
        QHash<QString, FlexData> groupMap;
        for (const QString& key : iniSettings.childKeys()) {
            QString val = iniSettings.value(key).toString();
            if (val == "true") groupMap[key] = FlexData(true);
            else if (val == "false") groupMap[key] = FlexData(false);
            else {
                bool ok;
                int intVal = val.toInt(&ok);
                if (ok) groupMap[key] = FlexData(intVal);
                else {
                    double dblVal = val.toDouble(&ok);
                    if (ok) groupMap[key] = FlexData(dblVal);
                    else groupMap[key] = FlexData(val);
                }
            }
        }
        resultMap[group] = FlexData(groupMap);
        iniSettings.endGroup();
    }
    *this = FlexData(resultMap);
    return true;
}

// ============================= 二进制序列化 =============================
QByteArray FlexData::serialize() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_12); // 固定序列化版本，确保跨版本跨平台二进制兼容
    stream << static_cast<quint32>(d->type);
    switch (d->type) {
    case FlexDataPrivate::TypeBool: stream << d->b; break;
    case FlexDataPrivate::TypeInt: stream << d->i; break;
    case FlexDataPrivate::TypeDouble: stream << d->d; break;
    case FlexDataPrivate::TypeString: stream << *d->s; break;
    case FlexDataPrivate::TypeByteArray: stream << *d->ba; break;
    case FlexDataPrivate::TypeDateTime: stream << *d->dt; break;
    case FlexDataPrivate::TypeUuid: stream << *d->uuid; break;
    case FlexDataPrivate::TypeMap: {
        stream << d->map->size();
        for (auto it = d->map->begin(); it != d->map->end(); ++it) {
            stream << it.key() << it.value().serialize();
        }
        break;
    }
    case FlexDataPrivate::TypeArray: {
        stream << d->list->size();
        for (const auto& item : *d->list)
            stream << item.serialize();
        break;
    }
    default: break;
    }
    return data;
}

bool FlexData::deserialize(const QByteArray& data) {
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_12); // 与序列化版本严格一致
    quint32 type;
    stream >> type;
    switch (type) {
    case FlexDataPrivate::TypeNull: *this = FlexData(); break;
    case FlexDataPrivate::TypeBool: { bool v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeInt: { int v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeDouble: { double v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeString: { QString v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeByteArray: { QByteArray v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeDateTime: { QDateTime v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeUuid: { QUuid v; stream >> v; *this = FlexData(v); break; }
    case FlexDataPrivate::TypeMap: {
        int sz; stream >> sz;
        QHash<QString, FlexData> map;
        for (int i=0; i<sz; ++i) {
            QString key; QByteArray valData;
            stream >> key >> valData;
            FlexData val; val.deserialize(valData);
            map[key] = val;
        }
        *this = FlexData(map);
        break;
    }
    case FlexDataPrivate::TypeArray: {
        int sz; stream >> sz;
        QList<FlexData> list;
        for (int i=0; i<sz; ++i) {
            QByteArray valData; stream >> valData;
            FlexData val; val.deserialize(valData);
            list.append(val);
        }
        *this = FlexData(list);
        break;
    }
    default: return false;
    }
    return stream.status() == QDataStream::Ok;
}

// ============================= QVariant 互转 =============================
FlexData::operator QVariant() const {
    return QVariant::fromValue(*this);
}

FlexData FlexData::fromVariant(const QVariant& variant) {
    if (variant.canConvert<FlexData>())
        return variant.value<FlexData>();
    if (variant.type() == QVariant::Map) {
        QHash<QString, FlexData> map;
        QVariantMap vmap = variant.toMap();
        for (auto it = vmap.begin(); it != vmap.end(); ++it)
            map[it.key()] = fromVariant(it.value());
        return FlexData(map);
    }
    if (variant.type() == QVariant::List) {
        QList<FlexData> list;
        QVariantList vlist = variant.toList();
        for (const auto& item : vlist)
            list.append(fromVariant(item));
        return FlexData(list);
    }
    if (variant.type() == QVariant::Bool) return FlexData(variant.toBool());
    if (variant.type() == QVariant::Int) return FlexData(variant.toInt());
    if (variant.type() == QVariant::Double) return FlexData(variant.toDouble());
    if (variant.type() == QVariant::String) return FlexData(variant.toString());
    return FlexData();
}

// ============================= 调试输出 =============================
QString FlexData::dump(int indent) const {
    QString spaces(indent * 2, ' ');
    switch (d->type) {
    case FlexDataPrivate::TypeNull: return "null";
    case FlexDataPrivate::TypeBool: return d->b ? "true" : "false";
    case FlexDataPrivate::TypeInt: return QString::number(d->i);
    case FlexDataPrivate::TypeDouble: return QString::number(d->d);
    case FlexDataPrivate::TypeString: return "\"" + *d->s + "\"";
    case FlexDataPrivate::TypeByteArray: return "b64(\"" + d->ba->toBase64() + "\")";
    case FlexDataPrivate::TypeDateTime: return "dt(\"" + d->dt->toString(Qt::ISODate) + "\")";
    case FlexDataPrivate::TypeUuid: return "uuid(\"" + d->uuid->toString() + "\")";
    case FlexDataPrivate::TypeMap: {
        QString out = "{\n";
        for (auto it = d->map->begin(); it != d->map->end(); ++it) {
            out += spaces + "  " + it.key() + ": " + it.value().dump(indent + 1) + ",\n";
        }
        out += spaces + "}";
        return out;
    }
    case FlexDataPrivate::TypeArray: {
        QString out = "[\n";
        for (const auto& item : *d->list) {
            out += spaces + "  " + item.dump(indent + 1) + ",\n";
        }
        out += spaces + "]";
        return out;
    }
    }
    return QString();
}
