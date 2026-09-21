#ifndef NAVGLOBALVIEW_H
#define NAVGLOBALVIEW_H

#include <QWidget>

class NavGlobalView : public QWidget
{
    Q_OBJECT
    // Qt属性系统，支持动态属性和QML绑定
    Q_PROPERTY(double heading READ heading WRITE setHeading)
    Q_PROPERTY(bool gpsValid READ gpsValid WRITE setGpsValid)
    Q_PROPERTY(bool offCourse READ offCourse WRITE setOffCourse)
    Q_PROPERTY(int satelliteStatus READ satelliteStatus WRITE setSatelliteStatus)

public:
    explicit NavGlobalView(QWidget *parent = nullptr);

    // 属性访问函数
    double heading() const { return m_heading; }
    void setHeading(double deg);           // 设置航向角（度）

    bool gpsValid() const { return m_gpsValid; }
    void setGpsValid(bool valid);          // 设置GPS定位状态

    bool offCourse() const { return m_offCourse; }
    void setOffCourse(bool off);           // 设置偏航报警状态

    int satelliteStatus() const { return m_satStatus; }
    void setSatelliteStatus(int status);   // 设置卫星状态(0-3)

protected:
    void paintEvent(QPaintEvent *event) override;   // 绘制事件
    void resizeEvent(QResizeEvent *event) override; // 尺寸变化事件

private:
    // 状态数据
    double m_heading;          // 航向角（度），0=正北
    bool m_gpsValid;           // 定位有效标志
    bool m_offCourse;          // 偏航报警标志
    int m_satStatus;           // 卫星状态：0无信号,1 GPS,2 北斗,3 组合

    QRectF m_contentRect;      // 绘制区域（留边距）
    QPixmap m_satIcons[4];     // 存储4种状态的图标

    // 绘制辅助函数
    void drawOuterCircle(QPainter &painter, const QPointF &center, double radius);
    void drawVehicle(QPainter &painter, const QPointF &center, double radius);
    void drawIndicators(QPainter &painter, const QPointF &center, double radius);
    void drawSatelliteStatus(QPainter &painter, const QPointF &center, double radius);
};

#endif // NAVGLOBALVIEW_H
