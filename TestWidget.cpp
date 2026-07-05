#include "TestWidget.h"
#include "ui_TestWidget.h"

#include "Translator.h"
#include "SqzBus.h"
#include <CustomSearchBox.h>
#include "SqzBus.h"
#include "SqzVar.h"
#include "MsgBox.h"
#include "SuperTableAll.h"
#include "PropertyAnimator.h"

Q_DECLARE_METATYPE(QHostAddress)


TestWidget::TestWidget(QWidget *parent) :
    FramelessWidget(parent),
    ui(new Ui::TestWidget)
{
    ui->setupUi(this);
    MyTranslator.registerWidget(this);
    M_InitTrUi();
    qRegisterMetaType<QHostAddress>("QHostAddress");

    SqzVar::Set("key",12);
    logdebug<< SqzVar::Get("key").toInt();


    // 基础窗口配置
    setWindowRadius(10);
    setShadowEnable(true);
    setResizeEnable(true);
    setSaveWindowState(true, "MainWindow");
    setDragByEmptyArea(true); // 隐藏标题栏后可拖动空白窗口

    // ========== 标题栏美化接口（解决黑底、丑按钮、黑方块） ==========
    auto bar = titleBar();
    bar->setWindowTitle("Qt5.12 无边框窗口");
    bar->hideIcon(); // 彻底消除左侧黑色方块
    bar->setTitleBarBgColor("#FFFFFF"); // 标题栏改为白色，和内容统一
    bar->setTitleTextColor("#111111");  // 标题文字黑色
    bar->setTitleBarHeight(36);         // 自定义标题栏高度

    // 【标题栏开关】两种用法
//     setTitleBarVisible(false); // 隐藏标题栏
     setTitleBarVisible(true);  // 显示标题栏

     QLabel * lb = new QLabel("业务内容区域，标题栏已改为白色，可拖动窗口",this);
        lb->move(400,100);

}

TestWidget::~TestWidget()
{
    AUTO_CONNECT_DESTRUCTOR;
    delete ui;
}

void TestWidget::M_InitTrUi()
{
    ui->pushButton->setText(TR("语言同步测试"));
    setWindowTitle(TR("测试弹窗"));
}

void TestWidget::inittable()
{
    resize(1100,650);

    SuperTableWidget* table = new SuperTableWidget();
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10,10,10,10);
    lay->addWidget(table);

    // 1. 设置表头
    QList<TableColumnConfig> headers = {
        {"id", "ID", TableCellType::Text, 80},
        {"name", "任务名称", TableCellType::Text, 220},
        {"status", "运行状态", TableCellType::StateTag, 130},
        {"progress", "完成进度", TableCellType::Progress, 160},
        {"sel", "选择", TableCellType::CheckBox, 90}
    };
    table->setHeaders(headers);

    // 2. 尺寸统一设置
    table->setGlobalRowHeight(36);    // 行高36
    table->setHeaderHeight(38);       // 表头高度
    table->setTableMinSize(800,400);  // 最小尺寸

    // 3. 全局样式
    QString qss = R"(
    QTableView
    {
        background-color: #ffffff;
        border: 1px solid #d0d7e3;
        gridline-color: #e5e9f2;
        selection-background-color: #cce5ff;
    }
    QHeaderView::section
    {
        background-color: #f5f7fa;
        border:none;
        border-right:1px solid #e5e9f2;
        border-bottom:1px solid #d0d7e3;
        padding-left:8px;
    }
    )";
    table->setTableStyleSheet(qss);

    // 4. 测试数据
    QList<TableRowData> datas;
    for(int i=0;i<3000;i++)
    {
        TableRowData r;
        r.set("id",i);
        r.set("name",QString("数据项_%1").arg(i));
        r.set("status",i%3==0?"正常":i%3==1?"失败":"等待");
        r.set("progress",rand()%100);
        r.set("sel",i%2==0);
        datas.append(r);
    }
    table->addRows(datas);

    // 行染色规则
    table->setRowColorRule([](const TableRowData& row)->QColor{
        if(row.get("status").toString() == "失败")
            return QColor(255,235,235);
        return QColor();
    });
}

bool ret = false;
void TestWidget::on_pushButton_clicked()
{

MsgBox::info("提示", "欢迎使用本软件", 0);
MsgBox::warning("提示", "欢迎使用本软件", 0);
MsgBox::critical("提示", "欢迎使用本软件", 0);
MsgBox::success("提示", "欢迎使用本软件", 0);

MsgBox::toast("提示");
}

void TestWidget::ReceiveUdpData(const QVariantList &list)
{
    QHostAddress address = list[0].value<QHostAddress>();
    quint16 port = list[1].toUInt();
    QByteArray data = list[2].toByteArray();
    logdebug <<address.toString()<<port<<data;
}
SQZOBJECT_NOARG(TestWidget)

void TestWidget::on_pushButton_2_clicked()
{

}
