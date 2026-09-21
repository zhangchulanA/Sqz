#include "NavGlobalView.h"
#include <QPainter>
#include <QDebug>
#include <cmath>

NavGlobalView::NavGlobalView(QWidget *parent)
    : QWidget(parent)
    , m_heading(0.0)          // 初始航向0度
    , m_gpsValid(false)       // 初始GPS无效
    , m_offCourse(false)      // 初始未偏航
    , m_satStatus(0)          // 初始无信号
{
    setMinimumSize(200, 200);
    setStyleSheet("background-color: black;");  // 黑色背景

    // 加载卫星状态图标并缩放到统一大小
    m_satIcons[0] = QPixmap(":/SqzData/img/1.png").scaled(32, 32);
    m_satIcons[1] = QPixmap(":/SqzData/img/123.png").scaled(32, 32);
    m_satIcons[2] = QPixmap(":/SqzData/img/2.png").scaled(32, 32);
    m_satIcons[3] = QPixmap(":/SqzData/img/3.png").scaled(32, 32);
}

void NavGlobalView::setHeading(double deg)
{
    m_heading = deg;   // 更新航向角
    update();          // 触发重绘
}

void NavGlobalView::setGpsValid(bool valid)
{
    m_gpsValid = valid;  // 更新GPS状态
    update();            // 触发重绘
}

void NavGlobalView::setOffCourse(bool off)
{
    m_offCourse = off;   // 更新偏航状态
    update();            // 触发重绘
}

void NavGlobalView::setSatelliteStatus(int status)
{
    m_satStatus = status;  // 更新卫星状态
    update();              // 触发重绘
}

void NavGlobalView::resizeEvent(QResizeEvent *)
{
    const int margin = 15;
    // 计算内容区域，四周留边距
    m_contentRect = rect().marginsRemoved(QMargins(margin, margin, margin, margin));
}

void NavGlobalView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  // 抗锯齿

    QPointF center = m_contentRect.center();
    double radius = qMin(m_contentRect.width(), m_contentRect.height()) / 2.0;

    // 1. 绘制带刻度的外圆（根据航向旋转）
    painter.save();
    painter.translate(center);
    painter.rotate(-m_heading);      // 旋转使刻度对准航向
    drawOuterCircle(painter, QPointF(0,0), radius);
    painter.restore();

    // 2. 绘制绿色固定直线（从中心向上延伸）
    painter.save();
    painter.translate(center);
    QPen penGreen(Qt::green);
    penGreen.setWidth(4);
    painter.setPen(penGreen);
    painter.drawLine(QPointF(0, 0), QPointF(0, -radius * 0.95));  // 正北指示线
    painter.restore();

    // 3. 绘制车体轮廓（褐色线框）
    drawVehicle(painter, center, radius);

    // 4. 绘制指示灯（GPS和偏航报警）
    drawIndicators(painter, center, radius);

    // 5. 绘制卫星状态图标
    drawSatelliteStatus(painter, center, radius);
}

void NavGlobalView::drawOuterCircle(QPainter &painter, const QPointF &center, double radius)
{
    Q_UNUSED(center);  // 已在外部进行translate

    // 蓝色外圆边框
    QPen penBlue(Qt::blue);
    penBlue.setWidth(4);
    painter.setPen(penBlue);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(0,0), radius, radius);

    // 褐色内圈
    QPen penBrown(QColor(139, 69, 19));
    penBrown.setWidth(3);
    painter.setPen(penBrown);
    double innerRadius = radius * 0.65;
    painter.drawEllipse(QPointF(0,0), innerRadius, innerRadius);

    // 刻度线：每10度短刻度，每30度长刻度
    QPen penTick(Qt::blue);
    penTick.setWidth(2);
    painter.setPen(penTick);
    for (int deg = 0; deg < 360; deg += 10) {
        double rad = deg * M_PI / 180.0;
        double inner = (deg % 30 == 0) ? radius * 0.78 : radius * 0.88;
        double outer = radius - 3;
        double x1 = inner * sin(rad);
        double y1 = -inner * cos(rad);
        double x2 = outer * sin(rad);
        double y2 = -outer * cos(rad);
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // 方向文字：N/S/W/E
    painter.setPen(QPen(Qt::white, 2));
    QFont font = painter.font();
    font.setPointSizeF(radius / 6);
    font.setBold(true);
    painter.setFont(font);

    struct Dir { int deg; QString text; };
    Dir dirs[] = {{0, "N"}, {90, "E"}, {180, "S"}, {270, "W"}};
    for (const auto &d : dirs) {
        double rad = d.deg * M_PI / 180.0;
        double textR = radius * 0.70;  // 文字在半径70%位置
        double x = textR * sin(rad);
        double y = -textR * cos(rad);
        QRectF rect(x - radius/5, y - radius/5, radius/2.5, radius/2.5);
        painter.drawText(rect, Qt::AlignCenter, d.text);
    }
}

void NavGlobalView::drawVehicle(QPainter &painter, const QPointF &center, double radius)
{
    painter.save();
    painter.translate(center);

    double size = radius * 0.20;   // 车体大小基于半径
    double w = size * 0.7;         // 车宽
    double h = size * 0.9;         // 车高

    // 褐色粗线条绘制车体
    QPen penBrown(QColor(139, 69, 19));
    penBrown.setWidth(4);
    painter.setPen(penBrown);
    painter.setBrush(Qt::NoBrush);  // 只画线框

    // 车身矩形
    QRectF bodyRect(-w/2, -h/2, w, h);
    painter.drawRect(bodyRect);

    // 车头三角形（朝上）
    QPolygonF triangle;
    double triangleHeight = size * 0.35;
    triangle << QPointF(0, -h/2 - triangleHeight)  // 顶点朝北
             << QPointF(-w/2, -h/2)                // 左下
             << QPointF(w/2, -h/2);                // 右下
    painter.drawPolygon(triangle);

    painter.restore();
}

void NavGlobalView::drawIndicators(QPainter &painter, const QPointF &center, double radius)
{
    double dotRadius = radius * 0.15;      // 指示灯大小
    double cornerOffset = radius * 0.01;   // 边缘偏移
    double dotRadiuspos = dotRadius * 0.5;

    // 左上角：GPS定位指示灯（绿色=有效，红色=无效）
    QPointF topLeft(center.x() - radius + cornerOffset + dotRadiuspos,
                    center.y() - radius + cornerOffset + dotRadiuspos);
    painter.setPen(Qt::NoPen);  // 无边框
    painter.setBrush(m_gpsValid ? Qt::green : Qt::red);
    painter.drawEllipse(topLeft, dotRadius, dotRadius);

    // 右上角：偏航报警指示灯（红色报警，灰色正常）
    QPointF topRight(center.x() + radius - cornerOffset - dotRadiuspos,
                     center.y() - radius + cornerOffset + dotRadiuspos);
    painter.setPen(Qt::NoPen);
    if (m_offCourse) {
        painter.setBrush(Qt::red);        // 偏航时亮红色
        painter.drawEllipse(topRight, dotRadius, dotRadius);
    } else {
        painter.setBrush(Qt::NoBrush);    // 正常时灰色空心
        QPen penGray(Qt::lightGray);
        penGray.setWidth(2);
        painter.setPen(penGray);
        painter.drawEllipse(topRight, dotRadius, dotRadius);
    }
}

void NavGlobalView::drawSatelliteStatus(QPainter &painter, const QPointF &center, double radius)
{
    // 计算右下角位置用于显示卫星图标
    double cornerOffset = radius * 0.12;
    double iconSize = radius * 0.3;

    QPointF bottomRight(center.x() + radius - cornerOffset - iconSize/2,
                        center.y() + radius - cornerOffset - iconSize/2);

    // 根据状态显示对应的卫星图标
    if (m_satStatus >= 0 && m_satStatus <= 3) {
        QRectF iconRect(bottomRight.x(), bottomRight.y(), iconSize, iconSize);
        painter.drawPixmap(iconRect.toRect(), m_satIcons[m_satStatus]);
    }
}
