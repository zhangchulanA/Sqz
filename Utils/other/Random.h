#ifndef RANDOM_H
#define RANDOM_H

#include <QObject>
#include <QRandomGenerator>
#include <QColor>
#include <QString>
#include <QVector>
#include <QUuid>
#include "SqzGlobal.h"
/**
 * @brief Random 工具类
 *
 * 提供常用随机数据生成能力：
 * - 数字
 * - 字符串
 * - 颜色
 * - UUID
 * - 容器随机
 *
 * 所有方法均为静态方法，直接使用即可
 */
namespace Sqz {
class SQZ_FRAMEWORK_API Random
{
public:
    /**
     * @brief 生成随机整数 [min, max]
     */
    static int intRange(int min, int max);

    /**
     * @brief 生成随机浮点数 [min, max)
     */
    static double doubleRange(double min, double max);

    /**
     * @brief 生成随机布尔值
     */
    static bool boolean();

    /**
     * @brief 生成随机字符串
     * @param length 长度
     * @param charset 可选字符集（默认字母+数字）
     */
    static QString string(int length,
                          const QString &charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    /**
     * @brief 生成随机UUID（无花括号）
     */
    static QString uuid();

    /**
     * @brief 生成随机颜色
     * @param withAlpha 是否包含透明度
     */
    static QColor color(bool withAlpha = false);

    /**
     * @brief 从 QVector 中随机取一个元素
     */
    template<typename T>
    static T pick(const QVector<T> &list)
    {
        if (list.isEmpty())
            return T();

        int index = intRange(0, list.size() - 1);
        return list[index];
    }

    /**
     * @brief 从 QStringList 中随机取一个字符串
     */
    static QString pickString(const QStringList &list);

    /**
     * @brief 生成随机“用户名”（好玩）
     */
    static QString randomName();

private:
    static QRandomGenerator *rng();
};
}
#endif // RANDOM_H
