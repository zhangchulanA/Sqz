#ifndef TESTWIDGET_H
#define TESTWIDGET_H

#include <QWidget>
#include "UdpServer.h"
#include "SqzHub.h"
#include "TimeoutKeeper.h"
#include "TimerUtils.h"
#include "FramelessWidget.h"
namespace Ui {
class TestWidget;
}

struct AAA{
    int a;
    QString b = "333";
};

class TestWidget : public FramelessWidget
{
    Q_OBJECT

    ENABLE_JSON
    public:
        enum State { Idle, Running, Done };
    Q_ENUM(State)

    DECLARE_ENUM_STR(State, State)

    AUTO_CONNECT_DECLARE


    public:
        explicit TestWidget(QWidget *parent = nullptr);
    ~TestWidget();
    Q_INVOKABLE void M_InitTrUi();

    void inittable();
private slots:
    void on_pushButton_clicked();
    void ReceiveUdpData(const QVariantList& list);
    void on_pushButton_2_clicked();

private:
    Ui::TestWidget *ui;

    UdpServer * m_udp = nullptr;

    TimeoutKeeper* keeper = nullptr;

    TimerUtils * tu;
};

#endif // TESTWIDGET_H
