#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMap>
#include "SqzGlobal.h"
/**
 * @brief 字符串工具类（完整修复版，Qt 5.12.9 跨平台）
 * @details
 * - 空值处理、补齐格式化、分割合并、替换、大小写转换
 * - 编码转换、隐私脱敏、版本比较、正则校验、内容提取
 * - 格式转换、URL 转义、随机字符串、相似度计算（基于编辑距离）
 * - 所有静态方法，线程安全，无外部依赖
 * @note 纯静态类，禁止实例化
 */

namespace Sqz {
class SQZ_FRAMEWORK_API StringUtils
{
public:
    StringUtils() = delete;

    // ============================================================
    // 1. 空值与空白处理
    // ============================================================
    static bool isEmpty(const QString &str);
    static bool isBlank(const QString &str);
    static QString safeStr(const QString &str, const QString &defaultVal = "");
    static QString trim(const QString &str);
    static QString trimAll(const QString &str);   // 删除所有空白字符

    // ============================================================
    // 2. 补齐格式化
    // ============================================================
    static QString zeroFill(int num, int len);
    static QString zeroFillStr(const QString &numStr, int len);
    static QString padLeft(const QString &str, int len, QChar ch = ' ');
    static QString padRight(const QString &str, int len, QChar ch = ' ');
    static QString padCenter(const QString &str, int len, QChar ch = ' ');

    // ============================================================
    // 3. 切割、合并、去重
    // ============================================================
    static QStringList split(const QString &str, const QString &sep, bool skipEmpty = true);
    static QStringList splitLines(const QString &str, bool skipEmpty = true);
    static QString join(const QStringList &list, const QString &sep);
    static QStringList removeEmpty(const QStringList &list);
    static QStringList removeDuplicate(const QStringList &list);

    // ============================================================
    // 4. 替换（普通 / 批量 / 正则）
    // ============================================================
    static QString replace(const QString &str, const QString &from, const QString &to);
    static QString replaceMap(const QString &str, const QMap<QString, QString> &map);
    static QString regexReplace(const QString &str, const QString &pattern, const QString &to);

    // ============================================================
    // 5. 大小写、命名格式转换
    // ============================================================
    static QString upper(const QString &str);
    static QString lower(const QString &str);
    static QString firstUpper(const QString &str);
    static QString camelToUnderline(const QString &camel);
    static QString underlineToCamel(const QString &line);

    // ============================================================
    // 6. 编码转换（安全处理）
    // ============================================================
    static QString utf8ToGbk(const QString &utf8);
    static QString gbkToUtf8(const QString &gbk);
    static QByteArray toUtf8Bytes(const QString &str);
    static QString fromUtf8Bytes(const QByteArray &bytes);

    // ============================================================
    // 7. 隐私脱敏
    // ============================================================
    static QString maskPhone(const QString &phone);
    static QString maskIdCard(const QString &idCard);
    static QString maskBankCard(const QString &card);
    static QString maskEmail(const QString &email);
    static QString maskMiddle(const QString &str, int keepLeft, int keepRight, const QString &mask = "****");

    // ============================================================
    // 8. 版本号比较
    // ============================================================
    static int compareVersion(const QString &v1, const QString &v2);
    static bool versionGreater(const QString &v1, const QString &v2);
    static bool versionEqual(const QString &v1, const QString &v2);

    // ============================================================
    // 9. 格式校验（正则）
    // ============================================================
    static bool isNumber(const QString &str);
    static bool isLetter(const QString &str);
    static bool isLetterNum(const QString &str);
    static bool isPhone(const QString &str);
    static bool isEmail(const QString &str);
    static bool isStrongPassword(const QString &str, int minLen = 8);

    // ============================================================
    // 10. 内容提取
    // ============================================================
    static QString extractNumber(const QString &str);
    static QString extractChinese(const QString &str);
    static QString extractEnglish(const QString &str);
    static QStringList extractPhones(const QString &str);
    static QStringList extractEmails(const QString &str);

    // ============================================================
    // 11. 格式转换
    // ============================================================
    static QString toThousandSep(double num);
    static QString ipIntToStr(quint32 ip);
    static quint32 ipStrToInt(const QString &ip);
    static QString strToHex(const QString &str);
    static QString hexToStr(const QString &hex);

    // ============================================================
    // 12. URL 转义 / 反转义
    // ============================================================
    static QString urlEncode(const QString &str);
    static QString urlDecode(const QString &str);

    // ============================================================
    // 13. 随机字符串（使用 QRandomGenerator）
    // ============================================================
    static QString randomStr(int len, bool num = true, bool lower = true, bool upper = true);
    static QString randomCode(int len);

    // ============================================================
    // 14. 首尾 / 包含判断
    // ============================================================
    static bool startsWith(const QString &str, const QString &prefix, bool ignoreCase = false);
    static bool endsWith(const QString &str, const QString &suffix, bool ignoreCase = false);
    static bool contains(const QString &str, const QString &sub, bool ignoreCase = false);

    // ============================================================
    // 15. 字符串相似度（基于莱文斯坦距离，0~100）
    // ============================================================
    static int similarity(const QString &a, const QString &b);
};
}
#endif // STRINGUTILS_H
