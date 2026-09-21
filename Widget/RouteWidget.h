// RouteWidget.h
#ifndef ROUTEWIDGET_H
#define ROUTEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QButtonGroup>
#include <QRegularExpression>

class RouteWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RouteWidget(QWidget *parent = nullptr);
    ~RouteWidget();

    // 获取输入的经纬度（度分秒格式，秒带小数）
    bool getLonLat(double &lon, double &lat) const;
    // 获取输入的时间（秒为单位）
    bool getTime(int &totalSeconds) const;

private slots:
    void onDigitClicked();
    void onDirectionClicked();
    void onClearClicked();
    void onBackspaceClicked();
    void onConfirmClicked();
    void onFocusChanged();

private:
    // 界面初始化
    void initUI();
    void initStyle();

    // 核心数据显示区域
    QLineEdit *m_lonEdit;      // 经度显示
    QLineEdit *m_latEdit;      // 纬度显示
    QLineEdit *m_timeEdit;     // 时间显示

    // 当前输入焦点所在区域 (0-经度, 1-纬度, 2-时间)
    int m_currentFocus;

    // 方向选择按钮 (经度: E/W, 纬度: N/S)
    QPushButton *m_lonDirBtn;
    QPushButton *m_latDirBtn;

    // 数字按键 (0-9, 小数点, 退格, 清除)
    QList<QPushButton*> m_digitButtons;

    // 辅助方法：格式化显示
    void formatLonLat(QLineEdit* edit, double value, bool isLon);
    bool parseLonLat(const QString& text, double& value, bool isLon) const;
    bool parseTime(const QString& text, int& seconds) const;

    // 更新显示内容（针对当前焦点）
    void updateDisplay(const QString& text);
    void appendToCurrent(const QString& text);
    void clearCurrent();
    void backspaceCurrent();

    // 数据存储 (度分秒)
    struct Angle {
        int degrees;
        int minutes;
        double seconds;  // 带小数
        char direction;  // 'E','W','N','S'
    };

    Angle m_lon;  // 经度
    Angle m_lat;  // 纬度
    int m_timeSeconds;  // 时间（秒）

    // 输入状态
    enum InputState {
        StateDegree,
        StateMinute,
        StateSecond,
        StateDirection
    };
    InputState m_inputState;
    bool m_hasDecimal;  // 当前输入是否包含小数点
};

#endif // ROUTEWIDGET_H
