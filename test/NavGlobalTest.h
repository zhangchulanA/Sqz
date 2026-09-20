#ifndef NAVGLOBALTEST_H
#define NAVGLOBALTEST_H

#include <QWidget>
#include <QTimer>

class NavGlobalView;

class NavGlobalTest : public QWidget
{
    Q_OBJECT

public:
    explicit NavGlobalTest(QWidget *parent = nullptr);
    ~NavGlobalTest();

private slots:
    void updateNavigationData();
    void updateStatusDisplay();
private:
    void setupUI();
    void setupConnections();

    NavGlobalView *m_navView;
    QTimer *m_timer;

    // 模拟数据
    double m_heading;
    bool m_gpsValid;
    bool m_offCourse;
    int m_satStatus;
    int m_counter;
};

#endif // NAVGLOBALTEST_H
