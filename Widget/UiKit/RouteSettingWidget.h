#ifndef ROUTESETTINGWIDGET_H
#define ROUTESETTINGWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>

class RouteSettingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RouteSettingWidget(QWidget *parent = nullptr);

    // 经纬度结构（度分秒）
    struct DMS {
        int degrees;
        int minutes;
        double seconds;   // 秒可带小数
    };

    // 获取经纬度（十进制度）
    double longitudeDecimal() const;
    double latitudeDecimal() const;

    // 获取经纬度（度分秒结构）
    DMS longitudeDMS() const;
    DMS latitudeDMS() const;

    // 获取方向（E/W, N/S）
    QString longitudeDirection() const;
    QString latitudeDirection() const;

    // 获取时间（总分钟数，或分解）
    int totalMinutes() const;          // 总分钟数
    void getTime(int &days, int &hours, int &minutes) const;

    // 设置值（十进制度）
    void setLongitude(double deg, const QString &dir = "E");
    void setLatitude(double deg, const QString &dir = "N");

    // 设置值（度分秒）
    void setLongitudeDMS(int deg, int min, double sec, const QString &dir = "E");
    void setLatitudeDMS(int deg, int min, double sec, const QString &dir = "N");

    // 设置时间（总分钟数）
    void setTime(int totalMinutes);

signals:
    void dataChanged();   // 任何数据改变时触发

private slots:
    void onValueChanged();

private:
    // 经度控件
    QComboBox *m_longDirCombo;
    QSpinBox  *m_longDegSpin;
    QSpinBox  *m_longMinSpin;
    QDoubleSpinBox *m_longSecSpin;

    // 纬度控件
    QComboBox *m_latDirCombo;
    QSpinBox  *m_latDegSpin;
    QSpinBox  *m_latMinSpin;
    QDoubleSpinBox *m_latSecSpin;

    // 时间控件
    QSpinBox *m_daySpin;
    QSpinBox *m_hourSpin;
    QSpinBox *m_minSpin;

    void setupUI();
    void connectSignals();
};

#endif // ROUTESETTINGWIDGET_H
