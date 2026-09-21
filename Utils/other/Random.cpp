#include "Random.h"

/**
 * @brief 获取全局随机数生成器
 *
 * Qt 推荐使用 QRandomGenerator::global()
 */
namespace Sqz {
QRandomGenerator *Random::rng()
{
    return QRandomGenerator::global();
}

int Random::intRange(int min, int max)
{
    if (min > max)
        std::swap(min, max);

    // Qt: bounded(max+1) -> [0, max]
    return rng()->bounded(min, max + 1);
}

double Random::doubleRange(double min, double max)
{
    if (min > max)
        std::swap(min, max);

    return min + (max - min) * rng()->generateDouble();
}

bool Random::boolean()
{
    return rng()->bounded(2) == 1;
}

QString Random::string(int length, const QString &charset)
{
    if (length <= 0 || charset.isEmpty())
        return "";

    QString result;
    int maxIndex = charset.length() - 1;

    for (int i = 0; i < length; ++i)
    {
        int index = intRange(0, maxIndex);
        result.append(charset[index]);
    }

    return result;
}

QString Random::uuid()
{
    // 去掉大括号
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QColor Random::color(bool withAlpha)
{
    int r = intRange(0, 255);
    int g = intRange(0, 255);
    int b = intRange(0, 255);

    if (withAlpha)
    {
        int a = intRange(0, 255);
        return QColor(r, g, b, a);
    }

    return QColor(r, g, b);
}

QString Random::pickString(const QStringList &list)
{
    if (list.isEmpty())
        return "";

    int index = intRange(0, list.size() - 1);
    return list.at(index);
}

QString Random::randomName()
{
    // 简单词库（你可以扩展）
    static QStringList adjectives = {
        "Cool", "Fast", "Silent", "Crazy", "Happy", "Lazy"
    };

    static QStringList nouns = {
        "Cat", "Dog", "Tiger", "Eagle", "Fox", "Panda"
    };

    QString name = pickString(adjectives)
                 + pickString(nouns)
                 + QString::number(intRange(10, 999));

    return name;
}
}
