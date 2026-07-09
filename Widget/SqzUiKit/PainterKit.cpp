#include "PainterKit.h"

namespace Sqz::Widget
{
    // 构造：保存绘图上下文
    PainterGuard::PainterGuard(QPainter* painter)
        : m_painter(painter)
    {
        if (m_painter)
        {
            m_painter->save();
        }
    }

    // 析构：自动恢复上下文
    PainterGuard::~PainterGuard()
    {
        if (m_painter)
        {
            m_painter->restore();
        }
    }

    // 开启抗锯齿
    PainterGuard& PainterGuard::antialias(bool enable)
    {
        if (m_painter)
            m_painter->setRenderHint(QPainter::Antialiasing, enable);
        return *this;
    }

    // 设置画笔对象
    PainterGuard& PainterGuard::pen(const QPen& pen)
    {
        if (m_painter)
            m_painter->setPen(pen);
        return *this;
    }

    // 修改画笔颜色，保留其他样式
    PainterGuard& PainterGuard::penColor(const QColor& color)
    {
        if (m_painter)
        {
            QPen p = m_painter->pen();
            p.setColor(color);
            m_painter->setPen(p);
        }
        return *this;
    }

    // 修改画笔粗细
    PainterGuard& PainterGuard::penWidth(int px)
    {
        if (m_painter)
        {
            QPen p = m_painter->pen();
            p.setWidth(px);
            m_painter->setPen(p);
        }
        return *this;
    }

    // 设置画刷对象
    PainterGuard& PainterGuard::brush(const QBrush& brush)
    {
        if (m_painter)
            m_painter->setBrush(brush);
        return *this;
    }

    // 修改画刷颜色，保留其他样式
    PainterGuard& PainterGuard::brushColor(const QColor& color)
    {
        if (m_painter)
        {
            QBrush b = m_painter->brush();
            b.setColor(color);
            m_painter->setBrush(b);
        }
        return *this;
    }

    // 清空画笔，无边框
    PainterGuard& PainterGuard::noPen()
    {
        if (m_painter)
            m_painter->setPen(Qt::NoPen);
        return *this;
    }

    // 清空画刷，无填充
    PainterGuard& PainterGuard::noBrush()
    {
        if (m_painter)
            m_painter->setBrush(Qt::NoBrush);
        return *this;
    }

    // 设置矩形裁剪范围
    PainterGuard& PainterGuard::clipRect(const QRectF& rect)
    {
        if (m_painter)
            m_painter->setClipRect(rect);
        return *this;
    }

    // 设置路径裁剪范围
    PainterGuard& PainterGuard::clipPath(const QPainterPath& path)
    {
        if (m_painter)
            m_painter->setClipPath(path);
        return *this;
    }

    // 绘制统一圆角矩形
    void drawRoundRect(QPainter* painter, const QRectF& rect, int radius)
    {
        if (!painter) return;
        painter->drawRoundedRect(rect, radius, radius);
    }

    // 绘制四角独立圆角矩形
    void drawPartialRoundRect(QPainter* painter, const QRectF& rect,
        int rTopLeft, int rTopRight, int rBottomLeft, int rBottomRight)
    {
        if (!painter) return;

        QPainterPath path;
        path.moveTo(rect.topLeft() + QPointF(rTopLeft, 0));

        // 上边 + 右上角圆弧
        path.lineTo(rect.topRight() - QPointF(rTopRight, 0));
        path.arcTo(rect.left(), rect.top(), rTopRight * 2, rTopRight * 2, 90, -90);

        // 右边 + 右下角圆弧
        path.lineTo(rect.bottomRight() - QPointF(0, rBottomRight));
        path.arcTo(rect.right() - rBottomRight * 2, rect.bottom() - rBottomRight * 2,
            rBottomRight * 2, rBottomRight * 2, 0, -90);

        // 下边 + 左下角圆弧
        path.lineTo(rect.bottomLeft() + QPointF(0, rBottomLeft));
        path.arcTo(rect.left(), rect.bottom() - rBottomLeft * 2,
            rBottomLeft * 2, rBottomLeft * 2, 270, -90);

        // 左边 + 左上角圆弧
        path.lineTo(rect.topLeft() + QPointF(0, rTopLeft));
        path.arcTo(rect.left(), rect.top(), rTopLeft * 2, rTopLeft * 2, 180, -90);

        path.closeSubpath();
        painter->drawPath(path);
    }

    // 水平渐变填充矩形
    void drawHGradientRect(QPainter* painter, const QRectF& rect,
        const QColor& startColor, const QColor& endColor)
    {
        if (!painter) return;
        QLinearGradient grad(rect.left(), 0, rect.right(), 0);
        grad.setColorAt(0, startColor);
        grad.setColorAt(1, endColor);
        painter->fillRect(rect, grad);
    }

    // 矩形内居中文字
    void drawCenterText(QPainter* painter, const QRectF& rect, const QString& text)
    {
        if (!painter) return;
        painter->drawText(rect, Qt::AlignCenter, text);
    }

    // 虚线矩形边框
    void drawDashedBorder(QPainter* painter, const QRectF& rect, int dashLen)
    {
        if (!painter) return;
        QPen pen = painter->pen();
        pen.setDashPattern({(qreal)dashLen, (qreal)dashLen});
        painter->save();
        painter->setPen(pen);
        painter->drawRect(rect);
        painter->restore();
    }
}
