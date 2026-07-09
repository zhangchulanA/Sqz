#include "JsonUtils.h"
#include <QFileInfo>
#include <QDir>
#include <QVariantMap>
#include <algorithm>
#include <QUrl>


namespace Sqz::Utils {
// 静态成员初始化
QString JsonUtils::m_lastError = "";
QMutex JsonUtils::m_errorMutex;

void JsonUtils::setError(const QString& err)
{
    QMutexLocker locker(&m_errorMutex);
    m_lastError = err;
    qWarning() << "JsonUtils Error:" << err;
}

QString JsonUtils::getLastError()
{
    QMutexLocker locker(&m_errorMutex);
    return m_lastError;
}

// ========================== 1. 文件操作 ==========================
QJsonDocument JsonUtils::loadFileToDoc(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError("文件打开失败：" + file.errorString());
        return QJsonDocument();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        setError("JSON解析失败：" + parseErr.errorString() + " 偏移：" + QString::number(parseErr.offset));
        return QJsonDocument();
    }
    return doc;
}

QJsonObject JsonUtils::loadFileToObject(const QString& filePath)
{
    QJsonDocument doc = loadFileToDoc(filePath);
    return doc.isObject() ? doc.object() : QJsonObject();
}

QJsonArray JsonUtils::loadFileToArray(const QString& filePath)
{
    QJsonDocument doc = loadFileToDoc(filePath);
    return doc.isArray() ? doc.array() : QJsonArray();
}

bool JsonUtils::writeDocToFile(const QJsonDocument& doc, const QString& filePath, bool format)
{
    if (doc.isNull() || doc.isEmpty()) {
        setError("写入失败：JSON 为空");
        return false;
    }

    QFileInfo info(filePath);
    // 检查并创建目录
    if (!QDir().mkpath(info.absolutePath())) {
        setError("写入失败：无法创建目录 " + info.absolutePath());
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError("文件写入失败：" + file.errorString());
        return false;
    }

    QByteArray data = format ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
    file.write(data);
    file.close();
    return true;
}

bool JsonUtils::writeObjectToFile(const QJsonObject& obj, const QString& filePath, bool format)
{
    return writeDocToFile(QJsonDocument(obj), filePath, format);
}

bool JsonUtils::writeArrayToFile(const QJsonArray& arr, const QString& filePath, bool format)
{
    return writeDocToFile(QJsonDocument(arr), filePath, format);
}

// ========================== 2. 字符串互转 ==========================
QString JsonUtils::objectToString(const QJsonObject& obj, bool format)
{
    QJsonDocument doc(obj);
    return format ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
}

QString JsonUtils::arrayToString(const QJsonArray& arr, bool format)
{
    QJsonDocument doc(arr);
    return format ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
}

QJsonObject JsonUtils::stringToObject(const QString& jsonStr)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setError("字符串转对象失败：" + err.errorString());
        return QJsonObject();
    }
    return doc.object();
}

QJsonArray JsonUtils::stringToArray(const QString& jsonStr)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        setError("字符串转数组失败：" + err.errorString());
        return QJsonArray();
    }
    return doc.array();
}

// ========================== 3. 安全取值 ==========================
int JsonUtils::getInt(const QJsonObject& obj, const QString& key, int def) { return obj.value(key).toInt(def); }
double JsonUtils::getDouble(const QJsonObject& obj, const QString& key, double def) { return obj.value(key).toDouble(def); }
QString JsonUtils::getString(const QJsonObject& obj, const QString& key, const QString& def) { return obj.value(key).toString(def); }
bool JsonUtils::getBool(const QJsonObject& obj, const QString& key, bool def) { return obj.value(key).toBool(def); }
QJsonObject JsonUtils::getObject(const QJsonObject& obj, const QString& key) { return obj.value(key).toObject(); }
QJsonArray JsonUtils::getArray(const QJsonObject& obj, const QString& key) { return obj.value(key).toArray(); }
QJsonValue JsonUtils::getValue(const QJsonObject& obj, const QString& key) { return obj.value(key); }

// ========================== 4. 深度取值 ==========================
QJsonValue JsonUtils::getDeepValue(const QJsonObject& rootObj, const QString& keyPath, const QJsonValue& def)
{
    QStringList keys = keyPath.split('.');
    QJsonValue cur = rootObj;
    for (const QString& k : keys) {
        if (!cur.isObject()) return def;
        cur = cur.toObject().value(k);
        if (cur.isUndefined()) return def;
    }
    return cur;
}

QString JsonUtils::getDeepString(const QJsonObject& rootObj, const QString& keyPath, const QString& def)
{
    return getDeepValue(rootObj, keyPath).toString(def);
}

int JsonUtils::getDeepInt(const QJsonObject& rootObj, const QString& keyPath, int def)
{
    return getDeepValue(rootObj, keyPath).toInt(def);
}

// ========================== 5. 赋值 ==========================
void JsonUtils::setValue(QJsonObject& obj, const QString& key, const QJsonValue& value)
{
    obj[key] = value;
}

// 修复后的 setDeepValue：递归修改，确保修改写回原始对象
void JsonUtils::setDeepValue(QJsonObject& rootObj, const QString& keyPath, const QJsonValue& value)
{
    QStringList parts = keyPath.split('.');
    if (parts.isEmpty()) return;

    if (parts.size() == 1) {
        rootObj[parts.first()] = value;
        return;
    }

    QString first = parts.first();
    parts.removeFirst();
    QString rest = parts.join('.');

    // 获取或创建子对象
    QJsonObject subObj = rootObj[first].toObject();
    setDeepValue(subObj, rest, value);
    rootObj[first] = subObj;
}

// ========================== 6. 键操作 ==========================
bool JsonUtils::contains(const QJsonObject& obj, const QString& key)
{
    return obj.contains(key);
}

bool JsonUtils::containsDeep(const QJsonObject& rootObj, const QString& keyPath)
{
    return !getDeepValue(rootObj, keyPath).isUndefined();
}

void JsonUtils::removeKey(QJsonObject& obj, const QString& key)
{
    obj.remove(key);
}

// 修复后的 removeDeepKey：递归删除
void JsonUtils::removeDeepKey(QJsonObject& rootObj, const QString& keyPath)
{
    QStringList parts = keyPath.split('.');
    if (parts.isEmpty()) return;

    if (parts.size() == 1) {
        rootObj.remove(parts.first());
        return;
    }

    QString first = parts.first();
    parts.removeFirst();
    if (!rootObj.contains(first) || !rootObj[first].isObject()) return;

    QJsonObject subObj = rootObj[first].toObject();
    removeDeepKey(subObj, parts.join('.'));
    rootObj[first] = subObj;
}

QStringList JsonUtils::getAllKeys(const QJsonObject& obj)
{
    return obj.keys();
}

void JsonUtils::renameKey(QJsonObject& obj, const QString& oldKey, const QString& newKey)
{
    if (obj.contains(oldKey)) {
        obj[newKey] = obj[oldKey];
        obj.remove(oldKey);
    }
}

// ========================== 7. 数组基础操作 ==========================
void JsonUtils::arrayAppend(QJsonArray& arr, const QJsonValue& v) { arr.append(v); }
void JsonUtils::arrayInsert(QJsonArray& arr, int i, const QJsonValue& v) { arr.insert(i, v); }
void JsonUtils::arrayRemoveAt(QJsonArray& arr, int i) { if (i >= 0 && i < arr.size()) arr.removeAt(i); }
void JsonUtils::arrayReplace(QJsonArray& arr, int i, const QJsonValue& v) { if (i >= 0 && i < arr.size()) arr.replace(i, v); }
QJsonValue JsonUtils::arrayGet(const QJsonArray& arr, int i, const QJsonValue& def)
{
    return (i >= 0 && i < arr.size()) ? arr[i] : def;
}
int JsonUtils::arrayFind(const QJsonArray& arr, const QJsonValue& v)
{
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i] == v) return i;
    }
    return -1;
}

// ========================== 8. 合并 & 过滤 ==========================
QJsonObject JsonUtils::mergeObject(const QJsonObject& target, const QJsonObject& source, bool overwrite)
{
    QJsonObject res = target;
    for (auto it = source.begin(); it != source.end(); ++it) {
        if (!res.contains(it.key()) || overwrite) {
            res[it.key()] = it.value();
        }
    }
    return res;
}

QJsonObject JsonUtils::filterKeys(const QJsonObject& obj, const QStringList& keepKeys)
{
    QJsonObject res;
    for (const QString& k : keepKeys) {
        if (obj.contains(k)) res[k] = obj[k];
    }
    return res;
}

// ========================== 9. 类型校验 ==========================
bool JsonUtils::isInt(const QJsonValue& val) { return val.isDouble() && (val.toInt() == val.toDouble()); }
bool JsonUtils::isString(const QJsonValue& val) { return val.isString(); }
bool JsonUtils::isObject(const QJsonValue& val) { return val.isObject(); }
bool JsonUtils::isArray(const QJsonValue& val) { return val.isArray(); }
bool JsonUtils::isNull(const QJsonValue& val) { return val.isNull() || val.isUndefined(); }
bool JsonUtils::isObjectEmpty(const QJsonObject& obj) { return obj.isEmpty(); }
bool JsonUtils::isArrayEmpty(const QJsonArray& arr) { return arr.isEmpty(); }

// ========================== 10. QVariant ==========================
QJsonValue JsonUtils::variantToJson(const QVariant& var) { return QJsonValue::fromVariant(var); }
QVariant JsonUtils::jsonToVariant(const QJsonValue& val) { return val.toVariant(); }
QJsonObject JsonUtils::variantMapToObject(const QVariantMap& map) { return QJsonObject::fromVariantMap(map); }
QVariantMap JsonUtils::objectToVariantMap(const QJsonObject& obj) { return obj.toVariantMap(); }

// ========================== 11. 格式化 ==========================
QString JsonUtils::formatJsonString(const QString& jsonStr)
{
    return objectToString(stringToObject(jsonStr), true);
}
QString JsonUtils::compactJsonString(const QString& jsonStr)
{
    return objectToString(stringToObject(jsonStr), false);
}

// ========================== 12. 深拷贝 ==========================
QJsonObject JsonUtils::copyObject(const QJsonObject& obj) { return QJsonObject(obj); }
QJsonArray JsonUtils::copyArray(const QJsonArray& arr) { return QJsonArray(arr); }

// ========================== 14. 深度合并 ==========================
QJsonObject JsonUtils::mergeDeep(const QJsonObject &target, const QJsonObject &source, bool overwrite)
{
    QJsonObject res = target;
    for (auto it = source.begin(); it != source.end(); ++it) {
        QString key = it.key();
        QJsonValue srcVal = it.value();

        if (res.contains(key) && res[key].isObject() && srcVal.isObject()) {
            res[key] = mergeDeep(res[key].toObject(), srcVal.toObject(), overwrite);
        } else {
            if (!res.contains(key) || overwrite) {
                res[key] = srcVal;
            }
        }
    }
    return res;
}

// ========================== 15. 深度比较 ==========================
bool JsonUtils::isEqual(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() != b.type()) return false;
    if (a.isObject()) return isObjectEqual(a.toObject(), b.toObject());
    if (a.isArray()) return isArrayEqual(a.toArray(), b.toArray());
    return a.toVariant() == b.toVariant();
}

bool JsonUtils::isObjectEqual(const QJsonObject &a, const QJsonObject &b)
{
    if (a.keys() != b.keys()) return false;
    for (const QString& k : a.keys()) {
        if (!isEqual(a[k], b[k])) return false;
    }
    return true;
}

bool JsonUtils::isArrayEqual(const QJsonArray &a, const QJsonArray &b)
{
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (!isEqual(a[i], b[i])) return false;
    }
    return true;
}

// ========================== 16. 扁平化/反扁平化 ==========================
static void flattenRecursive(const QJsonObject &obj, QJsonObject &out, const QString &prefix)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        if (it.value().isObject()) {
            flattenRecursive(it.value().toObject(), out, key);
        } else {
            out[key] = it.value();
        }
    }
}

QJsonObject JsonUtils::flatten(const QJsonObject &obj)
{
    QJsonObject out;
    flattenRecursive(obj, out, "");
    return out;
}

QJsonObject JsonUtils::unflatten(const QJsonObject &flatObj)
{
    QJsonObject root;
    for (auto it = flatObj.begin(); it != flatObj.end(); ++it) {
        setDeepValue(root, it.key(), it.value());  // 现在 setDeepValue 已修复
    }
    return root;
}

// ========================== 17. 清理空值 ==========================
QJsonObject JsonUtils::cleanEmptyValues(const QJsonObject &obj)
{
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QJsonValue v = it.value();
        if (v.isNull() || v.isUndefined()) continue;
        if (v.isString() && v.toString().isEmpty()) continue;
        if (v.isObject() && v.toObject().isEmpty()) continue;
        if (v.isArray() && v.toArray().isEmpty()) continue;
        out[it.key()] = v;
    }
    return out;
}

// ========================== 18. 键大小写 ==========================
QJsonObject JsonUtils::keysToLower(const QJsonObject &obj)
{
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        out[it.key().toLower()] = it.value();
    }
    return out;
}

QJsonObject JsonUtils::keysToUpper(const QJsonObject &obj)
{
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        out[it.key().toUpper()] = it.value();
    }
    return out;
}

// ========================== 19. 数组高级 ==========================
void JsonUtils::arraySort(QJsonArray &arr, bool ascending)
{
    QList<QJsonValue> list;
    for (const auto& v : arr) {
        list.append(v);
    }

    std::sort(list.begin(), list.end(), [ascending](const QJsonValue &a, const QJsonValue &b) {
        // 注意：不同类型比较结果依赖 QVariant 的行为，不一定符合预期。建议数组元素类型一致。
        if (ascending)
            return a.toVariant() < b.toVariant();
        else
            return a.toVariant() > b.toVariant();
    });

    arr = QJsonArray();
    for (const auto& v : list) {
        arr.append(v);
    }
}

void JsonUtils::arraySortByField(QJsonArray &arr, const QString &field, bool ascending)
{
    QList<QJsonValue> list;
    for (const auto& v : arr) {
        list.append(v);
    }

    std::sort(list.begin(), list.end(), [&](const QJsonValue &a, const QJsonValue &b) {
        if (!a.isObject() || !b.isObject()) return false; // 非对象元素排在前面（保持相对顺序不确定）
        QJsonValue va = a.toObject().value(field);
        QJsonValue vb = b.toObject().value(field);
        if (ascending)
            return va.toVariant() < vb.toVariant();
        else
            return va.toVariant() > vb.toVariant();
    });

    arr = QJsonArray();
    for (const auto& v : list) {
        arr.append(v);
    }
}

void JsonUtils::arrayRemoveDuplicates(QJsonArray &arr)
{
    QJsonArray out;
    QVariantList seen;
    for (const auto& v : arr) {
        auto var = v.toVariant();
        if (!seen.contains(var)) {
            seen << var;
            out.append(v);
        }
    }
    arr = out;
}

QJsonArray JsonUtils::arrayFilter(const QJsonArray &arr, const QString &field, const QJsonValue& match)
{
    QJsonArray out;
    for (const auto& v : arr) {
        if (v.isObject() && v.toObject().value(field) == match)
            out.append(v);
    }
    return out;
}

// ========================== 20. JSON 转 XML（修复：转义特殊字符、支持数组） ==========================
static QString escapeXml(const QString &text)
{
    QString result = text;
    result.replace("&", "&amp;")
          .replace("<", "&lt;")
          .replace(">", "&gt;")
          .replace("\"", "&quot;")
          .replace("'", "&apos;");
    return result;
}

static void jsonToXmlRecursive(const QJsonValue &val, QString &out, const QString &indent, const QString &tagName)
{
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        out += indent + "<" + tagName + ">\n";
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = it.key();
            QJsonValue v = it.value();
            jsonToXmlRecursive(v, out, indent + "  ", key);
        }
        out += indent + "</" + tagName + ">\n";
    }
    else if (val.isArray()) {
        QJsonArray arr = val.toArray();
        for (const auto& item : arr) {
            jsonToXmlRecursive(item, out, indent, tagName);
        }
    }
    else {
        // 基本类型：字符串、数字、布尔
        QString text;
        if (val.isString()) text = escapeXml(val.toString());
        else if (val.isBool()) text = val.toBool() ? "true" : "false";
        else if (val.isDouble()) text = QString::number(val.toDouble());
        else if (val.isNull()) text = "";
        else text = escapeXml(val.toVariant().toString());

        out += indent + "<" + tagName + ">" + text + "</" + tagName + ">\n";
    }
}

QString JsonUtils::jsonToXml(const QJsonObject &obj, const QString &rootName)
{
    if (obj.isEmpty()) return "";
    QString xml;
    jsonToXmlRecursive(obj, xml, "", rootName);
    return xml.trimmed();
}

// ========================== 21. URL 参数（修复：编码/解码） ==========================
QString JsonUtils::toUrlParams(const QJsonObject &obj)
{
    QStringList parts;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = QUrl::toPercentEncoding(it.key());
        QString value = QUrl::toPercentEncoding(it.value().toString());
        parts << key + "=" + value;
    }
    return parts.join("&");
}

QJsonObject JsonUtils::fromUrlParams(const QString &urlParams)
{
    QJsonObject out;
    QStringList pairs = urlParams.split("&");
    for (const QString& pair : pairs) {
        int idx = pair.indexOf("=");
        if (idx <= 0) continue;
        QString key = QUrl::fromPercentEncoding(pair.left(idx).toUtf8());
        QString value = QUrl::fromPercentEncoding(pair.mid(idx + 1).toUtf8());
        out[key] = value;
    }
    return out;
}

// ========================== 22. BOM 文件（支持对象和数组） ==========================
QJsonDocument JsonUtils::loadFileWithoutBom(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        setError("读取BOM文件失败：" + f.errorString());
        return QJsonDocument();
    }
    QByteArray data = f.readAll();
    f.close();
    if (data.startsWith("\xEF\xBB\xBF"))
        data = data.mid(3);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        setError("BOM文件JSON解析失败：" + err.errorString());
        return QJsonDocument();
    }
    return doc;
}

bool JsonUtils::writeFileWithBom(const QJsonObject &obj, const QString &filePath, bool format)
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        setError("写入BOM文件失败：" + f.errorString());
        return false;
    }
    QByteArray data = format ? QJsonDocument(obj).toJson(QJsonDocument::Indented)
                             : QJsonDocument(obj).toJson(QJsonDocument::Compact);
    f.write("\xEF\xBB\xBF");
    f.write(data);
    f.close();
    return true;
}

// ========================== 23. 提取叶子值 ==========================
static void getLeafsRecursive(const QJsonObject &obj, QVariantList &out)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QJsonValue v = it.value();
        if (v.isObject()) {
            getLeafsRecursive(v.toObject(), out);
        } else {
            out << v.toVariant();
        }
    }
}

QVariantList JsonUtils::getAllLeafValues(const QJsonObject &obj)
{
    QVariantList out;
    getLeafsRecursive(obj, out);
    return out;
}
}
