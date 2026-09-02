#include "NavGlobalTest.h"
#include "NavGlobalView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGridLayout>
#include <QDebug>

NavGlobalTest::NavGlobalTest(QWidget *parent)
    : QWidget(parent)
    , m_navView(nullptr)
    , m_timer(nullptr)
    , m_heading(0.0)
    , m_gpsValid(true)
    , m_offCourse(false)
    , m_satStatus(1)
    , m_counter(0)
{
    setupUI();
    setupConnections();

    // 启动定时器，模拟动态数据
    m_timer = new QTimer(this);
    m_timer->setInterval(100);  // 100ms更新一次
    connect(m_timer, &QTimer::timeout, this, &NavGlobalTest::updateNavigationData);
    m_timer->start();
}

NavGlobalTest::~NavGlobalTest()
{
    if (m_timer) {
        m_timer->stop();
    }
}

void NavGlobalTest::setupUI()
{
    setWindowTitle("导航全局视图测试 - NavGlobalTest");
    resize(900, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 上半部分：导航视图
    QHBoxLayout *viewLayout = new QHBoxLayout();

    // 主视图
    m_navView = new NavGlobalView(this);
    m_navView->setMinimumSize(400, 400);
    m_navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 状态显示面板
    QGroupBox *statusGroup = new QGroupBox("实时状态", this);
    QGridLayout *statusLayout = new QGridLayout(statusGroup);

    // 创建状态标签
    QLabel *headingLabel = new QLabel("航向角:", this);
    QLabel *headingValue = new QLabel("0.0°", this);
    headingValue->setObjectName("headingValue");
    headingValue->setStyleSheet("font-weight: bold; color: blue;");

    QLabel *gpsLabel = new QLabel("GPS状态:", this);
    QLabel *gpsValue = new QLabel("有效", this);
    gpsValue->setObjectName("gpsValue");
    gpsValue->setStyleSheet("font-weight: bold; color: green;");

    QLabel *offCourseLabel = new QLabel("偏航报警:", this);
    QLabel *offCourseValue = new QLabel("正常", this);
    offCourseValue->setObjectName("offCourseValue");
    offCourseValue->setStyleSheet("font-weight: bold; color: green;");

    QLabel *satLabel = new QLabel("卫星状态:", this);
    QLabel *satValue = new QLabel("GPS", this);
    satValue->setObjectName("satValue");
    satValue->setStyleSheet("font-weight: bold; color: darkblue;");

    // 添加状态标签到布局
    statusLayout->addWidget(headingLabel, 0, 0);
    statusLayout->addWidget(headingValue, 0, 1);
    statusLayout->addWidget(gpsLabel, 1, 0);
    statusLayout->addWidget(gpsValue, 1, 1);
    statusLayout->addWidget(offCourseLabel, 2, 0);
    statusLayout->addWidget(offCourseValue, 2, 1);
    statusLayout->addWidget(satLabel, 3, 0);
    statusLayout->addWidget(satValue, 3, 1);

    // 添加控制按钮
    QPushButton *resetBtn = new QPushButton("重置", this);
    QPushButton *randomBtn = new QPushButton("随机状态", this);
    QPushButton *toggleAutoBtn = new QPushButton("暂停/继续", this);
    toggleAutoBtn->setObjectName("toggleAutoBtn");

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(resetBtn);
    btnLayout->addWidget(randomBtn);
    btnLayout->addWidget(toggleAutoBtn);

    statusLayout->addLayout(btnLayout, 4, 0, 1, 2);

    // 右侧控制面板
    QGroupBox *controlGroup = new QGroupBox("手动控制", this);
    QGridLayout *controlLayout = new QGridLayout(controlGroup);

    // 航向控制滑块
    QLabel *headingSliderLabel = new QLabel("航向角:", this);
    QSlider *headingSlider = new QSlider(Qt::Horizontal, this);
    headingSlider->setObjectName("headingSlider");
    headingSlider->setRange(0, 360);
    headingSlider->setValue(0);
    headingSlider->setTickInterval(30);
    headingSlider->setTickPosition(QSlider::TicksBelow);

    QSpinBox *headingSpin = new QSpinBox(this);
    headingSpin->setObjectName("headingSpin");
    headingSpin->setRange(0, 360);
    headingSpin->setSuffix("°");

    // GPS控制
    QCheckBox *gpsCheck = new QCheckBox("GPS有效", this);
    gpsCheck->setObjectName("gpsCheck");
    gpsCheck->setChecked(true);

    // 偏航报警控制
    QCheckBox *offCourseCheck = new QCheckBox("偏航报警", this);
    offCourseCheck->setObjectName("offCourseCheck");
    offCourseCheck->setChecked(false);

    // 卫星状态选择
    QLabel *satSelectLabel = new QLabel("卫星模式:", this);
    QComboBox *satCombo = new QComboBox(this);
    satCombo->setObjectName("satCombo");
    satCombo->addItem("无信号", 0);
    satCombo->addItem("GPS", 1);
    satCombo->addItem("北斗", 2);
    satCombo->addItem("GPS+北斗", 3);
    satCombo->setCurrentIndex(1);

    controlLayout->addWidget(headingSliderLabel, 0, 0);
    controlLayout->addWidget(headingSlider, 0, 1);
    controlLayout->addWidget(headingSpin, 0, 2);
    controlLayout->addWidget(gpsCheck, 1, 0, 1, 2);
    controlLayout->addWidget(offCourseCheck, 2, 0, 1, 2);
    controlLayout->addWidget(satSelectLabel, 3, 0);
    controlLayout->addWidget(satCombo, 3, 1, 1, 2);

    // 组装上半部分
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(m_navView, 3);
    topLayout->addWidget(statusGroup, 1);
    topLayout->addWidget(controlGroup, 1);

    mainLayout->addLayout(topLayout);

    // 下半部分：信息显示
    QGroupBox *infoGroup = new QGroupBox("使用说明", this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    QLabel *infoLabel = new QLabel(
        "• 左上角指示灯: 定位有效(绿)/无效(红)\n"
        "• 右上角指示灯: 偏航报警(红)/正常(灰空心)\n"
        "• 右下角: 卫星状态显示\n"
        "• 绿色指示线: 当前航向方向\n"
        "• 车体朝上: 表示车辆前进方向\n"
        "• 蓝色外圈: 刻度盘，N/S/W/E标记方向\n"
        "• 支持动态模拟和手动控制两种模式",
        this
    );
    infoLabel->setStyleSheet("color: #333; font-size: 11px;");
    infoLayout->addWidget(infoLabel);
    mainLayout->addWidget(infoGroup, 0);

    // 存储控件引用以便在updateNavigationData中使用
    setProperty("headingValue", QVariant::fromValue(headingValue));
    setProperty("gpsValue", QVariant::fromValue(gpsValue));
    setProperty("offCourseValue", QVariant::fromValue(offCourseValue));
    setProperty("satValue", QVariant::fromValue(satValue));
    setProperty("headingSlider", QVariant::fromValue(headingSlider));
    setProperty("headingSpin", QVariant::fromValue(headingSpin));
    setProperty("gpsCheck", QVariant::fromValue(gpsCheck));
    setProperty("offCourseCheck", QVariant::fromValue(offCourseCheck));
    setProperty("satCombo", QVariant::fromValue(satCombo));
    setProperty("toggleAutoBtn", QVariant::fromValue(toggleAutoBtn));
}

void NavGlobalTest::setupConnections()
{
    // 获取控件
    QSlider *headingSlider = property("headingSlider").value<QSlider*>();
    QSpinBox *headingSpin = property("headingSpin").value<QSpinBox*>();
    QCheckBox *gpsCheck = property("gpsCheck").value<QCheckBox*>();
    QCheckBox *offCourseCheck = property("offCourseCheck").value<QCheckBox*>();
    QComboBox *satCombo = property("satCombo").value<QComboBox*>();
    QPushButton *resetBtn = findChild<QPushButton*>("重置");
    QPushButton *randomBtn = findChild<QPushButton*>("随机状态");
    QPushButton *toggleAutoBtn = property("toggleAutoBtn").value<QPushButton*>();

    // 连接信号槽
    if (headingSlider) {
        connect(headingSlider, &QSlider::valueChanged, this, [this, headingSpin](int value) {
            m_heading = value;
            m_navView->setHeading(m_heading);
            if (headingSpin) {
                headingSpin->setValue(value);
            }
            // 更新状态显示
            updateStatusDisplay();
        });
    }

    if (headingSpin) {
        connect(headingSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, headingSlider](int value) {
            m_heading = value;
            m_navView->setHeading(m_heading);
            if (headingSlider) {
                headingSlider->setValue(value);
            }
            updateStatusDisplay();
        });
    }

    if (gpsCheck) {
        connect(gpsCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_gpsValid = checked;
            m_navView->setGpsValid(checked);
            updateStatusDisplay();
        });
    }

    if (offCourseCheck) {
        connect(offCourseCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_offCourse = checked;
            m_navView->setOffCourse(checked);
            updateStatusDisplay();
        });
    }

    if (satCombo) {
        connect(satCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, satCombo](int index) {
            m_satStatus = satCombo->itemData(index).toInt();
            m_navView->setSatelliteStatus(m_satStatus);
            updateStatusDisplay();
        });
    }

    if (resetBtn) {
        connect(resetBtn, &QPushButton::clicked, this, [this, headingSlider, gpsCheck, offCourseCheck, satCombo]() {
            // 重置到初始状态
            m_heading = 0;
            m_gpsValid = true;
            m_offCourse = false;
            m_satStatus = 1;

            m_navView->setHeading(m_heading);
            m_navView->setGpsValid(m_gpsValid);
            m_navView->setOffCourse(m_offCourse);
            m_navView->setSatelliteStatus(m_satStatus);

            if (headingSlider) headingSlider->setValue(0);
            if (gpsCheck) gpsCheck->setChecked(true);
            if (offCourseCheck) offCourseCheck->setChecked(false);
            if (satCombo) satCombo->setCurrentIndex(1);

            updateStatusDisplay();
        });
    }

    if (randomBtn) {
        connect(randomBtn, &QPushButton::clicked, this, [this, headingSlider, gpsCheck, offCourseCheck, satCombo]() {
            // 生成随机状态
            m_heading = rand() % 360;
            m_gpsValid = rand() % 2 == 0;
            m_offCourse = rand() % 2 == 0;
            m_satStatus = rand() % 4;

            m_navView->setHeading(m_heading);
            m_navView->setGpsValid(m_gpsValid);
            m_navView->setOffCourse(m_offCourse);
            m_navView->setSatelliteStatus(m_satStatus);

            if (headingSlider) headingSlider->setValue(m_heading);
            if (gpsCheck) gpsCheck->setChecked(m_gpsValid);
            if (offCourseCheck) offCourseCheck->setChecked(m_offCourse);
            if (satCombo) {
                int index = satCombo->findData(m_satStatus);
                if (index >= 0) satCombo->setCurrentIndex(index);
            }

            updateStatusDisplay();
        });
    }

    if (toggleAutoBtn) {
        connect(toggleAutoBtn, &QPushButton::clicked, this, [this, toggleAutoBtn]() {
            if (m_timer->isActive()) {
                m_timer->stop();
                toggleAutoBtn->setText("启动自动");
            } else {
                m_timer->start();
                toggleAutoBtn->setText("暂停自动");
            }
        });
    }
}

void NavGlobalTest::updateNavigationData()
{
    // 模拟动态数据变化
    m_counter++;

    // 航向角缓慢变化 (0-360度)
    m_heading = (m_counter * 0.5);
    while (m_heading >= 360) m_heading -= 360;
    m_navView->setHeading(m_heading);

    // 每5秒切换一次GPS状态
    if (m_counter % 50 == 0) {
        m_gpsValid = !m_gpsValid;
        m_navView->setGpsValid(m_gpsValid);

        // 同步UI控件
        QCheckBox *gpsCheck = property("gpsCheck").value<QCheckBox*>();
        if (gpsCheck) {
            gpsCheck->blockSignals(true);
            gpsCheck->setChecked(m_gpsValid);
            gpsCheck->blockSignals(false);
        }
    }

    // 每3秒切换一次偏航报警
    if (m_counter % 30 == 0) {
        m_offCourse = !m_offCourse;
        m_navView->setOffCourse(m_offCourse);

        QCheckBox *offCourseCheck = property("offCourseCheck").value<QCheckBox*>();
        if (offCourseCheck) {
            offCourseCheck->blockSignals(true);
            offCourseCheck->setChecked(m_offCourse);
            offCourseCheck->blockSignals(false);
        }
    }

    // 每7秒切换卫星状态
    if (m_counter % 70 == 0) {
        m_satStatus = (m_satStatus + 1) % 4;
        m_navView->setSatelliteStatus(m_satStatus);

        QComboBox *satCombo = property("satCombo").value<QComboBox*>();
        if (satCombo) {
            satCombo->blockSignals(true);
            int index = satCombo->findData(m_satStatus);
            if (index >= 0) satCombo->setCurrentIndex(index);
            satCombo->blockSignals(false);
        }
    }

    // 更新状态显示
    updateStatusDisplay();
}

void NavGlobalTest::updateStatusDisplay()
{
    QLabel *headingValue = property("headingValue").value<QLabel*>();
    QLabel *gpsValue = property("gpsValue").value<QLabel*>();
    QLabel *offCourseValue = property("offCourseValue").value<QLabel*>();
    QLabel *satValue = property("satValue").value<QLabel*>();

    if (headingValue) {
        headingValue->setText(QString("%1°").arg(m_heading, 0, 'f', 1));
    }

    if (gpsValue) {
        gpsValue->setText(m_gpsValid ? "有效" : "无效");
        gpsValue->setStyleSheet(QString("font-weight: bold; color: %1;")
            .arg(m_gpsValid ? "green" : "red"));
    }

    if (offCourseValue) {
        offCourseValue->setText(m_offCourse ? "报警!" : "正常");
        offCourseValue->setStyleSheet(QString("font-weight: bold; color: %1;")
            .arg(m_offCourse ? "red" : "green"));
    }

    if (satValue) {
        QString statusText;
        switch (m_satStatus) {
            case 0: statusText = "无信号"; break;
            case 1: statusText = "GPS"; break;
            case 2: statusText = "北斗"; break;
            case 3: statusText = "GPS+北斗"; break;
            default: statusText = "未知";
        }
        satValue->setText(statusText);
    }
}
