#include "MainWindowTest.h"
#include "ui_MainWindowTest.h"
#include "PropertyAnimator.h"
#include "EventAggregator.h"

#include <PropertyAnimator.h>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
MainWindowTest::MainWindowTest(QWidget *parent) :
    SqzMainWindow(parent),
    ui(new Ui::MainWindowTest)
{
    ui->setupUi(this);
    // ========== 防抖：搜索框 ==========
     SQZ_DEBOUNCE(ui->lineEdit, &QLineEdit::textChanged,
         [](const QVariantList& args) {
             qDebug() << "搜索:" << args.first().toString();
         },
         5000);  // 停止输入300ms后执行
}

MainWindowTest::~MainWindowTest()
{
    delete ui;
}

SQZOBJECT_NOARG(MainWindowTest)

void MainWindowTest::on_pushButton_2_clicked()//ui->pushButton
{
    // 位置动画
     PropertyAnimator::animate(ui->pushButton)
         .property("pos")
         .from(QPoint(0, 0))
         .to(QPoint(300, 200))
         .duration(1000)
         .easing(QEasingCurve::OutBounce)
         .onStarted([](){ qDebug() << "位置动画开始"; })
         .onFinished([](){ qDebug() << "位置动画结束"; })
         .start();

}
