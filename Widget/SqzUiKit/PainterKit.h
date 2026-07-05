#ifndef PAINTERKIT_H
#define PAINTERKIT_H

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QRectF>
#include <QPointer>
#include <QPainterPath>

namespace PainterKit
{
    /**
     * RAII守卫，自动保存&恢复绘图上下文，防止画笔状态污染
     */
    class PainterGuard
    {
    public:
        // 绑定绘图对象，自动保存上下文
        explicit PainterGuard(QPainter* painter);
        // 离开作用域自动还原画笔状态
        ~PainterGuard();

        // 开启/关闭抗锯齿
        PainterGuard& antialias(bool enable);
        // 设置画笔
        PainterGuard& pen(const QPen& pen);
        // 仅修改画笔颜色
        PainterGuard& penColor(const QColor& color);
        // 仅修改画笔宽度
        PainterGuard& penWidth(int px);
        // 设置画刷
        PainterGuard& brush(const QBrush& brush);
        // 仅修改画刷颜色
        PainterGuard& brushColor(const QColor& color);
        // 取消画笔（无边框）
        PainterGuard& noPen();
        // 取消画刷（无填充）
        PainterGuard& noBrush();

        // 设置矩形裁剪区域
        PainterGuard& clipRect(const QRectF& rect);
        // 设置路径裁剪区域
        PainterGuard& clipPath(const QPainterPath& path);

    private:
        QPainter* m_painter = nullptr;;
    };

    // 快速创建临时守卫对象
    inline PainterGuard guard(QPainter* p)
    {
        return PainterGuard(p);
    }

    // 绘制四角统一圆角矩形
    void drawRoundRect(QPainter* painter, const QRectF& rect, int radius);

    // 绘制四角独立圆角矩形，四个角半径可分别控制
    void drawPartialRoundRect(QPainter* painter, const QRectF& rect,
        int rTopLeft, int rTopRight, int rBottomLeft, int rBottomRight);

    // 绘制水平渐变填充矩形
    void drawHGradientRect(QPainter* painter, const QRectF& rect,
        const QColor& startColor, const QColor& endColor);

    // 在矩形内部居中绘制文字
    void drawCenterText(QPainter* painter, const QRectF& rect, const QString& text);

    // 绘制虚线边框矩形
    void drawDashedBorder(QPainter* painter, const QRectF& rect, int dashLen = 4);
}

#endif // PAINTERKIT_H
