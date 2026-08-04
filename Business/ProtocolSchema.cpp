#include "ProtocolSchema.h"
#include <QtEndian>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <limits>
#include <cmath>
#include <algorithm>

namespace Sqz {

// ==================== 辅助静态函数 ====================

// 生成指定位长的掩码
static inline quint64 maskBits(int bitLength) {
    if (bitLength <= 0 || bitLength > ProtocolSchema::MAX_BIT_WIDTH)
        return 0;
    return (bitLength == 64) ? ~0ULL : (1ULL << bitLength) - 1;
}

// 将整数分解为大端比特排列的字节数组
static QByteArray decomposeToBigEndianBits(quint64 value, int bitLength) {
    int byteLen = (bitLength + 7) / 8;
    QByteArray bytes(byteLen, 0);
    value &= maskBits(bitLength);
    value <<= (byteLen * 8 - bitLength);
    for (int i = 0; i < byteLen; ++i) {
        bytes[i] = static_cast<char>((value >> ((byteLen - 1 - i) * 8)) & 0xFF);
    }
    return bytes;
}

// ==================== JSON 加载辅助函数 ====================

// 将字符串解析为 ValueType 枚举（大小写不敏感），失败返回 false
static bool parseValueType(const QString& s, ValueType& out) {
    const QString v = s.trimmed();
    if (v.compare("Int", Qt::CaseInsensitive) == 0)        { out = Int;       return true; }
    if (v.compare("UInt", Qt::CaseInsensitive) == 0)       { out = UInt;      return true; }
    if (v.compare("HexString", Qt::CaseInsensitive) == 0)  { out = HexString; return true; }
    if (v.compare("Base64", Qt::CaseInsensitive) == 0)     { out = Base64;    return true; }
    if (v.compare("RawBytes", Qt::CaseInsensitive) == 0)   { out = RawBytes;  return true; }
    if (v.compare("String", Qt::CaseInsensitive) == 0)     { out = String;    return true; }
    return false;
}

// 将字符串解析为 Endian 枚举（大小写不敏感）
static bool parseEndianStr(const QString& s, Endian& out) {
    const QString v = s.trimmed();
    if (v.compare("LittleEndian", Qt::CaseInsensitive) == 0) { out = LittleEndian; return true; }
    if (v.compare("BigEndian", Qt::CaseInsensitive) == 0)    { out = BigEndian;    return true; }
    return false;
}

// 将字符串解析为 BitOrder 枚举（大小写不敏感）
static bool parseBitOrderStr(const QString& s, BitOrder& out) {
    const QString v = s.trimmed();
    if (v.compare("MsbFirst", Qt::CaseInsensitive) == 0) { out = MsbFirst; return true; }
    if (v.compare("Physical", Qt::CaseInsensitive) == 0) { out = Physical; return true; }
    return false;
}

// 将 enumMap 对象（{"0":"X","1":"Y"}）解析为 QHash<quint64,QString>
// key 必须是可转 quint64 的数字字符串，value 必须为字符串
static bool parseEnumMapObj(const QJsonObject& obj, QHash<quint64, QString>& out, QString* err) {
    out.clear();
    for (const QString& key : obj.keys()) {
        bool ok = false;
        qulonglong raw = key.toULongLong(&ok);
        if (!ok) {
            if (err) *err = QString("enumMap key '%1' not numeric").arg(key);
            return false;
        }
        const QJsonValue& v = obj.value(key);
        if (!v.isString()) {
            if (err) *err = QString("enumMap key '%1' value must be string").arg(key);
            return false;
        }
        out.insert(static_cast<quint64>(raw), v.toString());
    }
    return true;
}

// ==================== ProtocolSchema 类实现 ====================

ProtocolSchema::ProtocolSchema() { }

ProtocolSchema::~ProtocolSchema() { }

// ---------- 添加固定长度字段 ----------
ProtocolSchema& ProtocolSchema::addField(const QString& name, int startByte, int startBit,
                                         int bitLength, ValueType type, Endian endian,
                                         BitOrder bitOrder, bool isSigned,
                                         double factor, double offset, QString* err) {
    QMutexLocker lock(&m_mutex);

    // 参数合法性校验
    if (name.trimmed().isEmpty()) {
        if (err) *err = "Field name cannot be empty";
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Field %1 startBit must 0~7").arg(name);
        return *this;
    }
    if (bitLength < 1 || bitLength > MAX_BIT_WIDTH) {
        if (err) *err = QString("Field %1 bitLength must 1~64").arg(name);
        return *this;
    }

    // 同名字段重复检测
    for (const auto& f : m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate field name: %1").arg(name);
            return *this;
        }
    }

    Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = bitLength;
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = isSigned;
    f.factor = factor;
    f.offset = offset;
    f.lenField = "";
    f.valueMap.clear();         // 默认无枚举映射
    f.hasMapping = false;
    f.conditions.clear();       // 空列表=无条件
    f.isDefaultBranch = false;
    f.defaultBranchField = "";
    m_fields.append(f);
    invalidateSortCache();  // 字段变化，使排序缓存失效
    return *this;
}

// ---------- 添加变长字段 ----------
ProtocolSchema& ProtocolSchema::addVariableField(const QString& name, int startByte, int startBit,
                                                 const QString& lenField, ValueType type,
                                                 Endian endian, BitOrder bitOrder,
                                                 double factor, double offset, QString* err) {
    QMutexLocker lock(&m_mutex);

    if (name.trimmed().isEmpty()) {
        if (err) *err = "Variable field name cannot be empty";
        return *this;
    }
    if (lenField.trimmed().isEmpty()) {
        if (err) *err = QString("Var field %1 lenField empty").arg(name);
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Var field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Var field %1 startBit must 0~7").arg(name);
        return *this;
    }

    // 同名字段重复检测
    for (const auto& f : m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate var field name: %1").arg(name);
            return *this;
        }
    }

    Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = 0;            // 0 表示变长
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = false;
    f.lenField = lenField;
    f.factor = factor;
    f.offset = offset;
    f.valueMap.clear();         // 默认无枚举映射
    f.hasMapping = false;
    f.conditions.clear();
    f.isDefaultBranch = false;
    f.defaultBranchField = "";
    m_fields.append(f);
    invalidateSortCache();  // 字段变化，使排序缓存失效
    return *this;
}

// ---------- 枚举映射 map ----------
// 为最近添加的字段追加一条 原始字节值 -> 字符串 的映射
// 互斥机制: 首次调用即标记 hasMapping=true 并将 factor/offset 重置为 1.0/0.0
ProtocolSchema& ProtocolSchema::map(quint64 rawValue, const QString& str, QString* err) {
    QMutexLocker lock(&m_mutex);

    // 必须先添加字段
    if (m_fields.isEmpty()) {
        if (err) *err = "map() called before any addField/addVariableField";
        return *this;
    }
    Field& f = m_fields.last();   // 作用于最近添加的字段

    // 仅支持整数类型字段（字节值映射语义）
    if (f.type != Int && f.type != UInt) {
        if (err) *err = QString("map() only valid for Int/UInt field, field '%1' type mismatch").arg(f.name);
        return *this;
    }

    // 追加（或覆盖）映射条目
    f.valueMap.insert(rawValue, str);
    // 启用映射：互斥地禁用线性变换
    if (!f.hasMapping) {
        f.hasMapping = true;
        f.factor = 1.0;   // 重置系数，线性变换不再生效
        f.offset = 0.0;   // 重置偏移
    }
    invalidateSortCache();  // 字段配置变化，使排序缓存失效（保持一致性）
    return *this;
}

// ---------- 条件分支入口 ----------
// 记录条件字段名，供 otherwise() 直接在 ProtocolSchema 上调用时使用
ConditionalBuilder ProtocolSchema::when(const QString& fieldName, const QVariant& value) {
    m_lastConditionField = fieldName;
    return ConditionalBuilder(this, fieldName, value, false);
}

// 使用最近一次 when() 的条件字段名作为默认分支的条件字段
ConditionalBuilder ProtocolSchema::otherwise() {
    return ConditionalBuilder(this, m_lastConditionField, QVariant(), true);
}

// ---------- 清空 ----------
void ProtocolSchema::clear() {
    QMutexLocker lock(&m_mutex);
    m_fields.clear();
    m_lastConditionField.clear();  // 重置条件字段记录
    m_protocolName.clear();        // 重置协议名
    invalidateSortCache();  // 清空字段，使排序缓存失效
}

// ---------- 验证 ----------
bool ProtocolSchema::validateSchema(QString* errMsg) const {
    QMutexLocker lock(&m_mutex);
    QStringList cycle;
    if (hasVarCycleDependency(cycle, errMsg))
        return false;
    return true;
}

// 检测变长字段循环依赖
bool ProtocolSchema::hasVarCycleDependency(QStringList& cycleList, QString* err) const {
    QHash<QString, QString> varMap;
    for (const auto& f : m_fields) {
        if (f.bitLength == 0)
            varMap[f.name] = f.lenField;
    }

    QSet<QString> visited;
    for (const auto& var : varMap.keys()) {
        QString cur = var;
        QStringList path;
        while (varMap.contains(cur)) {
            if (visited.contains(cur)) {
                if (path.contains(cur)) {
                    auto idx = path.indexOf(cur);
                    cycleList = path.mid(idx);
                    if (err) *err = QString("Var field cycle dependency: %1").arg(cycleList.join("->"));
                    return true;
                }
                break;
            }
            visited.insert(cur);
            path << cur;
            cur = varMap[cur];
        }
    }
    return false;
}

// ---------- JSON 配置加载 ----------
// 获取协议名（线程安全读）
QString ProtocolSchema::protocolName() const {
    QMutexLocker lock(&m_mutex);
    return m_protocolName;
}

// 从 JSON 对象加载完整协议：clear -> 解析顶层 -> 逐字段 loadOneField -> validateSchema
// 任意步骤失败均回滚到空 schema 并返回 false
bool ProtocolSchema::loadJson(const QJsonObject& root, QString* err) {
    QMutexLocker lock(&m_mutex);

    // 失败时统一回滚到空 schema
    auto fail = [this, err](const QString& msg) -> bool {
        m_fields.clear();
        m_protocolName.clear();
        m_lastConditionField.clear();
        invalidateSortCache();
        if (err) *err = msg;
        return false;
    };

    // 1) 先清空（clear() 会再锁一次，recursive mutex 安全；含 m_protocolName）
    clear();

    // 2) protocolName（可选，必须为字符串）
    if (root.contains("protocolName")) {
        const QJsonValue& v = root.value("protocolName");
        if (!v.isString()) return fail("protocolName must be a string");
        m_protocolName = v.toString();
    }

    // 3) defaults（可选对象，提供标量属性缺省值；enumMap 不参与继承）
    QJsonObject defaults;
    if (root.contains("defaults")) {
        if (!root.value("defaults").isObject())
            return fail("defaults must be an object");
        defaults = root.value("defaults").toObject();
    }

    // 4) enumMaps（可选对象：命名枚举库，供字段 enumMap 字符串引用）
    QJsonObject enumMaps;
    if (root.contains("enumMaps")) {
        if (!root.value("enumMaps").isObject())
            return fail("enumMaps must be an object");
        enumMaps = root.value("enumMaps").toObject();
    }

    // 5) fields（必填数组）
    if (!root.contains("fields") || !root.value("fields").isArray())
        return fail("fields must be an array");
    const QJsonArray fields = root.value("fields").toArray();

    // 6) 逐字段加载
    for (int i = 0; i < fields.size(); ++i) {
        if (!fields.at(i).isObject())
            return fail(QString("fields[%1] must be an object").arg(i));
        QString fieldErr;
        if (!loadOneField(fields.at(i).toObject(), defaults, enumMaps, &fieldErr))
            return fail(QString("fields[%1]: %2").arg(i).arg(fieldErr));
    }

    // 7) 校验变长字段循环依赖
    QString cycleErr;
    if (!validateSchema(&cycleErr))
        return fail(QString("validateSchema failed: %1").arg(cycleErr));

    if (err) err->clear();
    return true;
}

// 从 JSON 文件加载协议定义（读文件 -> 解析 -> loadJson）
bool ProtocolSchema::loadFile(const QString& path, QString* err) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QString("Cannot open file: %1").arg(path);
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        if (err) *err = QString("JSON parse error at offset %1: %2")
                .arg(parseErr.offset).arg(parseErr.errorString());
        return false;
    }
    if (!doc.isObject()) {
        if (err) *err = "JSON root must be an object";
        return false;
    }
    return loadJson(doc.object(), err);
}

// 加载单个字段：解析标量属性（带 defaults 继承）-> addField/addVariableField
// -> 解析 enumMap（内联对象或命名引用）-> 调 map 写入
// -> 在 m_fields.last() 上设置 conditions/isDefaultBranch/defaultBranchField
// 不加锁（由 loadJson 持锁保证线程安全）
bool ProtocolSchema::loadOneField(const QJsonObject& fo, const QJsonObject& defaults,
                                  const QJsonObject& enumMaps, QString* err) {
    // ---- 必填：name ----
    if (!fo.contains("name") || !fo.value("name").isString()) {
        if (err) *err = "field missing 'name'";
        return false;
    }
    const QString name = fo.value("name").toString().trimmed();
    if (name.isEmpty()) {
        if (err) *err = "field name empty";
        return false;
    }

    // ---- 必填：startByte（用 qint64 接收防溢出，再范围检查）----
    if (!fo.contains("startByte")) {
        if (err) *err = QString("field '%1' missing 'startByte'").arg(name);
        return false;
    }
    qint64 sb64 = fo.value("startByte").toVariant().toLongLong();
    if (sb64 < 0 || sb64 > std::numeric_limits<int>::max()) {
        if (err) *err = QString("field '%1' startByte out of int range").arg(name);
        return false;
    }
    const int startByte = static_cast<int>(sb64);

    // ---- 必填：startBit（0~7）----
    if (!fo.contains("startBit")) {
        if (err) *err = QString("field '%1' missing 'startBit'").arg(name);
        return false;
    }
    const int startBit = fo.value("startBit").toInt(-1);
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("field '%1' startBit must be 0~7").arg(name);
        return false;
    }

    // ---- lenField（可选；存在则走变长字段路径）----
    QString lenField;
    if (fo.contains("lenField") && fo.value("lenField").isString())
        lenField = fo.value("lenField").toString().trimmed();

    // ---- defaults 继承辅助：字段自身 > defaults > fallback ----
    auto inheritStr = [&](const char* key, const QString& fallback) -> QString {
        if (fo.contains(key)) return fo.value(key).toString();
        if (defaults.contains(key)) return defaults.value(key).toString();
        return fallback;
    };
    auto inheritDouble = [&](const char* key, double fallback) -> double {
        if (fo.contains(key)) return fo.value(key).toDouble(fallback);
        if (defaults.contains(key)) return defaults.value(key).toDouble(fallback);
        return fallback;
    };
    auto inheritBool = [&](const char* key, bool fallback) -> bool {
        if (fo.contains(key)) return fo.value(key).toBool(fallback);
        if (defaults.contains(key)) return defaults.value(key).toBool(fallback);
        return fallback;
    };

    // ---- type / endian / bitOrder / isSigned / factor / offset ----
    ValueType type;
    if (!parseValueType(inheritStr("type", "UInt"), type)) {
        if (err) *err = QString("field '%1' unknown type").arg(name);
        return false;
    }
    Endian endian;
    if (!parseEndianStr(inheritStr("endian", "LittleEndian"), endian)) {
        if (err) *err = QString("field '%1' invalid endian").arg(name);
        return false;
    }
    BitOrder bitOrder;
    if (!parseBitOrderStr(inheritStr("bitOrder", "Physical"), bitOrder)) {
        if (err) *err = QString("field '%1' invalid bitOrder").arg(name);
        return false;
    }
    const bool isSigned = inheritBool("isSigned", false);
    const double factor = inheritDouble("factor", 1.0);
    const double offset = inheritDouble("offset", 0.0);

    // ---- 调用 addField 或 addVariableField（复用其校验与重名检测）----
    QString addErr;
    if (!lenField.isEmpty()) {
        // 变长字段：bitLength/isSigned 由 addVariableField 内部固定，此处忽略
        addVariableField(name, startByte, startBit, lenField, type, endian, bitOrder,
                         factor, offset, &addErr);
    } else {
        // 固定字段：bitLength 必填且 1~64
        if (!fo.contains("bitLength")) {
            if (err) *err = QString("field '%1' missing 'bitLength'").arg(name);
            return false;
        }
        const int bitLength = fo.value("bitLength").toInt(0);
        addField(name, startByte, startBit, bitLength, type, endian, bitOrder,
                 isSigned, factor, offset, &addErr);
    }
    if (!addErr.isEmpty()) {
        if (err) *err = addErr;
        return false;
    }

    // ---- 取新字段引用继续配置 ----
    Field& f = m_fields.last();

    // ---- 变长字段 lenField 存在性补检（validateSchema 仅查环，不查存在性）----
    if (!lenField.isEmpty()) {
        bool lenExists = false;
        for (const auto& other : m_fields) {
            if (other.name == lenField) { lenExists = true; break; }
        }
        if (!lenExists) {
            if (err) *err = QString("field '%1' lenField '%2' not defined").arg(name, lenField);
            return false;
        }
    }

    // ---- enumMap（可选；内联对象 或 命名引用字符串）----
    if (fo.contains("enumMap")) {
        const QJsonValue& em = fo.value("enumMap");
        QJsonObject mapObj;
        if (em.isObject()) {
            mapObj = em.toObject();                       // 内联定义
        } else if (em.isString()) {
            const QString ref = em.toString();            // 命名引用
            if (!enumMaps.contains(ref)) {
                if (err) *err = QString("field '%1' enumMap '%2' not found in enumMaps").arg(name, ref);
                return false;
            }
            if (!enumMaps.value(ref).isObject()) {
                if (err) *err = QString("enumMaps.%1 must be an object").arg(ref);
                return false;
            }
            mapObj = enumMaps.value(ref).toObject();
        } else {
            if (err) *err = QString("field '%1' enumMap must be object or string").arg(name);
            return false;
        }

        // 解析为 QHash<quint64,QString>
        QHash<quint64, QString> parsed;
        QString mapErr;
        if (!parseEnumMapObj(mapObj, parsed, &mapErr)) {
            if (err) *err = QString("field '%1' %2").arg(name, mapErr);
            return false;
        }

        // 通过 map() 写入（复用其 Int/UInt 类型校验与互斥副作用；非整数字段会失败）
        for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it) {
            QString mErr;
            map(it.key(), it.value(), &mErr);
            if (!mErr.isEmpty()) {
                if (err) *err = QString("field '%1' enumMap: %2").arg(name, mErr);
                return false;
            }
        }
    }

    // ---- comment 字段：显式忽略 ----

    // ---- conditions（可选数组，AND 逻辑）----
    if (fo.contains("conditions")) {
        if (!fo.value("conditions").isArray()) {
            if (err) *err = QString("field '%1' conditions must be an array").arg(name);
            return false;
        }
        const QJsonArray conds = fo.value("conditions").toArray();
        for (const QJsonValue& cv : conds) {
            if (!cv.isObject()) {
                if (err) *err = QString("field '%1' condition must be an object").arg(name);
                return false;
            }
            const QJsonObject co = cv.toObject();
            if (!co.contains("field") || !co.value("field").isString()) {
                if (err) *err = QString("field '%1' condition missing 'field'").arg(name);
                return false;
            }
            if (!co.contains("value")) {
                if (err) *err = QString("field '%1' condition missing 'value'").arg(name);
                return false;
            }
            // value 用 toVariant() 保留数字/字符串原样，matchCondition 会规范化比较
            f.conditions.append({ co.value("field").toString(), co.value("value").toVariant() });
        }
    }

    // ---- isDefaultBranch / defaultBranchField ----
    if (fo.contains("isDefaultBranch"))
        f.isDefaultBranch = fo.value("isDefaultBranch").toBool(false);
    if (fo.contains("defaultBranchField") && fo.value("defaultBranchField").isString())
        f.defaultBranchField = fo.value("defaultBranchField").toString();

    // ---- 默认分支一致性校验：isDefaultBranch=true 时 defaultBranchField 必须非空 ----
    // 防止 evaluateCondition 默认分支因 defaultBranchField 空而误判"总是生效"
    if (f.isDefaultBranch && f.defaultBranchField.isEmpty()) {
        if (err) *err = QString("field '%1' isDefaultBranch=true requires non-empty defaultBranchField").arg(name);
        return false;
    }

    return true;
}

// 获取按绝对比特偏移排序的字段列表
// 使用缓存机制：若字段定义未变化则直接返回缓存结果，避免重复排序
// 返回: 排序后的字段副本
QVector<ProtocolSchema::Field> ProtocolSchema::getSortedFields() const {
    QMutexLocker lock(&m_mutex);
    // 缓存命中：直接返回缓存副本
    if (m_sortCacheValid && !m_sortedFieldsCache.isEmpty()) {
        return m_sortedFieldsCache;
    }
    // 缓存未命中：复制并排序
    QVector<Field> copy = m_fields;
    std::sort(copy.begin(), copy.end(), [](const Field& a, const Field& b) {
        return absoluteBitOffset(a) < absoluteBitOffset(b);
    });
    // 更新缓存
    m_sortedFieldsCache = copy;
    m_sortCacheValid = true;
    return copy;
}

// ---------- 字段重叠检测 ----------
QStringList ProtocolSchema::checkOverlap(const QJsonObject& runtimeVarData, QString* err) const {
    QMutexLocker lock(&m_mutex);
    QStringList overlaps;
    QVector<QPair<qint64, qint64>> fieldRanges;

    for (const auto& f : m_fields) {
        qint64 start = absoluteBitOffset(f);
        qint64 end;
        if (f.bitLength > 0) {
            end = start + f.bitLength;
        } else {
            // 动态变长字段计算运行时bit范围
            if (!runtimeVarData.contains(f.lenField)) {
                if (err) *err = QString("Overlap check missing len field %1 for %2").arg(f.lenField, f.name);
                continue;
            }
            bool ok;
            qint64 lenByte = runtimeVarData[f.lenField].toVariant().toLongLong(&ok);
            if (!ok || lenByte < 0 || lenByte > MAX_VAR_BYTE_SIZE)
                continue;
            end = start + lenByte * 8;
        }
        fieldRanges.append({start, end});
    }

    for (int i = 0; i < fieldRanges.size(); ++i) {
        qint64 aS = fieldRanges[i].first;
        qint64 aE = fieldRanges[i].second;
        QString aName = m_fields[i].name;
        for (int j = i + 1; j < fieldRanges.size(); ++j) {
            qint64 bS = fieldRanges[j].first;
            qint64 bE = fieldRanges[j].second;
            QString bName = m_fields[j].name;
            if (aE > bS && bE > aS) {
                overlaps << aName + " and " + bName;
            }
        }
    }
    return overlaps;
}

// ---------- 评估条件 ----------
// 判断给定字段的条件分支是否在当前上下文下生效
// field:   待评估的字段（含 conditions/defaultBranchField/isDefaultBranch）
// context: 已解析字段的 JSON 对象，用于查询条件字段值
// 返回:    条件满足返回 true，否则 false
// 注意：比较时对数值类型做宽容处理（int vs double 视为相等）；
//       对含枚举映射的条件字段，统一按原始数值比较，保证可靠性
static bool variantMatch(const QVariant& a, const QVariant& b) {
    if (a == b) return true;
    // 数值类型宽容比较：int/uint/longlong 等与 double 之间的比较
    bool okA = false, okB = false;
    double dA = a.toDouble(&okA);
    double dB = b.toDouble(&okB);
    if (okA && okB) {
        return qAbs(dA - dB) < 1e-9;
    }
    return false;
}

// 按字段名查找字段定义（const 版本）
const ProtocolSchema::Field* ProtocolSchema::findField(const QString& name) const {
    for (const auto& f : m_fields) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

// 将值规范化为原始数值（用于条件比较）
// 含映射字段: 字符串反查为原始数值；数字原样返回
// 无映射字段: 原样返回
bool ProtocolSchema::normalizeToRawValue(const Field& field, const QVariant& val, QVariant& out) const {
    if (!field.hasMapping) {
        out = val;
        return true;
    }
    // 含枚举映射：若为字符串则反向查找原始字节值
    if (val.type() == QVariant::String) {
        const QString str = val.toString();
        for (auto it = field.valueMap.constBegin(); it != field.valueMap.constEnd(); ++it) {
            if (it.value() == str) {
                out = QVariant(static_cast<qulonglong>(it.key()));
                return true;
            }
        }
        // 未命中映射表：尝试将字符串解析为数值（兼容数值字符串如 "1"）
        bool ok = false;
        qulonglong parsed = str.toULongLong(&ok);
        if (ok) {
            out = QVariant(parsed);
            return true;
        }
        return false;   // 无法转换为原始数值
    }
    // 数字类型直接使用
    out = val;
    return true;
}

// 比较单个条件是否满足（基于原始数值比较）
// 对含枚举映射的条件字段，将上下文值与条件值都规范化为原始数值后再比较
// 保证 when("status", 1) 这类条件即使 status 解析后为字符串 "开机" 也能正确匹配
bool ProtocolSchema::matchCondition(const QString& fieldName, const QVariant& condValue,
                                    const QJsonObject& context) const {
    if (!context.contains(fieldName)) return false;
    const Field* condField = findField(fieldName);
    const QVariant ctxVar = context[fieldName].toVariant();
    if (condField) {
        QVariant ctxRaw, condRaw;
        if (normalizeToRawValue(*condField, ctxVar, ctxRaw) &&
                normalizeToRawValue(*condField, condValue, condRaw)) {
            return variantMatch(ctxRaw, condRaw);
        }
    }
    // 退化为直接比较
    return variantMatch(ctxVar, condValue);
}

bool ProtocolSchema::evaluateCondition(const Field& field, const QJsonObject& context) const {
    // 无条件字段（条件列表为空且非默认分支）始终生效
    if (field.conditions.isEmpty() && !field.isDefaultBranch) {
        return true;
    }

    // 检查所有条件 (AND 逻辑)，使用 matchCondition 保证枚举映射字段按原始数值比较
    for (const auto& cond : field.conditions) {
        if (!matchCondition(cond.first, cond.second, context)) {
            return false;
        }
    }

    // 默认分支：仅当所有非默认分支均不匹配时才生效
    if (field.isDefaultBranch) {
        for (const auto& other : m_fields) {
            if (other.isDefaultBranch) continue;
            if (other.conditions.isEmpty()) continue;
            // 检查其他分支是否匹配当前默认分支对应的条件字段
            if (other.conditions.first().first == field.defaultBranchField) {
                bool allMatch = true;
                for (const auto& cond : other.conditions) {
                    if (!matchCondition(cond.first, cond.second, context)) {
                        allMatch = false;
                        break;
                    }
                }
                if (allMatch) return false; // 有匹配的非默认分支，默认分支不生效
            }
        }
        return true;
    }

    return true;
}

// ---------- 解析单个字段 ----------
bool ProtocolSchema::parseField(const Field& f, const QByteArray& data,
                                QJsonObject& result, QString* errorMsg) const {
    qint64 bitOffset64 = absoluteBitOffset(f);
    if (bitOffset64 > std::numeric_limits<int>::max()) {
        if (errorMsg) *errorMsg = QString("Field %1 bit offset overflow").arg(f.name);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }
    int bitOffset = static_cast<int>(bitOffset64);

    // 解析整数字段
    if (f.type == Int || f.type == UInt) {
        quint64 raw;
        QString subErr;
        if (!readBits(data, bitOffset, f.bitLength, raw, f.endian, f.bitOrder, &subErr)) {
            if (errorMsg) *errorMsg = QString("Failed to read field '%1': %2").arg(f.name, subErr);
            result.insert(f.name, QJsonValue::Null);
            return false;
        }

        // 枚举映射模式：输出字符串，跳过 factor/offset 线性变换（互斥）
        // 保证 JSON value 为字符串类型
        if (f.hasMapping) {
            auto it = f.valueMap.constFind(raw);
            if (it != f.valueMap.constEnd()) {
                result.insert(f.name, it.value());          // 命中映射：输出对应字符串
            } else {
                // 未命中映射：输出原始数值的字符串形式，保持 value 为字符串类型
                // 这样反向打包时也能通过数值字符串正确还原
                result.insert(f.name, QString::number(raw));
            }
            return true;
        }

        // 普通模式：应用线性变换得到物理值
        double finalVal = applyLinearTransform(raw, f.factor, f.offset, f.isSigned, f.bitLength);
        result.insert(f.name, finalVal);
        return true;
    }

    // 解析字节类型字段
    QByteArray bytes;
    QString subErr;
    if (!readBitsToBytes(data, bitOffset, f.bitLength, bytes, f.endian, f.bitOrder, &subErr)) {
        if (errorMsg) *errorMsg = QString("Failed to read field '%1': %2").arg(f.name, subErr);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }

    switch (f.type) {
    case HexString: result.insert(f.name, QString::fromLatin1(bytes.toHex())); break;
    case Base64:    result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
    case RawBytes:  result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
    case String:    result.insert(f.name, QString::fromUtf8(bytes)); break;
    default:        result.insert(f.name, QJsonValue::Null);
    }
    return true;
}

// ---------- 解析 ----------
// 将二进制数据按字段定义解析为 JSON 对象
// 解析顺序：1) 无条件固定字段 2) 无条件变长字段 3) 条件字段（依赖前两步结果）
// data:     原始二进制数据
// errorMsg: 错误信息输出（可选）
// 返回:     包含所有已解析字段的 JSON 对象
QJsonObject ProtocolSchema::parse(const QByteArray& data, QString* errorMsg) const {
    QMutexLocker lock(&m_mutex);
    QJsonObject result;  // 直接用 result 做条件判断，支持嵌套条件实时求值

    // 第一遍：解析所有无条件固定长度字段（conditions为空、非默认分支、且bitLength>0）
    // 这些字段是协议的基础，必须先完成解析以供后续变长/条件字段依赖
    for (const Field& f : m_fields) {
        if (f.conditions.isEmpty() && !f.isDefaultBranch && f.bitLength > 0) {
            parseField(f, data, result, errorMsg);
        }
    }

    // 第二遍：解析所有无条件变长字段（conditions为空、非默认分支、且bitLength==0）
    // 变长字段依赖长度字段的值，因此必须在固定字段解析完成后进行
    for (const Field& f : m_fields) {
        if (f.conditions.isEmpty() && !f.isDefaultBranch && f.bitLength == 0) {
            parseVariableField(f, data, result, errorMsg);
        }
    }

    // 第三遍：解析条件字段（包括固定长度和变长，以及默认分支）
    // 使用 result 而非 context 做条件判断，使嵌套条件中父字段解析后立即可用
    for (const Field& f : m_fields) {
        // 无条件非默认分支已在前面处理
        if (f.conditions.isEmpty() && !f.isDefaultBranch) {
            continue;
        }

        // 先检查条件是否满足，不满足则跳过该字段（使用 result 做实时判断）
        if (!evaluateCondition(f, result)) {
            continue;
        }

        // 条件满足，按字段类型解析
        if (f.bitLength > 0) {
            parseField(f, data, result, errorMsg);
        } else {
            parseVariableField(f, data, result, errorMsg);
        }
    }

    return result;
}

// ---------- 解析变长字段（内部辅助）----------
// 根据长度字段指示的字节数，从 data 中读取变长数据并转换为对应类型写入 result
// field:    变长字段定义（bitLength == 0）
// data:     原始二进制数据
// result:   解析结果输出（同时作为长度字段值的来源）
// errorMsg: 错误信息输出
// 返回:     解析成功返回 true
bool ProtocolSchema::parseVariableField(const Field& f, const QByteArray& data,
                                        QJsonObject& result, QString* errorMsg) const {
    // 校验长度字段是否已解析
    if (!result.contains(f.lenField)) {
        if (errorMsg) *errorMsg = QString("Length field '%1' missing for '%2'").arg(f.lenField, f.name);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }

    // 读取长度值并校验范围
    bool ok = false;
    qint64 lenBytes = result[f.lenField].toVariant().toLongLong(&ok);
    if (!ok || lenBytes < 0 || lenBytes > MAX_VAR_BYTE_SIZE) {
        if (errorMsg) *errorMsg = QString("Invalid length for field '%1'").arg(f.name);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }

    // 计算比特偏移并防止 int 溢出
    qint64 bitOffset64 = absoluteBitOffset(f);
    if (bitOffset64 > std::numeric_limits<int>::max()) {
        if (errorMsg) *errorMsg = QString("Var field %1 bit offset overflow").arg(f.name);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }
    int bitOffset = static_cast<int>(bitOffset64);
    qint64 bitLength64 = lenBytes * 8;

    // 边界检查：确保变长字段不超出数据范围
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    if (bitOffset64 + bitLength64 > totalBits) {
        if (errorMsg) *errorMsg = QString("Variable field '%1' out of bounds").arg(f.name);
        result.insert(f.name, QJsonValue::Null);
        return false;
    }
    int bitLength = static_cast<int>(bitLength64);

    // 读取原始字节
    QByteArray bytes;
    QString subErr;
    if (!readBitsToBytes(data, bitOffset, bitLength, bytes, f.endian, f.bitOrder, &subErr)) {
        if (errorMsg) *errorMsg = subErr;
        result.insert(f.name, QJsonValue::Null);
        return false;
    }

    // 按字段类型转换为 JSON 值
    switch (f.type) {
    case HexString: result.insert(f.name, QString::fromLatin1(bytes.toHex())); break;
    case Base64:    result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
    case RawBytes:  result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
    case String:    result.insert(f.name, QString::fromUtf8(bytes)); break;
    default:        result.insert(f.name, QJsonValue::Null);
    }
    return true;
}

// ---------- 打包 ----------
// 将 JSON 对象按字段定义打包为二进制数据
// 失败时 out 会被清空，确保调用方拿到干净的空数据
bool ProtocolSchema::pack(const QJsonObject& values, QByteArray& out, QString* errorMsg) const {
    QMutexLocker lock(&m_mutex);

    // 使用 lambda 包裹主体逻辑，确保所有 return false 路径退出后统一清空 out
    bool ok = [&]() -> bool {
            QString errBuf;
            QHash<QString, QString> lenToVar;
            QHash<QString, int> varLengths;
            QHash<QString, QByteArray> varContents;
            QJsonObject context = values;  // 用于条件求值的上下文

            // 构建长度字段 -> 变长字段映射（仅无条件非默认分支）
            for (const Field& f : m_fields) {
            if (f.bitLength == 0 && f.conditions.isEmpty() && !f.isDefaultBranch) {
            lenToVar[f.lenField] = f.name;
}
}

            // 计算无条件变长字段的内容（仅无条件非默认分支）
            for (const Field& f : m_fields) {
            if (f.bitLength == 0 && f.conditions.isEmpty() && !f.isDefaultBranch) {
            if (!values.contains(f.name)) {
            continue;
}
            QJsonValue val = values.value(f.name);
            if (val.isNull() || val.isUndefined()) {
            errBuf = QString("Missing value for variable field '%1'").arg(f.name);
            if (errorMsg) *errorMsg = errBuf;
            return false;
}
            QString subErr;
            QByteArray content = encodeValue(val, f.type, -1, &subErr);
            if (content.isNull()) {
            errBuf = QString("Encode var field %1 failed: %2").arg(f.name, subErr);
            if (errorMsg) *errorMsg = errBuf;
            return false;
}
            if (content.size() > MAX_VAR_BYTE_SIZE) {
            errBuf = QString("Var field %1 size exceed max limit %2").arg(f.name).arg(MAX_VAR_BYTE_SIZE);
            if (errorMsg) *errorMsg = errBuf;
            return false;
}
            varContents[f.name] = content;
            varLengths[f.name] = content.size();
}
}

// 计算条件变长字段的内容（根据条件值或默认分支）
for (const Field& f : m_fields) {
    if (f.bitLength == 0 && (!f.conditions.isEmpty() || f.isDefaultBranch)) {
        if (evaluateCondition(f, context)) {
            QJsonValue val = values.value(f.name);
            if (val.isNull() || val.isUndefined()) {
                errBuf = QString("Missing value for variable field '%1'").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            QString subErr;
            QByteArray content = encodeValue(val, f.type, -1, &subErr);
            if (content.isNull()) {
                errBuf = QString("Encode var field %1 failed: %2").arg(f.name, subErr);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            if (content.size() > MAX_VAR_BYTE_SIZE) {
                errBuf = QString("Var field %1 size exceed max limit %2").arg(f.name).arg(MAX_VAR_BYTE_SIZE);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            varContents[f.name] = content;
            varLengths[f.name] = content.size();
        }
    }
}

// 计算总最大bit，防止int溢出
qint64 maxBit = 0;
for (const Field& f : m_fields) {
    if (!values.contains(f.name)) {
        continue;
    }
    qint64 bitStart = absoluteBitOffset(f);
    qint64 bitEnd = bitStart;
    if (f.bitLength > 0) {
        bitEnd += f.bitLength - 1;
    } else {
        QByteArray content = varContents.value(f.name);
        if (!content.isEmpty()) bitEnd += static_cast<qint64>(content.size()) * 8 - 1;
    }
    if (bitEnd > maxBit) maxBit = bitEnd;
}

if (maxBit < 0) {
    errBuf = "Calculated frame bit length negative";
    if (errorMsg) *errorMsg = errBuf;
    return false;
}

// 分配内存
qint64 byteCount64 = (maxBit + 7) / 8;
if (byteCount64 > std::numeric_limits<int>::max()) {
    errBuf = "Frame size overflow int limit";
    if (errorMsg) *errorMsg = errBuf;
    return false;
}
int byteCount = static_cast<int>(byteCount64);
out.resize(byteCount);
out.fill(0);

// 按bit偏移升序排序字段写入，防止覆盖
QVector<Field> sortedFields = getSortedFields();
for (const Field& f : sortedFields) {
    // 条件字段或默认分支：检查条件是否满足
    if ((!f.conditions.isEmpty() || f.isDefaultBranch) && !evaluateCondition(f, context)) {
        continue;  // 条件不满足，跳过此字段
    }
    if (!values.contains(f.name)) {
        continue;
    }
    qint64 bitOffset64 = absoluteBitOffset(f);
    if (bitOffset64 > std::numeric_limits<int>::max()) {
        errBuf = QString("Field %1 bit offset overflow").arg(f.name);
        if (errorMsg) *errorMsg = errBuf;
        return false;
    }
    int bitOffset = static_cast<int>(bitOffset64);

    if (f.bitLength > 0) {
        // 长度字段（可能同时是条件字段）
        if (lenToVar.contains(f.name)) {
            QString varName = lenToVar[f.name];
            int lenBytes = varLengths.value(varName, -1);
            if (lenBytes > MAX_VAR_BYTE_SIZE) {
                errBuf = QString("Variable length %1 exceeds max limit %2").arg(lenBytes).arg(MAX_VAR_BYTE_SIZE);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            if (lenBytes < 0) {
                errBuf = QString("Length field '%1' for '%2' not computed").arg(f.name, varName);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            quint64 intVal = static_cast<quint64>(lenBytes);
            quint64 masked = intVal & maskBits(f.bitLength);
            if (masked != intVal) {
                errBuf = QString("Length value %1 overflow field %2 bit width").arg(lenBytes).arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            QString subErr;
            if (!writeBits(out, bitOffset, f.bitLength, masked, f.endian, f.bitOrder, &subErr)) {
                errBuf = QString("Write len field %1 failed: %2").arg(f.name, subErr);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
        } else {
            QJsonValue val = values.value(f.name);
            if (val.isNull() || val.isUndefined()) {
                errBuf = QString("Missing value for field '%1'").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            if (f.type == Int || f.type == UInt) {
                quint64 intVal;
                QString subErr;
                if (f.hasMapping) {
                    // 枚举映射模式：从字符串反向查找原始字节值
                    // 跳过 factor/offset 线性变换（互斥），直接写入原始值
                    if (!reverseMapLookup(f, val, intVal, &subErr)) {
                        errBuf = QString("Convert field %1 value failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                    // 范围检查：原始值不得超过字段位宽
                    quint64 maxVal = maskBits(f.bitLength);
                    if ((intVal & maxVal) != intVal) {
                        errBuf = QString("Mapped raw value %1 overflow field %2 bit width")
                                .arg(intVal).arg(f.name);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                } else {
                    // 普通模式：应用逆线性变换得到原始整数值
                    if (!valueToInteger(val, f.bitLength, f.isSigned, f.factor, f.offset, intVal, &subErr)) {
                        errBuf = QString("Convert field %1 value failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                }
                if (!writeBits(out, bitOffset, f.bitLength, intVal, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Write field %1 failed: %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
            } else {
                int fixedBytes = (f.bitLength + 7) / 8;
                QString subErr;
                QByteArray bytes = encodeValue(val, f.type, fixedBytes, &subErr);
                if (bytes.isNull()) {
                    errBuf = QString("Encode field %1 failed: %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
                if (!writeBitsToBytes(out, bitOffset, bytes, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Write bytes field %1 failed: %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
            }
        }
    } else {
        // 变长字段写入
        QByteArray content = varContents.value(f.name);
        if (!content.isEmpty()) {
            QString subErr;
            if (!writeBitsToBytes(out, bitOffset, content, f.endian, f.bitOrder, &subErr)) {
                errBuf = QString("Write var field %1 failed: %2").arg(f.name, subErr);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
        }
    }
}

return true;
}();

// 失败时清空输出缓冲区，确保调用方拿到干净的空数据
if (!ok) {
    out.clear();
    return false;
}
return true;
}

// ---------- 打包便捷版本 ----------
QByteArray ProtocolSchema::packToArray(const QJsonObject& values, QString* errorMsg) const {
    QByteArray result;
    if (!pack(values, result, errorMsg)) result.clear();
    return result;
}

// ---------- 核心位操作 readBits ----------
// 从数据缓冲区读取任意比特偏移和长度（1~64位）的整数
// data:     输入数据缓冲区
// bitOffset: 起始比特偏移（0-based）
// bitLength: 读取的比特数（1~64）
// out:      输出读取到的无符号整数值
// endian:   字节序（仅 bitLength > 8 时有效）
// bitOrder:  字节内比特顺序（Physical 或 MsbFirst）
// err:      错误信息输出
// 返回:     成功返回 true
bool ProtocolSchema::readBits(const QByteArray& data, int bitOffset, int bitLength,
                              quint64& out, Endian endian, BitOrder bitOrder, QString* err) const {
    out = 0;
    // 参数校验：偏移非负、长度合法
    if (bitOffset < 0) {
        if (err) *err = "bitOffset negative";
        return false;
    }
    if (bitLength <= 0 || bitLength > MAX_BIT_WIDTH) {
        if (err) *err = QString("bitLength out of limit 1~64, got %1").arg(bitLength);
        return false;
    }
    // 空数据检查必须在越界检查之前，否则空数据的越界计算无意义
    if (data.isEmpty()) {
        if (err) *err = "Empty input data buffer";
        return false;
    }
    // 越界检查：使用 64 位运算防止溢出
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (err) *err = "Read out of data bit bounds";
        return false;
    }

    // Physical 模式：bit0 为最低位，按物理比特索引逐位读取
    if (bitOrder == Physical) {
        quint64 value = 0;
        for (int i = 0; i < bitLength; ++i) {
            qint64 bitIdx = static_cast<qint64>(bitOffset) + i;
            int byteIdx = static_cast<int>(bitIdx / 8);
            int bitInByte = static_cast<int>(bitIdx % 8);
            quint8 byte = static_cast<quint8>(data.at(byteIdx));
            quint8 bitVal = (byte >> bitInByte) & 0x01;
            if (bitVal)
                value |= (1ULL << i);
        }
        // 字节序处理：仅多字节（>8位）时生效，反转字节顺序以匹配大端语义
        if (bitLength > 8 && endian == BigEndian) {
            int byteCount = (bitLength + 7) / 8;
            quint64 swapped = 0;
            for (int i = 0; i < byteCount; ++i) {
                swapped |= ((value >> (i * 8)) & 0xFF) << ((byteCount - 1 - i) * 8);
            }
            value = swapped;
        }
        out = value & maskBits(bitLength);
        return true;
    }

    // MsbFirst 模式：startBit=0 表示从字节高位开始，按网络字节序逐位读取
    int startByte = bitOffset / 8;
    int startBitInByte = bitOffset % 8;
    int endByte = (bitOffset + bitLength - 1) / 8;
    int byteSpan = endByte - startByte + 1;
    QByteArray bytes = data.mid(startByte, byteSpan);

    quint64 value = 0;
    int bitsLeft = bitLength;
    int byteIdx = 0;
    int bitPos = startBitInByte;
    while (bitsLeft > 0 && byteIdx < bytes.size()) {
        quint8 byte = static_cast<quint8>(bytes[byteIdx]);
        int bitsFromThisByte = qMin(8 - bitPos, bitsLeft);
        int shift = 8 - bitPos - bitsFromThisByte;
        quint8 segment = (byte >> shift) & ((1 << bitsFromThisByte) - 1);
        value = (value << bitsFromThisByte) | segment;
        bitsLeft -= bitsFromThisByte;
        bitPos = 0;
        ++byteIdx;
    }

    // 字节序处理：MsbFirst 默认按大端组装，若需小端则反转字节顺序
    if (bitLength > 8 && endian == LittleEndian) {
        int byteCount = (bitLength + 7) / 8;
        quint64 swapped = 0;
        for (int i = 0; i < byteCount; ++i) {
            swapped |= ((value >> (i * 8)) & 0xFF) << ((byteCount - 1 - i) * 8);
        }
        value = swapped;
    }
    out = value & maskBits(bitLength);
    return true;
}

// ---------- 核心位操作 writeBits ----------
bool ProtocolSchema::writeBits(QByteArray& data, int bitOffset, int bitLength, quint64 value,
                               Endian endian, BitOrder bitOrder, QString* errorMsg) const {
    if (bitOffset < 0) {
        if (errorMsg) *errorMsg = "bitOffset negative";
        return false;
    }
    if (bitLength <= 0 || bitLength > MAX_BIT_WIDTH) {
        if (errorMsg) *errorMsg = QString("bitLength out of limit 1~64, got %1").arg(bitLength);
        return false;
    }
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (errorMsg) *errorMsg = "Write out of bounds";
        return false;
    }
    if (data.isNull()) {
        if (errorMsg) *errorMsg = "Target data buffer null";
        return false;
    }
    value &= maskBits(bitLength);

    // Physical 模式
    if (bitOrder == Physical) {
        quint64 toWrite = value;
        if (bitLength > 8 && endian == BigEndian) {
            int byteCount = (bitLength + 7) / 8;
            quint64 swapped = 0;
            for (int i = 0; i < byteCount; ++i) {
                swapped |= ((value >> ((byteCount - 1 - i) * 8)) & 0xFF) << (i * 8);
            }
            toWrite = swapped;
        }
        for (int i = 0; i < bitLength; ++i) {
            qint64 bitIdx = static_cast<qint64>(bitOffset) + i;
            int byteIdx = static_cast<int>(bitIdx / 8);
            int bitInByte = static_cast<int>(bitIdx % 8);
            quint8 bitVal = (toWrite >> i) & 0x01;
            uchar* targetByte = reinterpret_cast<uchar*>(data.data() + byteIdx);
            if (bitVal)
                *targetByte |= (1 << bitInByte);
            else
                *targetByte &= ~(1 << bitInByte);
        }
        return true;
    }

    // MsbFirst 模式
    quint64 internalValue = value;
    if (bitLength > 8 && endian == LittleEndian) {
        int byteCount = (bitLength + 7) / 8;
        quint64 swapped = 0;
        for (int i = 0; i < byteCount; ++i) {
            swapped |= ((internalValue >> ((byteCount - 1 - i) * 8)) & 0xFF) << (i * 8);
        }
        internalValue = swapped;
    }

    QByteArray bitsBytes = decomposeToBigEndianBits(internalValue, bitLength);
    int bitsWritten = 0;
    int srcByteIdx = 0;
    int srcBitPos = 0;
    while (bitsWritten < bitLength && srcByteIdx < bitsBytes.size()) {
        quint8 srcByte = static_cast<quint8>(bitsBytes[srcByteIdx]);
        int bitsRemainingInSrc = 8 - srcBitPos;
        qint64 targetBitIdx = static_cast<qint64>(bitOffset) + bitsWritten;
        int targetByteIdx = static_cast<int>(targetBitIdx / 8);
        int targetBitPos = static_cast<int>(targetBitIdx % 8);
        int bitsToWrite = qMin(bitsRemainingInSrc, bitLength - bitsWritten);
        quint8 srcSegment = (srcByte >> (8 - srcBitPos - bitsToWrite)) & ((1 << bitsToWrite) - 1);
        uchar* targetByte = reinterpret_cast<uchar*>(data.data() + targetByteIdx);

        quint8 clearMask;
        if (bitsToWrite == 8) {
            clearMask = 0x00;
        } else {
            clearMask = ~(((1 << bitsToWrite) - 1) << (8 - targetBitPos - bitsToWrite));
        }
        *targetByte &= clearMask;
        *targetByte |= (srcSegment << (8 - targetBitPos - bitsToWrite));
        bitsWritten += bitsToWrite;
        srcBitPos += bitsToWrite;
        if (srcBitPos >= 8) {
            srcBitPos = 0;
            ++srcByteIdx;
        }
    }
    return true;
}

// ---------- readBitsToBytes ----------
bool ProtocolSchema::readBitsToBytes(const QByteArray& data, int bitOffset, int bitLength,
                                     QByteArray& out, Endian endian, BitOrder bitOrder, QString* err) const {
    out.clear();
    if (bitLength <= 0 || bitOffset < 0) {
        if (err) *err = "Invalid bit offset/length for bytes read";
        return false;
    }
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (err) *err = "Bytes read out of data bounds";
        return false;
    }

    int byteLen = (bitLength + 7) / 8;
    out.resize(byteLen);
    out.fill(0);
    for (int i = 0; i < byteLen; ++i) {
        int bitsThisByte = qMin(8, bitLength - i * 8);
        quint64 byteVal = 0;
        QString subErr;
        if (!readBits(data, bitOffset + i * 8, bitsThisByte, byteVal, endian, bitOrder, &subErr)) {
            if (err) *err = subErr;
            out.clear();
            return false;
        }
        out[i] = static_cast<char>(byteVal & 0xFF);
    }
    return true;
}

// ---------- writeBitsToBytes ----------
bool ProtocolSchema::writeBitsToBytes(QByteArray& data, int bitOffset, const QByteArray& bytes,
                                      Endian endian, BitOrder bitOrder, QString* errorMsg) const {
    if (bitOffset < 0) return true;
    if (bytes.isEmpty()) return true;

    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    int bitLength = bytes.size() * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (errorMsg) *errorMsg = "WriteBitsToBytes out of bounds";
        return false;
    }

    for (int i = 0; i < bytes.size(); ++i) {
        quint8 byteVal = static_cast<quint8>(bytes[i]);
        QString subErr;
        if (!writeBits(data, bitOffset + i * 8, 8, byteVal, endian, bitOrder, &subErr)) {
            if (errorMsg) *errorMsg = subErr;
            return false;
        }
    }
    return true;
}

// ---------- encodeValue ----------
QByteArray ProtocolSchema::encodeValue(const QJsonValue& value, ValueType type, int fixedBytes,
                                       QString* errorMsg) const {
    QByteArray result;
    switch (type) {
    case HexString: {
        QString hex = value.toString().trimmed();
        if ((hex.length() % 2) != 0) {
            if (errorMsg) *errorMsg = QString("Hex string length must even, got %1").arg(hex.length());
            return QByteArray();
        }
        result = QByteArray::fromHex(hex.toLatin1());
        if (result.isEmpty() && !hex.isEmpty()) {
            if (errorMsg) *errorMsg = "Invalid hex string";
            return QByteArray();
        }
        break;
    }
    case Base64:
    case RawBytes: {
        QString b64 = value.toString().trimmed();
        result = QByteArray::fromBase64(b64.toLatin1());
        if (result.isEmpty() && !b64.isEmpty()) {
            if (errorMsg) *errorMsg = "Invalid base64 string";
            return QByteArray();
        }
        break;
    }
    case String: {
        QString str = value.toString();
        result = str.toUtf8();
        break;
    }
    default:
        if (errorMsg) *errorMsg = "encodeValue called on non-bytes type";
        return QByteArray();
    }

    if (fixedBytes > 0) {
        if (result.size() < fixedBytes) {
            result.append(fixedBytes - result.size(), '\0');
        } else if (result.size() > fixedBytes) {
            if (errorMsg) *errorMsg = "Value too long for fixed-length field";
            return QByteArray();
        }
    }
    return result;
}

// ---------- valueToInteger ----------
bool ProtocolSchema::valueToInteger(const QJsonValue& value, int bitLength, bool isSigned,
                                    double factor, double offset,
                                    quint64& out, QString* errorMsg) const {
    out = 0;
    if (!value.isDouble()) {
        if (errorMsg) *errorMsg = "Value is not a number";
        return false;
    }

    double physicalValue = value.toDouble();
    if (qAbs(factor) < FLOAT_EPS) {
        if (errorMsg) *errorMsg = "Factor is zero, cannot invert";
        return false;
    }

    double rawDouble = (physicalValue - offset) / factor;
    qint64 signedRaw = static_cast<qint64>(std::round(rawDouble));

    if (isSigned) {
        qint64 minVal;
        qint64 maxVal;
        if (bitLength == 64) {
            minVal = std::numeric_limits<qint64>::min();
            maxVal = std::numeric_limits<qint64>::max();
        } else {
            minVal = -(1ULL << (bitLength - 1));
            maxVal = static_cast<qint64>((1ULL << (bitLength - 1)) - 1);
        }
        if (signedRaw < minVal || signedRaw > maxVal) {
            if (errorMsg) *errorMsg = QString("Inverse transformed value %1 out of signed range [%2,%3]")
                    .arg(signedRaw).arg(minVal).arg(maxVal);
            return false;
        }
        out = static_cast<quint64>(signedRaw) & maskBits(bitLength);
    } else {
        quint64 max = maskBits(bitLength);
        if (signedRaw < 0 || static_cast<quint64>(signedRaw) > max) {
            if (errorMsg) *errorMsg = QString("Inverse transformed value %1 out of unsigned range [0,%2]")
                    .arg(signedRaw).arg(max);
            return false;
        }
        out = static_cast<quint64>(signedRaw);
    }
    return true;
}

// ---------- reverseMapLookup ----------
// 反向映射查找：根据 JSON 值（打包输入）查找原始字节值
// 用于枚举映射字段打包时将字符串还原为原始字节值写入比特流
// 查找策略（保证打包可靠性）:
//   1. 若值为字符串：先在 valueMap 中反向匹配字符串得到原始字节值
//   2. 若未命中映射表：尝试将字符串解析为数值（兼容用户传入 "3" 这类数值字符串）
//   3. 若值为数字：直接使用（兼容用户传入数字的情况，不推荐与映射混用）
// f:    字段定义（含 valueMap）
// val:  打包输入的 JSON 值
// out:  输出原始字节值
// err:  错误信息输出
// 返回: 成功返回 true
bool ProtocolSchema::reverseMapLookup(const Field& f, const QJsonValue& val,
                                      quint64& out, QString* err) const {
    out = 0;
    // 字符串输入：优先反向查表
    if (val.isString()) {
        const QString str = val.toString();
        for (auto it = f.valueMap.constBegin(); it != f.valueMap.constEnd(); ++it) {
            if (it.value() == str) {
                out = it.key();
                return true;
            }
        }
        // 未命中映射表：尝试将字符串解析为无符号数值
        bool ok = false;
        qulonglong parsed = str.toULongLong(&ok);
        if (ok) {
            out = parsed;
            return true;
        }
        if (err) *err = QString("String '%1' not found in map and not a valid number").arg(str);
        return false;
    }
    // 数字输入：直接使用（兼容用户直接传数字的场景）
    if (val.isDouble()) {
        double d = val.toDouble();
        if (d < 0) {
            if (err) *err = "Negative value cannot map to unsigned raw byte value";
            return false;
        }
        out = static_cast<quint64>(d);
        return true;
    }
    if (err) *err = "Value is neither string nor number for mapped field";
    return false;
}

// ---------- applyLinearTransform ----------
// 将原始整数值（读取后）应用系数和偏移变换为最终物理值
// 对有符号字段进行符号扩展，注意 64 位字段需特殊处理以避免溢出
// rawValue:  读取到的无符号原始值
// factor:    线性变换系数
// offset:    线性变换偏移
// isSigned:  字段是否为有符号类型
// bitLength: 字段比特长度（1~64）
// 返回:      变换后的物理值 = rawValue * factor + offset
double ProtocolSchema::applyLinearTransform(quint64 rawValue, double factor, double offset,
                                            bool isSigned, int bitLength) {
    if (!isSigned) {
        // 无符号字段：直接用 quint64 转 double，避免符号扩展
        // 注意：double 仅有 53 位尾数，64 位无符号整数会丢失精度
        // 但这是 JSON double 的固有限制，无法同时保证范围和精度
        double value = static_cast<double>(rawValue);
        return value * factor + offset;
    }

    // 有符号字段：进行符号扩展后再转换
    qint64 signedRaw = 0;
    if (bitLength == 64) {
        // 64 位有符号整数：直接位转换（C++ 标准保证 quint64 与 qint64 互转保留位模式）
        signedRaw = static_cast<qint64>(rawValue);
    } else if (bitLength > 0) {
        // 非 64 位有符号：检查最高有效位是否为 1，是则进行符号扩展
        quint64 signBit = 1ULL << (bitLength - 1);
        if (rawValue & signBit) {
            // 符号扩展：将高位全部置 1
            signedRaw = static_cast<qint64>(rawValue | (~0ULL << bitLength));
        } else {
            signedRaw = static_cast<qint64>(rawValue);
        }
    }

    double value = static_cast<double>(signedRaw);
    return value * factor + offset;
}

// ==================== ConditionalBuilder 类实现 ====================

ConditionalBuilder::ConditionalBuilder(ProtocolSchema* schema, const QString& conditionField,
                                       const QVariant& conditionValue, bool isDefault)
    : m_schema(schema)
    , m_conditionField(conditionField)
    , m_conditionValue(conditionValue)
    , m_isDefault(isDefault) { }

ConditionalBuilder::~ConditionalBuilder() { }

// 添加固定长度字段到当前分支
ConditionalBuilder& ConditionalBuilder::addField(const QString& name, int startByte, int startBit,
                                                 int bitLength, ValueType type, Endian endian,
                                                 BitOrder bitOrder, bool isSigned,
                                                 double factor, double offset, QString* err) {
    QMutexLocker lock(&m_schema->m_mutex);

    // 参数校验
    if (name.trimmed().isEmpty()) {
        if (err) *err = "Field name cannot be empty";
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Field %1 startBit must 0~7").arg(name);
        return *this;
    }
    if (bitLength < 1 || bitLength > ProtocolSchema::MAX_BIT_WIDTH) {
        if (err) *err = QString("Field %1 bitLength must 1~64").arg(name);
        return *this;
    }

    // 同名字段重复检测
    for (const auto& f : m_schema->m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate field name: %1").arg(name);
            return *this;
        }
    }

    ProtocolSchema::Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = bitLength;
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = isSigned;
    f.factor = factor;
    f.offset = offset;
    f.lenField = "";
    f.valueMap.clear();         // 默认无枚举映射
    f.hasMapping = false;
    f.conditions = m_inheritedConditions;
    if (!m_conditionField.isEmpty() && !m_isDefault) {
        f.conditions.append({m_conditionField, m_conditionValue});
    }
    f.isDefaultBranch = m_isDefault;
    if (m_isDefault) {
        f.defaultBranchField = m_conditionField;
    } else {
        f.defaultBranchField = "";
    }
    m_schema->m_fields.append(f);
    m_schema->invalidateSortCache();  // 字段变化，使排序缓存失效
    return *this;
}

// 添加变长字段到当前分支
ConditionalBuilder& ConditionalBuilder::addVariableField(const QString& name, int startByte, int startBit,
                                                         const QString& lenField, ValueType type,
                                                         Endian endian, BitOrder bitOrder,
                                                         double factor, double offset, QString* err) {
    QMutexLocker lock(&m_schema->m_mutex);

    if (name.trimmed().isEmpty()) {
        if (err) *err = "Variable field name cannot be empty";
        return *this;
    }
    if (lenField.trimmed().isEmpty()) {
        if (err) *err = QString("Var field %1 lenField empty").arg(name);
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Var field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Var field %1 startBit must 0~7").arg(name);
        return *this;
    }

    // 同名字段重复检测
    for (const auto& f : m_schema->m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate var field name: %1").arg(name);
            return *this;
        }
    }

    ProtocolSchema::Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = 0;
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = false;
    f.lenField = lenField;
    f.factor = factor;
    f.offset = offset;
    f.valueMap.clear();         // 默认无枚举映射
    f.hasMapping = false;
    f.conditions = m_inheritedConditions;
    if (!m_conditionField.isEmpty() && !m_isDefault) {
        f.conditions.append({m_conditionField, m_conditionValue});
    }
    f.isDefaultBranch = m_isDefault;
    if (m_isDefault) {
        f.defaultBranchField = m_conditionField;
    } else {
        f.defaultBranchField = "";
    }
    m_schema->m_fields.append(f);
    m_schema->invalidateSortCache();  // 字段变化，使排序缓存失效
    return *this;
}

// 为最近添加的字段追加枚举映射（条件分支内链式调用）
// 语义同 ProtocolSchema::map，作用于 m_schema 中最近添加的字段
ConditionalBuilder& ConditionalBuilder::map(quint64 rawValue, const QString& str, QString* err) {
    QMutexLocker lock(&m_schema->m_mutex);

    if (m_schema->m_fields.isEmpty()) {
        if (err) *err = "map() called before any addField/addVariableField";
        return *this;
    }
    ProtocolSchema::Field& f = m_schema->m_fields.last();

    // 仅支持整数类型字段（字节值映射语义）
    if (f.type != Int && f.type != UInt) {
        if (err) *err = QString("map() only valid for Int/UInt field, field '%1' type mismatch").arg(f.name);
        return *this;
    }

    f.valueMap.insert(rawValue, str);
    if (!f.hasMapping) {
        f.hasMapping = true;
        f.factor = 1.0;   // 互斥：禁用线性变换
        f.offset = 0.0;
    }
    m_schema->invalidateSortCache();
    return *this;
}

// 在当前分支内部开启新的条件分支
ConditionalBuilder ConditionalBuilder::when(const QString& fieldName, const QVariant& value) {
    ConditionalBuilder builder(m_schema, fieldName, value, false);
    // 继承当前 builder 的条件列表，形成 AND 逻辑
    builder.m_inheritedConditions = m_inheritedConditions;
    if (!m_conditionField.isEmpty() && !m_isDefault) {
        builder.m_inheritedConditions.append({m_conditionField, m_conditionValue});
    }
    return builder;
}

// 当前分支的默认分支
ConditionalBuilder ConditionalBuilder::otherwise() {
    // 使用当前 builder 的条件字段作为默认分支的条件字段
    ConditionalBuilder builder(m_schema, m_conditionField, QVariant(), true);
    // 继承父级条件
    builder.m_inheritedConditions = m_inheritedConditions;
    return builder;
}

// 结束当前分支
ProtocolSchema& ConditionalBuilder::endBranch() {
    return *m_schema;
}

} // namespace Sqz
