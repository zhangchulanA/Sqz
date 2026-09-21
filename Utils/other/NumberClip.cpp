#include "NumberClip.h"
#include <cmath>
namespace Sqz {
int NumberClip::limit(int val, int min, int max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

qint64 NumberClip::limit(qint64 val, qint64 min, qint64 max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

double NumberClip::limit(double val, double min, double max, int fixDigit)
{
    double res = val;
    if (res < min)
        res = min;
    if (res > max)
        res = max;

    if (fixDigit >= 0)
    {
        double powNum = std::pow(10.0, fixDigit);
        res = std::round(res * powNum) / powNum;
    }
    return res;
}
}
