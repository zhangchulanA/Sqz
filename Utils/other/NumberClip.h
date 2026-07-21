#ifndef NUMBERCLIP_H
#define NUMBERCLIP_H
#include <QtGlobal>
/**
 * @brief 数值边界裁剪工具
 * 功能：将数值强制限制在 [min, max] 区间
 * 小于最小值返回min，大于最大值返回max，中间原值返回
 * 仅支持 int / qint64 / double 通用数值
 */
namespace Sqz {
class NumberClip
{
public:
    // 普通int整数裁剪
    static int limit(int val, int min, int max);

    // 长整型，用于ID、大数量
    static qint64 limit(qint64 val, qint64 min, qint64 max);

    /**
     * @brief 浮点数裁剪
     * @param fixDigit 保留小数位数，传-1不做四舍五入
     */
    static double limit(double val, double min, double max, int fixDigit = -1);
};
}
#endif // NUMBERCLIP_H
