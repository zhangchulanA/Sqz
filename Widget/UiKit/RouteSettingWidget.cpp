#include "RouteSettingWidget.h"
#include <QFormLayout>

RouteSettingWidget::RouteSettingWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
}

void RouteSettingWidget::setupUI()
{
    // ----- 经度组 -----
    QGroupBox *lonGroup = new QGroupBox("经度 (Longitude)");
    m_longDirCombo = new QComboBox();
    m_longDirCombo->addItems({"E", "W"});
    m_longDegSpin = new QSpinBox();
    m_longDegSpin->setRange(0, 180);
    m_longDegSpin->setSuffix("°");
    m_longMinSpin = new QSpinBox();
    m_longMinSpin->setRange(0, 59);
    m_longMinSpin->setSuffix("'");
    m_longSecSpin = new QDoubleSpinBox();
    m_longSecSpin->setRange(0.0, 59.999);
    m_longSecSpin->setDecimals(3);
    m_longSecSpin->setSuffix("\"");
    m_longSecSpin->setSingleStep(0.001);

    QHBoxLayout *lonLayout = new QHBoxLayout;
    lonLayout->addWidget(m_longDirCombo);
    lonLayout->addWidget(m_longDegSpin);
    lonLayout->addWidget(m_longMinSpin);
    lonLayout->addWidget(m_longSecSpin);
    lonGroup->setLayout(lonLayout);

    // ----- 纬度组 -----
    QGroupBox *latGroup = new QGroupBox("纬度 (Latitude)");
    m_latDirCombo = new QComboBox();
    m_latDirCombo->addItems({"N", "S"});
    m_latDegSpin = new QSpinBox();
    m_latDegSpin->setRange(0, 90);
    m_latDegSpin->setSuffix("°");
    m_latMinSpin = new QSpinBox();
    m_latMinSpin->setRange(0, 59);
    m_latMinSpin->setSuffix("'");
    m_latSecSpin = new QDoubleSpinBox();
    m_latSecSpin->setRange(0.0, 59.999);
    m_latSecSpin->setDecimals(3);
    m_latSecSpin->setSuffix("\"");
    m_latSecSpin->setSingleStep(0.001);

    QHBoxLayout *latLayout = new QHBoxLayout;
    latLayout->addWidget(m_latDirCombo);
    latLayout->addWidget(m_latDegSpin);
    latLayout->addWidget(m_latMinSpin);
    latLayout->addWidget(m_latSecSpin);
    latGroup->setLayout(latLayout);

    // ----- 时间组（持续天数/时/分）-----
    QGroupBox *timeGroup = new QGroupBox("持续时间 (Duration)");
    m_daySpin = new QSpinBox();
    m_daySpin->setRange(0, 999);
    m_daySpin->setSuffix(" D");
    m_hourSpin = new QSpinBox();
    m_hourSpin->setRange(0, 23);
    m_hourSpin->setSuffix(" H");
    m_minSpin = new QSpinBox();
    m_minSpin->setRange(0, 59);
    m_minSpin->setSuffix(" M");

    QHBoxLayout *timeLayout = new QHBoxLayout;
    timeLayout->addWidget(m_daySpin);
    timeLayout->addWidget(m_hourSpin);
    timeLayout->addWidget(m_minSpin);
    timeGroup->setLayout(timeLayout);

    // ----- 主布局 -----
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(lonGroup);
    mainLayout->addWidget(latGroup);
    mainLayout->addWidget(timeGroup);
    setLayout(mainLayout);
}

void RouteSettingWidget::connectSignals()
{
    // 将所有子控件的 valueChanged / currentIndexChanged 连接到统一槽
    auto connectChild = [this](QObject *obj) {
        if (auto spin = qobject_cast<QSpinBox*>(obj))
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RouteSettingWidget::onValueChanged);
        else if (auto dspin = qobject_cast<QDoubleSpinBox*>(obj))
            connect(dspin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RouteSettingWidget::onValueChanged);
        else if (auto combo = qobject_cast<QComboBox*>(obj))
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RouteSettingWidget::onValueChanged);
    };

    // 遍历所有子控件（简化，实际可递归）
    for (auto child : findChildren<QWidget*>()) {
        connectChild(child);
    }
}

void RouteSettingWidget::onValueChanged()
{
    emit dataChanged();
}

// ----- 获取接口实现 -----
double RouteSettingWidget::longitudeDecimal() const
{
    double deg = m_longDegSpin->value();
    double min = m_longMinSpin->value() / 60.0;
    double sec = m_longSecSpin->value() / 3600.0;
    double val = deg + min + sec;
    if (m_longDirCombo->currentText() == "W")
        val = -val;
    return val;
}

double RouteSettingWidget::latitudeDecimal() const
{
    double deg = m_latDegSpin->value();
    double min = m_latMinSpin->value() / 60.0;
    double sec = m_latSecSpin->value() / 3600.0;
    double val = deg + min + sec;
    if (m_latDirCombo->currentText() == "S")
        val = -val;
    return val;
}

RouteSettingWidget::DMS RouteSettingWidget::longitudeDMS() const
{
    return {m_longDegSpin->value(), m_longMinSpin->value(), m_longSecSpin->value()};
}

RouteSettingWidget::DMS RouteSettingWidget::latitudeDMS() const
{
    return {m_latDegSpin->value(), m_latMinSpin->value(), m_latSecSpin->value()};
}

QString RouteSettingWidget::longitudeDirection() const { return m_longDirCombo->currentText(); }
QString RouteSettingWidget::latitudeDirection() const  { return m_latDirCombo->currentText(); }

int RouteSettingWidget::totalMinutes() const
{
    return m_daySpin->value() * 24 * 60 + m_hourSpin->value() * 60 + m_minSpin->value();
}

void RouteSettingWidget::getTime(int &days, int &hours, int &minutes) const
{
    days = m_daySpin->value();
    hours = m_hourSpin->value();
    minutes = m_minSpin->value();
}

// ----- 设置接口实现 -----
void RouteSettingWidget::setLongitude(double deg, const QString &dir)
{
    bool negative = (deg < 0);
    if (negative) deg = -deg;
    int d = static_cast<int>(deg);
    double rem = (deg - d) * 60.0;
    int m = static_cast<int>(rem);
    double s = (rem - m) * 60.0;
    m_longDegSpin->setValue(d);
    m_longMinSpin->setValue(m);
    m_longSecSpin->setValue(s);
    m_longDirCombo->setCurrentText(negative ? "W" : dir);
}

void RouteSettingWidget::setLatitude(double deg, const QString &dir)
{
    bool negative = (deg < 0);
    if (negative) deg = -deg;
    int d = static_cast<int>(deg);
    double rem = (deg - d) * 60.0;
    int m = static_cast<int>(rem);
    double s = (rem - m) * 60.0;
    m_latDegSpin->setValue(d);
    m_latMinSpin->setValue(m);
    m_latSecSpin->setValue(s);
    m_latDirCombo->setCurrentText(negative ? "S" : dir);
}

void RouteSettingWidget::setLongitudeDMS(int deg, int min, double sec, const QString &dir)
{
    m_longDegSpin->setValue(deg);
    m_longMinSpin->setValue(min);
    m_longSecSpin->setValue(sec);
    m_longDirCombo->setCurrentText(dir);
}

void RouteSettingWidget::setLatitudeDMS(int deg, int min, double sec, const QString &dir)
{
    m_latDegSpin->setValue(deg);
    m_latMinSpin->setValue(min);
    m_latSecSpin->setValue(sec);
    m_latDirCombo->setCurrentText(dir);
}

void RouteSettingWidget::setTime(int totalMinutes)
{
    int days = totalMinutes / (24 * 60);
    int remainder = totalMinutes % (24 * 60);
    int hours = remainder / 60;
    int minutes = remainder % 60;
    m_daySpin->setValue(days);
    m_hourSpin->setValue(hours);
    m_minSpin->setValue(minutes);
}
