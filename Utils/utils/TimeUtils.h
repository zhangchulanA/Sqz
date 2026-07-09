#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QString>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include "SqzGlobal.h"
/**
 * @brief 时间工具类（完整修复版，Qt 5.12.9 跨平台）
 * @details
 * - 获取当前时间/日期/时间戳（秒/毫秒）
 * - 时间戳 ↔ 日期时间互转（正确处理 0 时间戳）
 * - 多种格式化输出（标准、中文、文件名）
 * - 时间差计算（秒/分/时/天）
 * - 秒/毫秒 → HH:mm:ss / HH:mm:ss.zzz（支持 qint64 避免溢出）
 * - 友好时间显示：刚刚、几分钟前、几小时前
 * - UTC ↔ 本地时间互转
 * - 时间加减（天/小时/月/年，修复整数溢出）
 * - 日期判断：闰年、今天、昨天、同天/同月/同年
 * - 星期获取（中文/英文）
 * - 日期时间合法性校验
 * - 生成唯一时间串（用于 ID/文件名）
 *
 * @note 所有静态方法，线程安全，无需实例化
 */

namespace Sqz::Utils {
class SQZ_FRAMEWORK_API TimeUtils
{
public:
    // ==============================================
    // 1. 获取当前时间（各种格式）
    // ==============================================

    /**
     * @brief 获取当前标准日期时间
     * @return "yyyy-MM-dd HH:mm:ss"
     */
    static QString currentDateTime();

    /**
     * @brief 获取当前日期
     * @return "yyyy-MM-dd"
     */
    static QString currentDate();

    /**
     * @brief 获取当前时间
     * @return "HH:mm:ss"
     */
    static QString currentTime();

    /**
     * @brief 获取适合文件名的日期时间串
     * @return "yyyyMMdd_HHmmss"
     */
    static QString currentDateTimeForFile();

    /**
     * @brief 获取中文格式日期时间
     * @return "yyyy年MM月dd日 HH:mm:ss"
     */
    static QString currentDateTimeChinese();

    // ==============================================
    // 2. 时间戳（秒 / 毫秒）
    // ==============================================

    /**
     * @brief 获取当前秒级时间戳
     */
    static qint64 currentTimestamp();

    /**
     * @brief 获取当前毫秒级时间戳
     */
    static qint64 currentTimestampMs();

    /**
     * @brief 秒级时间戳 → 标准日期时间字符串
     * @param ts 秒时间戳（负数返回空字符串，0 返回 "1970-01-01 00:00:00"）
     */
    static QString timestampToDateTime(qint64 ts);

    /**
     * @brief 毫秒级时间戳 → 标准日期时间字符串
     * @param tsMs 毫秒时间戳（负数返回空字符串，0 返回 "1970-01-01 00:00:00"）
     */
    static QString timestampMsToDateTime(qint64 tsMs);

    /**
     * @brief 标准时间字符串 → 秒级时间戳
     * @param dtStr "yyyy-MM-dd HH:mm:ss"
     * @return 时间戳，解析失败返回 0
     */
    static qint64 dateTimeToTimestamp(const QString &dtStr);

    /**
     * @brief 标准时间字符串 → 毫秒级时间戳
     * @param dtStr "yyyy-MM-dd HH:mm:ss"
     * @return 毫秒时间戳，解析失败返回 0
     */
    static qint64 dateTimeToTimestampMs(const QString &dtStr);

    // ==============================================
    // 3. 自定义格式化
    // ==============================================

    /**
     * @brief 格式化 QDateTime
     * @param dt  时间对象
     * @param fmt 格式字符串，默认 "yyyy-MM-dd HH:mm:ss"
     */
    static QString format(const QDateTime &dt, const QString &fmt = "yyyy-MM-dd HH:mm:ss");

    /**
     * @brief 格式化 QDate
     * @return "yyyy-MM-dd"
     */
    static QString formatDate(const QDate &d);

    /**
     * @brief 格式化 QTime
     * @return "HH:mm:ss"
     */
    static QString formatTime(const QTime &t);

    // ==============================================
    // 4. 时间差计算
    // ==============================================

    /**
     * @brief 计算两个时间相差秒数
     * @param s 开始时间
     * @param e 结束时间
     * @return e - s 的秒数
     */
    static qint64 secsBetween(const QDateTime &s, const QDateTime &e);

    /**
     * @brief 计算相差分钟数
     */
    static qint64 minsBetween(const QDateTime &s, const QDateTime &e);

    /**
     * @brief 计算相差小时数
     */
    static qint64 hoursBetween(const QDateTime &s, const QDateTime &e);

    /**
     * @brief 计算相差天数
     */
    static qint64 daysBetween(const QDateTime &s, const QDateTime &e);

    // ==============================================
    // 5. 秒/毫秒 → 00:00:00 格式（支持 qint64，防止溢出）
    // ==============================================

    /**
     * @brief 秒数 → "HH:mm:ss"
     * @param sec 秒数（qint64，支持大数值）
     */
    static QString secToHMS(qint64 sec);

    /**
     * @brief 秒数 → "HH:mm"
     * @param sec 秒数
     */
    static QString secToHM(qint64 sec);

    /**
     * @brief 毫秒数 → "HH:mm:ss.zzz"
     * @param msec 毫秒数
     */
    static QString msecToHMSzzz(qint64 msec);

    // ==============================================
    // 6. 友好时间显示
    // ==============================================

    /**
     * @brief 秒时间戳 → 友好显示字符串
     * @example "刚刚"、"5分钟前"、"2小时前"、"3天前"、"2025-01-01"
     */
    static QString friendlyTime(qint64 timestamp);

    /**
     * @brief QDateTime → 友好显示字符串
     */
    static QString friendlyTime(const QDateTime &dt);

    // ==============================================
    // 7. UTC ↔ 本地时间
    // ==============================================

    static QDateTime utcToLocal(const QDateTime &utcDt);
    static QDateTime localToUtc(const QDateTime &localDt);

    // ==============================================
    // 8. 时间加减（使用 qint64 防止乘法溢出）
    // ==============================================

    static QDateTime addDays(const QDateTime &dt, int days);
    static QDateTime addHours(const QDateTime &dt, int hours);
    static QDateTime addMonths(const QDateTime &dt, int months);
    static QDateTime addYears(const QDateTime &dt, int years);

    // ==============================================
    // 9. 日期判断
    // ==============================================

    static bool isLeapYear(int year);
    static bool isToday(const QDate &d);
    static bool isYesterday(const QDate &d);
    static bool isSameDay(const QDateTime &a, const QDateTime &b);
    static bool isSameMonth(const QDateTime &a, const QDateTime &b);
    static bool isSameYear(const QDateTime &a, const QDateTime &b);

    // ==============================================
    // 10. 星期获取
    // ==============================================

    /**
     * @brief 获取中文星期名称
     * @return "周一"、"周二" ... "周日"
     */
    static QString weekDayChinese(const QDate &d);

    /**
     * @brief 获取英文星期全称
     * @return "Monday"、"Tuesday" ...
     */
    static QString weekDayEnglish(const QDate &d);

    // ==============================================
    // 11. 合法性校验
    // ==============================================

    /**
     * @brief 校验日期时间字符串是否合法
     * @param str 时间字符串
     * @param fmt 格式，默认 "yyyy-MM-dd HH:mm:ss"
     */
    static bool isValidDateTime(const QString &str, const QString &fmt = "yyyy-MM-dd HH:mm:ss");

    /**
     * @brief 校验日期字符串是否合法
     * @param str 日期字符串
     * @param fmt 格式，默认 "yyyy-MM-dd"
     */
    static bool isValidDate(const QString &str, const QString &fmt = "yyyy-MM-dd");

    // ==============================================
    // 12. 唯一时间串（毫秒精度）
    // ==============================================

    /**
     * @brief 生成全局唯一时间字符串（毫秒级）
     * @return "yyyyMMddHHmmsszzz"
     * @example "20250501120000123"
     */
    static QString uniqueTimeStr();
};
}
#endif // TIMEUTILS_H
