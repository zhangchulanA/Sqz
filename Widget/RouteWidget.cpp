// RouteWidget.cpp
#include "RouteWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QButtonGroup>
#include <QFrame>
#include <QDebug>
#include <QRegularExpression>
#include <QMessageBox>

RouteWidget::RouteWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentFocus(0)
    , m_timeSeconds(0)
    , m_inputState(StateDegree)
    , m_hasDecimal(false)
{
    // 初始化经纬度数据
    m_lon = {0, 0, 0.0, 'E'};
    m_lat = {0, 0, 0.0, 'N'};

    initUI();
    initStyle();

    // 默认焦点在经度
    m_lonEdit->setFocus();
    m_currentFocus = 0;
}

RouteWidget::~RouteWidget()
{
}

void RouteWidget::initUI()
{
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ==================== 标题 ====================
    QLabel *titleLabel = new QLabel("路线设置", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00BFFF;");
    mainLayout->addWidget(titleLabel);

    // ==================== 经纬度显示区域 ====================
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(10);
    gridLayout->setVerticalSpacing(5);

    // 经度行
    QLabel *lonLabel = new QLabel("Lon", this);
    lonLabel->setStyleSheet("color: #FF4500; font-weight: bold;");
    gridLayout->addWidget(lonLabel, 0, 0);

    m_lonDirBtn = new QPushButton("E", this);
    m_lonDirBtn->setFixedSize(40, 30);
    m_lonDirBtn->setCheckable(true);
    m_lonDirBtn->setChecked(true);
    gridLayout->addWidget(m_lonDirBtn, 0, 1);

    m_lonEdit = new QLineEdit(this);
    m_lonEdit->setReadOnly(true);
    m_lonEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lonEdit->setStyleSheet("QLineEdit { background: #1A1A1A; color: #00BFFF; border: 2px solid #333; border-radius: 5px; padding: 5px; font-size: 16px; } QLineEdit:focus { border: 2px solid #00BFFF; }");
    gridLayout->addWidget(m_lonEdit, 0, 2, 1, 3);

    // 纬度行
    QLabel *latLabel = new QLabel("Lat", this);
    latLabel->setStyleSheet("color: #FF4500; font-weight: bold;");
    gridLayout->addWidget(latLabel, 1, 0);

    m_latDirBtn = new QPushButton("N", this);
    m_latDirBtn->setFixedSize(40, 30);
    m_latDirBtn->setCheckable(true);
    m_latDirBtn->setChecked(true);
    gridLayout->addWidget(m_latDirBtn, 1, 1);

    m_latEdit = new QLineEdit(this);
    m_latEdit->setReadOnly(true);
    m_latEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_latEdit->setStyleSheet("QLineEdit { background: #1A1A1A; color: #00BFFF; border: 2px solid #333; border-radius: 5px; padding: 5px; font-size: 16px; } QLineEdit:focus { border: 2px solid #00BFFF; }");
    gridLayout->addWidget(m_latEdit, 1, 2, 1, 3);

    // 时间行
    QLabel *timeLabel = new QLabel("Time", this);
    timeLabel->setStyleSheet("color: #FF4500; font-weight: bold;");
    gridLayout->addWidget(timeLabel, 2, 0);

    m_timeEdit = new QLineEdit(this);
    m_timeEdit->setReadOnly(true);
    m_timeEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timeEdit->setStyleSheet("QLineEdit { background: #1A1A1A; color: #00BFFF; border: 2px solid #333; border-radius: 5px; padding: 5px; font-size: 16px; } QLineEdit:focus { border: 2px solid #00BFFF; }");
    gridLayout->addWidget(m_timeEdit, 2, 1, 1, 4);

    mainLayout->addLayout(gridLayout);

    // ==================== 分隔线 ====================
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #333;");
    mainLayout->addWidget(line);

    // ==================== 计算器风格数字键盘 ====================
    QGridLayout *keypadLayout = new QGridLayout();
    keypadLayout->setSpacing(8);

    // 创建数字按键 0-9
    QStringList digitLabels = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "0"};
    int row = 0, col = 0;
    for (const QString& label : digitLabels) {
        QPushButton *btn = new QPushButton(label, this);
        btn->setFixedSize(60, 50);
        btn->setStyleSheet("QPushButton { background: #2A2A2A; color: white; border: 1px solid #444; border-radius: 5px; font-size: 18px; } QPushButton:hover { background: #3A3A3A; } QPushButton:pressed { background: #00BFFF; }");
        connect(btn, &QPushButton::clicked, this, &RouteWidget::onDigitClicked);
        m_digitButtons.append(btn);
        keypadLayout->addWidget(btn, row, col);
        col++;
        if (col > 2) { col = 0; row++; }
    }

    // 小数点
    QPushButton *dotBtn = new QPushButton(".", this);
    dotBtn->setFixedSize(60, 50);
    dotBtn->setStyleSheet("QPushButton { background: #2A2A2A; color: white; border: 1px solid #444; border-radius: 5px; font-size: 18px; } QPushButton:hover { background: #3A3A3A; } QPushButton:pressed { background: #00BFFF; }");
    connect(dotBtn, &QPushButton::clicked, this, &RouteWidget::onDigitClicked);
    m_digitButtons.append(dotBtn);
    keypadLayout->addWidget(dotBtn, 3, 0);

    // 方向切换按钮 (经度E/W, 纬度N/S)
    QPushButton *dirBtn = new QPushButton("E/W", this);
    dirBtn->setFixedSize(60, 50);
    dirBtn->setStyleSheet("QPushButton { background: #FF4500; color: white; border: 1px solid #FF4500; border-radius: 5px; font-size: 14px; font-weight: bold; } QPushButton:hover { background: #FF6347; }");
    connect(dirBtn, &QPushButton::clicked, this, &RouteWidget::onDirectionClicked);
    keypadLayout->addWidget(dirBtn, 3, 1);

    // 退格键
    QPushButton *backBtn = new QPushButton("⌫", this);
    backBtn->setFixedSize(60, 50);
    backBtn->setStyleSheet("QPushButton { background: #FF4500; color: white; border: 1px solid #FF4500; border-radius: 5px; font-size: 18px; } QPushButton:hover { background: #FF6347; }");
    connect(backBtn, &QPushButton::clicked, this, &RouteWidget::onBackspaceClicked);
    keypadLayout->addWidget(backBtn, 3, 2);

    // 清除键
    QPushButton *clearBtn = new QPushButton("C", this);
    clearBtn->setFixedSize(60, 50);
    clearBtn->setStyleSheet("QPushButton { background: #FF4500; color: white; border: 1px solid #FF4500; border-radius: 5px; font-size: 18px; } QPushButton:hover { background: #FF6347; }");
    connect(clearBtn, &QPushButton::clicked, this, &RouteWidget::onClearClicked);
    keypadLayout->addWidget(clearBtn, 4, 0);

    // 确认键
    QPushButton *confirmBtn = new QPushButton("✓ 执行", this);
    confirmBtn->setFixedSize(130, 50);
    confirmBtn->setStyleSheet("QPushButton { background: #00BFFF; color: black; border: 1px solid #00BFFF; border-radius: 5px; font-size: 16px; font-weight: bold; } QPushButton:hover { background: #1E90FF; }");
    connect(confirmBtn, &QPushButton::clicked, this, &RouteWidget::onConfirmClicked);
    keypadLayout->addWidget(confirmBtn, 4, 1, 1, 2);

    mainLayout->addLayout(keypadLayout);

    // ==================== 焦点切换 ====================
    // 点击编辑框切换焦点
    connect(m_lonEdit, &QLineEdit::selectionChanged, [this]() {
        m_currentFocus = 0;
        m_lonEdit->setFocus();
    });
    connect(m_latEdit, &QLineEdit::selectionChanged, [this]() {
        m_currentFocus = 1;
        m_latEdit->setFocus();
    });
    connect(m_timeEdit, &QLineEdit::selectionChanged, [this]() {
        m_currentFocus = 2;
        m_timeEdit->setFocus();
    });

    // 设置初始显示
    formatLonLat(m_lonEdit, 0.0, true);
    formatLonLat(m_latEdit, 0.0, false);
    m_timeEdit->setText("00:00:00");
}

void RouteWidget::initStyle()
{
    // 设置窗口整体风格
    setStyleSheet("QWidget { background: #0A0A0A; color: white; } QLineEdit { background: #1A1A1A; color: #00BFFF; border: 2px solid #333; border-radius: 5px; padding: 5px; }");
    setFixedSize(400, 500);
}

void RouteWidget::onDigitClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    QString text = btn->text();

    // 处理小数点
    if (text == ".") {
        if (m_hasDecimal) return;  // 已经包含小数点
        m_hasDecimal = true;
    }

    appendToCurrent(text);
}

void RouteWidget::onDirectionClicked()
{
    // 根据当前焦点切换方向
    if (m_currentFocus == 0) {
        // 经度 E/W
        if (m_lon.direction == 'E') {
            m_lon.direction = 'W';
            m_lonDirBtn->setText("W");
        } else {
            m_lon.direction = 'E';
            m_lonDirBtn->setText("E");
        }
        formatLonLat(m_lonEdit, 0.0, true);
    } else if (m_currentFocus == 1) {
        // 纬度 N/S
        if (m_lat.direction == 'N') {
            m_lat.direction = 'S';
            m_latDirBtn->setText("S");
        } else {
            m_lat.direction = 'N';
            m_latDirBtn->setText("N");
        }
        formatLonLat(m_latEdit, 0.0, false);
    }
}

void RouteWidget::onClearClicked()
{
    clearCurrent();
}

void RouteWidget::onBackspaceClicked()
{
    backspaceCurrent();
}

void RouteWidget::onConfirmClicked()
{
    double lon, lat;
    int seconds;

    if (getLonLat(lon, lat) && getTime(seconds)) {
        // 这里可以发射信号或执行路线设置操作
        qDebug() << "Route confirmed: Lon=" << lon << " Lat=" << lat << " Time=" << seconds << "s";
        // 可以添加一个简单反馈
        QMessageBox::information(this, "路线设置",
            QString("经度: %1\n纬度: %2\n时间: %3秒").arg(lon).arg(lat).arg(seconds));
    }
}

void RouteWidget::onFocusChanged()
{

}

void RouteWidget::formatLonLat(QLineEdit* edit, double value, bool isLon)
{
    // 实际使用时，value应该从m_lon/m_lat获取
    // 这里只是示例格式化
    QString dir = isLon ? (m_lon.direction == 'E' ? "E" : "W") : (m_lat.direction == 'N' ? "N" : "S");
    QString text = QString("%1%2°%3'%4\"")
        .arg(dir)
        .arg(isLon ? m_lon.degrees : m_lat.degrees, 2, 10, QChar('0'))
        .arg(isLon ? m_lon.minutes : m_lat.minutes, 2, 10, QChar('0'))
        .arg(isLon ? m_lon.seconds : m_lat.seconds, 0, 'f', 3);
    edit->setText(text);
}

bool RouteWidget::parseLonLat(const QString& text, double& value, bool isLon) const
{
    // 解析度分秒格式：E/W 090°56'28.048" 或 N/S 23°36'18.648"
    QRegularExpression regex(R"(([EWNS])(\d{2,3})°(\d{2})'([\d.]+)\")");
    QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch()) return false;

    int deg = match.captured(2).toInt();
    int min = match.captured(3).toInt();
    double sec = match.captured(4).toDouble();
    char dir = match.captured(1).at(0).toLatin1();

    // 转换为十进制
    value = deg + min / 60.0 + sec / 3600.0;
    if (dir == 'W' || dir == 'S') value = -value;

    return true;
}

bool RouteWidget::parseTime(const QString& text, int& seconds) const
{
    QRegularExpression regex(R"((\d{2}):(\d{2}):(\d{2}))");
    QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch()) return false;

    int h = match.captured(1).toInt();
    int m = match.captured(2).toInt();
    int s = match.captured(3).toInt();

    seconds = h * 3600 + m * 60 + s;
    return true;
}

void RouteWidget::updateDisplay(const QString& text)
{
    switch (m_currentFocus) {
        case 0: m_lonEdit->setText(text); break;
        case 1: m_latEdit->setText(text); break;
        case 2: m_timeEdit->setText(text); break;
    }
}

void RouteWidget::appendToCurrent(const QString& text)
{
    // 获取当前编辑框的文本
    QString currentText;
    switch (m_currentFocus) {
        case 0: currentText = m_lonEdit->text(); break;
        case 1: currentText = m_latEdit->text(); break;
        case 2: currentText = m_timeEdit->text(); break;
    }

    // 简单处理：移除格式字符，只保留数字和小数点
    QString cleanText = currentText;
    cleanText.remove(QRegularExpression("[^\\d.]"));
    cleanText += text;

    // 限制输入长度（根据不同类型）
    if (m_currentFocus == 2) {
        // 时间格式：只允许数字，自动格式化为 HH:MM:SS
        if (cleanText.length() > 6) cleanText.chop(1);
        QString formatted = cleanText;
        if (formatted.length() >= 2) formatted.insert(2, ":");
        if (formatted.length() >= 5) formatted.insert(5, ":");
        updateDisplay(formatted);
    } else {
        // 经纬度：度分秒格式
        // 实际项目中应该更智能地解析，这里简化
        if (cleanText.length() > 12) cleanText.chop(1);
        updateDisplay(cleanText);
    }
}

void RouteWidget::clearCurrent()
{
    switch (m_currentFocus) {
        case 0: m_lonEdit->clear(); m_lon = {0,0,0.0,'E'}; break;
        case 1: m_latEdit->clear(); m_lat = {0,0,0.0,'N'}; break;
        case 2: m_timeEdit->setText("00:00:00"); m_timeSeconds = 0; break;
    }
    m_hasDecimal = false;
}

void RouteWidget::backspaceCurrent()
{
    QString currentText;
    switch (m_currentFocus) {
        case 0: currentText = m_lonEdit->text(); break;
        case 1: currentText = m_latEdit->text(); break;
        case 2: currentText = m_timeEdit->text(); break;
    }

    if (!currentText.isEmpty()) {
        currentText.chop(1);
        updateDisplay(currentText);
    }
}

bool RouteWidget::getLonLat(double &lon, double &lat) const
{
    // 从编辑框解析
    if (!parseLonLat(m_lonEdit->text(), lon, true)) return false;
    if (!parseLonLat(m_latEdit->text(), lat, false)) return false;
    return true;
}

bool RouteWidget::getTime(int &totalSeconds) const
{
    return parseTime(m_timeEdit->text(), totalSeconds);
}
