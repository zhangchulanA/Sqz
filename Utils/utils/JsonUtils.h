#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QFile>
#include <QDebug>
#include <QVariant>
#include <QStringList>
#include <QMutex>
#include "SqzGlobal.h"
/**
 * @brief 完整修复版 JSON 工具类（Qt 5.12.9 跨平台）
 * @details
 * - 文件读写、字符串互转、安全取值、深度操作、数组高级、合并比较
 * - 扁平化/反扁平化、空值清理、键大小写转换、XML/URL 互转
 * - BOM 文件支持、错误管理（线程安全）、深拷贝、类型校验
 * - 已修复：深度赋值/删除失效、XML 数组丢失、URL 未编码、全局错误非线程安全等
 * @note 纯静态类，禁止实例化
 */

namespace Sqz {
class SQZ_FRAMEWORK_API JsonUtils
{
public:
    JsonUtils() = delete;

    // ========================== 1. JSON 文件操作 ==========================
    static QJsonDocument loadFileToDoc(const QString& filePath);
    static QJsonObject loadFileToObject(const QString& filePath);
    static QJsonArray loadFileToArray(const QString& filePath);
    static bool writeDocToFile(const QJsonDocument& doc, const QString& filePath, bool format = true);
    static bool writeObjectToFile(const QJsonObject& obj, const QString& filePath, bool format = true);
    static bool writeArrayToFile(const QJsonArray& arr, const QString& filePath, bool format = true);

    // ========================== 2. JSON 字符串互转 ==========================
    static QString objectToString(const QJsonObject& obj, bool format = true);
    static QString arrayToString(const QJsonArray& arr, bool format = true);
    static QJsonObject stringToObject(const QString& jsonStr);
    static QJsonArray stringToArray(const QString& jsonStr);

    // ========================== 3. 安全取值 ==========================
    static int getInt(const QJsonObject& obj, const QString& key, int def = 0);
    static double getDouble(const QJsonObject& obj, const QString& key, double def = 0.0);
    static QString getString(const QJsonObject& obj, const QString& key, const QString& def = "");
    static bool getBool(const QJsonObject& obj, const QString& key, bool def = false);
    static QJsonObject getObject(const QJsonObject& obj, const QString& key);
    static QJsonArray getArray(const QJsonObject& obj, const QString& key);
    static QJsonValue getValue(const QJsonObject& obj, const QString& key);

    // ========================== 4. 深度取值 ==========================
    static QJsonValue getDeepValue(const QJsonObject& rootObj, const QString& keyPath, const QJsonValue& def = QJsonValue());
    static QString getDeepString(const QJsonObject& rootObj, const QString& keyPath, const QString& def = "");
    static int getDeepInt(const QJsonObject& rootObj, const QString& keyPath, int def = 0);

    // ========================== 5. 安全赋值 & 深度赋值 ==========================
    static void setValue(QJsonObject& obj, const QString& key, const QJsonValue& value);
    static void setDeepValue(QJsonObject& rootObj, const QString& keyPath, const QJsonValue& value);

    // ========================== 6. 键操作 ==========================
    static bool contains(const QJsonObject& obj, const QString& key);
    static bool containsDeep(const QJsonObject& rootObj, const QString& keyPath);
    static void removeKey(QJsonObject& obj, const QString& key);
    static void removeDeepKey(QJsonObject& rootObj, const QString& keyPath);
    static QStringList getAllKeys(const QJsonObject& obj);
    static void renameKey(QJsonObject& obj, const QString& oldKey, const QString& newKey);

    // ========================== 7. 数组基础操作 ==========================
    static void arrayAppend(QJsonArray& arr, const QJsonValue& value);
    static void arrayInsert(QJsonArray& arr, int index, const QJsonValue& value);
    static void arrayRemoveAt(QJsonArray& arr, int index);
    static void arrayReplace(QJsonArray& arr, int index, const QJsonValue& newValue);
    static QJsonValue arrayGet(const QJsonArray& arr, int index, const QJsonValue& def = QJsonValue());
    static int arrayFind(const QJsonArray& arr, const QJsonValue& value);

    // ========================== 8. 合并 & 过滤 ==========================
    static QJsonObject mergeObject(const QJsonObject& target, const QJsonObject& source, bool overwrite = true);
    static QJsonObject filterKeys(const QJsonObject& obj, const QStringList& keepKeys);

    // ========================== 9. 类型校验 ==========================
    static bool isInt(const QJsonValue& val);
    static bool isString(const QJsonValue& val);
    static bool isObject(const QJsonValue& val);
    static bool isArray(const QJsonValue& val);
    static bool isNull(const QJsonValue& val);
    static bool isObjectEmpty(const QJsonObject& obj);
    static bool isArrayEmpty(const QJsonArray& arr);

    // ========================== 10. QVariant 互转 ==========================
    static QJsonValue variantToJson(const QVariant& var);
    static QVariant jsonToVariant(const QJsonValue& val);
    static QJsonObject variantMapToObject(const QVariantMap& map);
    static QVariantMap objectToVariantMap(const QJsonObject& obj);

    // ========================== 11. 格式化 ==========================
    static QString formatJsonString(const QString& jsonStr);
    static QString compactJsonString(const QString& jsonStr);

    // ========================== 12. 深拷贝 ==========================
    static QJsonObject copyObject(const QJsonObject& obj);
    static QJsonArray copyArray(const QJsonArray& arr);

    // ========================== 13. 错误信息（线程安全） ==========================
    static QString getLastError();

    // ========================== 14. 深度递归合并 ==========================
    static QJsonObject mergeDeep(const QJsonObject &target, const QJsonObject &source, bool overwrite = true);

    // ========================== 15. 深度比较 ==========================
    static bool isEqual(const QJsonValue &a, const QJsonValue &b);
    static bool isObjectEqual(const QJsonObject &a, const QJsonObject &b);
    static bool isArrayEqual(const QJsonArray &a, const QJsonArray &b);

    // ========================== 16. 扁平化/反扁平化 ==========================
    static QJsonObject flatten(const QJsonObject &obj);
    static QJsonObject unflatten(const QJsonObject &flatObj);

    // ========================== 17. 清理空值 ==========================
    static QJsonObject cleanEmptyValues(const QJsonObject &obj);

    // ========================== 18. 键大小写转换 ==========================
    static QJsonObject keysToLower(const QJsonObject &obj);
    static QJsonObject keysToUpper(const QJsonObject &obj);

    // ========================== 19. 数组高级 ==========================
    static void arraySort(QJsonArray &arr, bool ascending = true);
    static void arraySortByField(QJsonArray &arr, const QString &field, bool ascending = true);
    static void arrayRemoveDuplicates(QJsonArray &arr);
    static QJsonArray arrayFilter(const QJsonArray &arr, const QString &field, const QJsonValue& matchValue);

    // ========================== 20. JSON 转 XML ==========================
    static QString jsonToXml(const QJsonObject &obj, const QString &rootName = "root");

    // ========================== 21. JSON <-> URL 参数 ==========================
    static QString toUrlParams(const QJsonObject &obj);
    static QJsonObject fromUrlParams(const QString &urlParams);

    // ========================== 22. UTF-8 BOM 文件读写 ==========================
    static QJsonDocument loadFileWithoutBom(const QString &filePath);
    static bool writeFileWithBom(const QJsonObject &obj, const QString &filePath, bool format = true);

    // ========================== 23. 提取叶子值 ==========================
    static QVariantList getAllLeafValues(const QJsonObject &obj);

private:
    static void setError(const QString& err);
    static QString m_lastError;
    static QMutex m_errorMutex;   // 保护 m_lastError 的互斥锁
};
}
#endif // JSONUTILS_H
