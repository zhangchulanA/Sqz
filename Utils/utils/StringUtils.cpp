#include "StringUtils.h"
#include <QTextCodec>
#include <QRegExp>
#include <QUrl>
#include <QSet>
#include <QLocale>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QVector>
#include <QDebug>

// =============================
// 1. 空值与空白处理
// =============================

namespace Sqz::Utils {
bool StringUtils::isEmpty(const QString &str)
{
    return str.isEmpty();
}

bool StringUtils::isBlank(const QString &str)
{
    return str.trimmed().isEmpty();
}

QString StringUtils::safeStr(const QString &str, const QString &defaultVal)
{
    return isBlank(str) ? defaultVal : str;
}

QString StringUtils::trim(const QString &str)
{
    return str.trimmed();
}

// 删除所有空白字符（空格、制表符、换行等）
QString StringUtils::trimAll(const QString &str)
{
    QString result;
    result.reserve(str.size());
    for (QChar ch : str) {
        if (!ch.isSpace()) {
            result.append(ch);
        }
    }
    return result;
}

// =============================
// 2. 补齐格式化
// =============================

QString StringUtils::zeroFill(int num, int len)
{
    return QString("%1").arg(num, len, 10, QChar('0'));
}

QString StringUtils::zeroFillStr(const QString &numStr, int len)
{
    return padLeft(numStr, len, '0');
}

QString StringUtils::padLeft(const QString &str, int len, QChar ch)
{
    return str.rightJustified(len, ch);
}

QString StringUtils::padRight(const QString &str, int len, QChar ch)
{
    return str.leftJustified(len, ch);
}

QString StringUtils::padCenter(const QString &str, int len, QChar ch)
{
    int left = (len - str.length()) / 2;
    int right = len - str.length() - left;
    return QString(left, ch) + str + QString(right, ch);
}

// =============================
// 3. 切割合并去重
// =============================

QStringList StringUtils::split(const QString &str, const QString &sep, bool skipEmpty)
{
    return str.split(sep, skipEmpty ? QString::SkipEmptyParts : QString::KeepEmptyParts);
}

QStringList StringUtils::splitLines(const QString &str, bool skipEmpty)
{
    return str.split(QRegExp("[\r\n]"), skipEmpty ? QString::SkipEmptyParts : QString::KeepEmptyParts);
}

QString StringUtils::join(const QStringList &list, const QString &sep)
{
    return list.join(sep);
}

QStringList StringUtils::removeEmpty(const QStringList &list)
{
    QStringList res;
    for (const QString &s : list) {
        if (!isBlank(s)) res << s;
    }
    return res;
}

QStringList StringUtils::removeDuplicate(const QStringList &list)
{
    QStringList res;
    QSet<QString> set;
    for (const QString &s : list) {
        if (!set.contains(s)) {
            set.insert(s);
            res << s;
        }
    }
    return res;
}

// =============================
// 4. 替换
// =============================

QString StringUtils::replace(const QString &str, const QString &from, const QString &to)
{
    QString s = str;
    return s.replace(from, to);
}

QString StringUtils::replaceMap(const QString &str, const QMap<QString, QString> &map)
{
    QString s = str;
    for (auto it = map.begin(); it != map.end(); ++it) {
        s.replace(it.key(), it.value());
    }
    return s;
}

QString StringUtils::regexReplace(const QString &str, const QString &pattern, const QString &to)
{
    QString s = str;
    return s.replace(QRegExp(pattern), to);
}

// =============================
// 5. 大小写与命名格式
// =============================

QString StringUtils::upper(const QString &str) { return str.toUpper(); }
QString StringUtils::lower(const QString &str) { return str.toLower(); }

QString StringUtils::firstUpper(const QString &str)
{
    if (str.isEmpty()) return str;
    return str.left(1).toUpper() + str.mid(1);
}

// 改进版：处理连续大写字母（如 XMLHttpRequest -> xml_http_request）
QString StringUtils::camelToUnderline(const QString &camel)
{
    QString result;
    for (int i = 0; i < camel.size(); ++i) {
        QChar ch = camel[i];
        if (ch.isUpper()) {
            if (i > 0) {
                // 如果前一个字符是小写或当前是大写但下一个是小写，需要加下划线
                QChar prev = camel[i-1];
                if (prev.isLower() || (i+1 < camel.size() && camel[i+1].isLower())) {
                    result += '_';
                }
            }
            result += ch.toLower();
        } else {
            result += ch;
        }
    }
    return result;
}

// 改进版：处理前导下划线、连续下划线
QString StringUtils::underlineToCamel(const QString &line)
{
    QString result;
    bool capNext = false;
    for (int i = 0; i < line.size(); ++i) {
        QChar ch = line[i];
        if (ch == '_') {
            capNext = true;
            // 连续下划线：跳过，但保持下一个字符大写
            continue;
        }
        if (capNext) {
            result += ch.toUpper();
            capNext = false;
        } else {
            result += ch;
        }
    }
    // 如果原字符串以 '_' 开头，第一个字符已经丢失，这里无需额外处理
    return result;
}

// =============================
// 6. 编码转换（安全处理）
// =============================

QString StringUtils::utf8ToGbk(const QString &utf8)
{
    QTextCodec *codec = QTextCodec::codecForName("GBK");
    if (!codec) {
        qWarning() << "StringUtils::utf8ToGbk: GBK codec not found, return original";
        return utf8;
    }
    return codec->toUnicode(utf8.toUtf8());
}

QString StringUtils::gbkToUtf8(const QString &gbk)
{
    QTextCodec *codec = QTextCodec::codecForName("GBK");
    if (!codec) {
        qWarning() << "StringUtils::gbkToUtf8: GBK codec not found, return original";
        return gbk;
    }
    return codec->fromUnicode(gbk);
}

QByteArray StringUtils::toUtf8Bytes(const QString &str)
{
    return str.toUtf8();
}

QString StringUtils::fromUtf8Bytes(const QByteArray &bytes)
{
    return QString::fromUtf8(bytes);
}

// =============================
// 7. 隐私脱敏
// =============================

QString StringUtils::maskPhone(const QString &phone)
{
    return phone.length() == 11 ? phone.left(3) + "****" + phone.right(4) : phone;
}

QString StringUtils::maskIdCard(const QString &idCard)
{
    return idCard.length() >= 10 ? idCard.left(6) + "********" + idCard.right(4) : idCard;
}

QString StringUtils::maskBankCard(const QString &card)
{
    return card.length() > 8 ? card.left(4) + " **** **** " + card.right(4) : card;
}

QString StringUtils::maskEmail(const QString &email)
{
    int at = email.indexOf('@');
    if (at > 1) {
        return email.left(1) + "****" + email.mid(at);
    }
    return email;
}

QString StringUtils::maskMiddle(const QString &str, int keepLeft, int keepRight, const QString &mask)
{
    if (str.length() <= keepLeft + keepRight) return str;
    return str.left(keepLeft) + mask + str.right(keepRight);
}

// =============================
// 8. 版本比较
// =============================

int StringUtils::compareVersion(const QString &v1, const QString &v2)
{
    QStringList l1 = split(v1, ".");
    QStringList l2 = split(v2, ".");
    int n = qMax(l1.size(), l2.size());
    for (int i = 0; i < n; ++i) {
        int a = i < l1.size() ? l1[i].toInt() : 0;
        int b = i < l2.size() ? l2[i].toInt() : 0;
        if (a > b) return 1;
        if (a < b) return -1;
    }
    return 0;
}

bool StringUtils::versionGreater(const QString &v1, const QString &v2)
{
    return compareVersion(v1, v2) > 0;
}

bool StringUtils::versionEqual(const QString &v1, const QString &v2)
{
    return compareVersion(v1, v2) == 0;
}

// =============================
// 9. 格式校验（使用静态正则减少编译开销）
// =============================

static QRegExp numberRx("^\\d+$");
static QRegExp letterRx("^[a-zA-Z]+$");
static QRegExp letterNumRx("^[a-zA-Z0-9]+$");
static QRegExp phoneRx("^1[3-9]\\d{9}$");
static QRegExp emailRx("^\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*\\.\\w+$");

bool StringUtils::isNumber(const QString &str) { return numberRx.exactMatch(str); }
bool StringUtils::isLetter(const QString &str) { return letterRx.exactMatch(str); }
bool StringUtils::isLetterNum(const QString &str) { return letterNumRx.exactMatch(str); }
bool StringUtils::isPhone(const QString &str) { return phoneRx.exactMatch(str); }
bool StringUtils::isEmail(const QString &str) { return emailRx.exactMatch(str); }

bool StringUtils::isStrongPassword(const QString &str, int minLen)
{
    if (str.length() < minLen) return false;
    bool hasDigit = false, hasLetter = false, hasSpecial = false;
    for (QChar ch : str) {
        if (ch.isDigit()) hasDigit = true;
        else if (ch.isLetter()) hasLetter = true;
        else if (!ch.isSpace()) hasSpecial = true; // 非空格且非数字字母算特殊字符
    }
    return hasDigit && hasLetter && hasSpecial;
}

// =============================
// 10. 内容提取
// =============================

QString StringUtils::extractNumber(const QString &str)
{
    QString result;
    QRegExp rx("[^0-9]");
    result = str;
    return result.replace(rx, "");
}

QString StringUtils::extractChinese(const QString &str)
{
    QString result;
    QRegExp rx("[^\\u4e00-\\u9fa5]");
    result = str;
    return result.replace(rx, "");
}

QString StringUtils::extractEnglish(const QString &str)
{
    QString result;
    QRegExp rx("[^a-zA-Z]");
    result = str;
    return result.replace(rx, "");
}

QStringList StringUtils::extractPhones(const QString &str)
{
    QStringList res;
    QRegExp rx("1[3-9]\\d{9}");
    int pos = 0;
    while ((pos = rx.indexIn(str, pos)) != -1) {
        res << rx.cap(0);
        pos += rx.matchedLength();
    }
    return res;
}

QStringList StringUtils::extractEmails(const QString &str)
{
    QStringList res;
    QRegExp rx("\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*\\.\\w+");
    int pos = 0;
    while ((pos = rx.indexIn(str, pos)) != -1) {
        res << rx.cap(0);
        pos += rx.matchedLength();
    }
    return res;
}

// =============================
// 11. 格式转换
// =============================

QString StringUtils::toThousandSep(double num)
{
    return QLocale::system().toString(num, 'f', 2);
}

QString StringUtils::ipIntToStr(quint32 ip)
{
    return QString("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xFF)
        .arg((ip >> 16) & 0xFF)
        .arg((ip >> 8) & 0xFF)
        .arg(ip & 0xFF);
}

quint32 StringUtils::ipStrToInt(const QString &ip)
{
    QHostAddress addr(ip);
    return addr.toIPv4Address();
}

QString StringUtils::strToHex(const QString &str)
{
    return str.toUtf8().toHex();
}

QString StringUtils::hexToStr(const QString &hex)
{
    return QByteArray::fromHex(hex.toUtf8());
}

// =============================
// 12. URL 转义
// =============================

QString StringUtils::urlEncode(const QString &str)
{
    return QUrl::toPercentEncoding(str);
}

QString StringUtils::urlDecode(const QString &str)
{
    return QUrl::fromPercentEncoding(str.toUtf8());
}

// =============================
// 13. 随机字符串（使用 QRandomGenerator）
// =============================

QString StringUtils::randomStr(int len, bool num, bool lower, bool upper)
{
    const QString digits = "0123456789";
    const QString lowers = "abcdefghijklmnopqrstuvwxyz";
    const QString uppers = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QString pool;
    if (num) pool += digits;
    if (lower) pool += lowers;
    if (upper) pool += uppers;
    if (pool.isEmpty()) return "";

    QRandomGenerator *gen = QRandomGenerator::global();
    QString result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
        result.append(pool.at(gen->bounded(pool.size())));
    }
    return result;
}

QString StringUtils::randomCode(int len)
{
    QRandomGenerator *gen = QRandomGenerator::global();
    QString result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
        result.append(QChar('0' + gen->bounded(10)));
    }
    return result;
}

// =============================
// 14. 首尾包含判断
// =============================

bool StringUtils::startsWith(const QString &str, const QString &prefix, bool ignoreCase)
{
    return ignoreCase ? str.startsWith(prefix, Qt::CaseInsensitive) : str.startsWith(prefix);
}

bool StringUtils::endsWith(const QString &str, const QString &suffix, bool ignoreCase)
{
    return ignoreCase ? str.endsWith(suffix, Qt::CaseInsensitive) : str.endsWith(suffix);
}

bool StringUtils::contains(const QString &str, const QString &sub, bool ignoreCase)
{
    return ignoreCase ? str.contains(sub, Qt::CaseInsensitive) : str.contains(sub);
}

// =============================
// 15. 字符串相似度（莱文斯坦距离）
// =============================

int StringUtils::similarity(const QString &a, const QString &b)
{
    int n = a.size();
    int m = b.size();
    if (n == 0 && m == 0) return 100;
    if (n == 0 || m == 0) return 0;

    // 使用一维滚动数组优化空间
    QVector<int> dp(m + 1);
    for (int j = 0; j <= m; ++j) dp[j] = j;

    for (int i = 1; i <= n; ++i) {
        int prev = dp[0]; // 左上角值
        dp[0] = i;
        for (int j = 1; j <= m; ++j) {
            int temp = dp[j];
            if (a[i-1] == b[j-1]) {
                dp[j] = prev;
            } else {
                dp[j] = qMin(qMin(dp[j-1], dp[j]), prev) + 1;
            }
            prev = temp;
        }
    }
    int distance = dp[m];
    int maxLen = qMax(n, m);
    int similarityPercent = (maxLen - distance) * 100 / maxLen;
    return qBound(0, similarityPercent, 100);
}
}
