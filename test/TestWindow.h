// main.cpp
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "SuperTableAll.h"

using namespace Sqz;

class TestWindow : public QMainWindow
{
    Q_OBJECT
public:
    TestWindow(QWidget* parent = nullptr) : QMainWindow(parent)
    {
        setupUI();
        setupConnections();
        initData();
    }

private:
    SuperTableWidget* m_table = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QLineEdit* m_findEdit = nullptr;
    QLineEdit* m_updateEdit = nullptr;

    void setupUI()
    {
        QWidget* central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout* mainLayout = new QVBoxLayout(central);

        // 表格控件
        m_table = new SuperTableWidget(this);
        m_table->setGlobalRowHeight(40);
        m_table->setTableMinSize(600, 300);
        mainLayout->addWidget(m_table);

        // 状态栏
        m_statusLabel = new QLabel("就绪", this);
        mainLayout->addWidget(m_statusLabel);

        // 控制面板
        QGroupBox* controlBox = new QGroupBox("控制面板", this);
        QVBoxLayout* controlLayout = new QVBoxLayout(controlBox);

        // 第一行：筛选
        QHBoxLayout* row1 = new QHBoxLayout();
        row1->addWidget(new QLabel("筛选(列:status):"));
        m_filterEdit = new QLineEdit(this);
        m_filterEdit->setPlaceholderText("输入关键词筛选...");
        row1->addWidget(m_filterEdit);
        QPushButton* btnFilter = new QPushButton("筛选", this);
        row1->addWidget(btnFilter);
        QPushButton* btnClearFilter = new QPushButton("清除筛选", this);
        row1->addWidget(btnClearFilter);
        controlLayout->addLayout(row1);

        // 第二行：查找
        QHBoxLayout* row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("查找(列:name):"));
        m_findEdit = new QLineEdit(this);
        m_findEdit->setPlaceholderText("查找关键词...");
        row2->addWidget(m_findEdit);
        QPushButton* btnFind = new QPushButton("查找", this);
        row2->addWidget(btnFind);
        controlLayout->addLayout(row2);

        // 第三行：更新
        QHBoxLayout* row3 = new QHBoxLayout();
        row3->addWidget(new QLabel("更新(将name中包含):"));
        m_updateEdit = new QLineEdit(this);
        m_updateEdit->setPlaceholderText("查找关键词...");
        row3->addWidget(m_updateEdit);
        QPushButton* btnUpdate = new QPushButton("将status设为'已更新'", this);
        row3->addWidget(btnUpdate);
        controlLayout->addLayout(row3);

        // 第四行：操作按钮
        QHBoxLayout* row4 = new QHBoxLayout();
        QPushButton* btnAddRow = new QPushButton("追加行", this);
        row4->addWidget(btnAddRow);
        QPushButton* btnClearData = new QPushButton("清空数据", this);
        row4->addWidget(btnClearData);
        QPushButton* btnPrintJson = new QPushButton("打印JSON", this);
        row4->addWidget(btnPrintJson);
        QPushButton* btnGetSelected = new QPushButton("获取选中行", this);
        row4->addWidget(btnGetSelected);
        controlLayout->addLayout(row4);

        mainLayout->addWidget(controlBox);

        // 连接信号
        connect(btnFilter, &QPushButton::clicked, this, &TestWindow::onFilter);
        connect(btnClearFilter, &QPushButton::clicked, this, &TestWindow::onClearFilter);
        connect(btnFind, &QPushButton::clicked, this, &TestWindow::onFind);
        connect(btnUpdate, &QPushButton::clicked, this, &TestWindow::onUpdate);
        connect(btnAddRow, &QPushButton::clicked, this, &TestWindow::onAddRow);
        connect(btnClearData, &QPushButton::clicked, this, &TestWindow::onClearData);
        connect(btnPrintJson, &QPushButton::clicked, this, &TestWindow::onPrintJson);
        connect(btnGetSelected, &QPushButton::clicked, this, &TestWindow::onGetSelected);

        // 连接表格信号
        connect(m_table, &SuperTableWidget::rowClickedIndex, this, &TestWindow::onRowClicked);
        connect(m_table, &SuperTableWidget::rowDoubleClicked, this, &TestWindow::onRowDoubleClicked);
        connect(m_table, &SuperTableWidget::rowCountChanged, this, &TestWindow::onRowCountChanged);
        connect(m_table, &SuperTableWidget::SelectionChanged, this, &TestWindow::onSelectionChanged);
    }

    void setupConnections()
    {
        // 额外连接：筛选框回车触发筛选
        connect(m_filterEdit, &QLineEdit::returnPressed, this, &TestWindow::onFilter);
    }

    void initData()
    {
        // 定义列配置
        QList<TableColumnConfig> columns;

        TableColumnConfig col1;
        col1.name = "id";
        col1.title = "ID";
        col1.type = TableCellType::Text;
        col1.width = 60;
        columns.append(col1);

        TableColumnConfig col2;
        col2.name = "name";
        col2.title = "姓名";
        col2.type = TableCellType::Text;
        col2.width = 120;
        columns.append(col2);

        TableColumnConfig col3;
        col3.name = "age";
        col3.title = "年龄";
        col3.type = TableCellType::Text;
        col3.width = 80;
        columns.append(col3);

        TableColumnConfig col4;
        col4.name = "active";
        col4.title = "激活";
        col4.type = TableCellType::CheckBox;
        col4.width = 80;
        columns.append(col4);

        TableColumnConfig col5;
        col5.name = "progress";
        col5.title = "进度";
        col5.type = TableCellType::Progress;
        col5.width = 150;
        columns.append(col5);

        TableColumnConfig col6;
        col6.name = "status";
        col6.title = "状态";
        col6.type = TableCellType::StateTag;
        col6.width = 100;
        columns.append(col6);

        m_table->setHeaders(columns);

        // 准备测试数据
        QList<TableRowData> rows;

        // 使用JSON方式批量添加
        QJsonArray jsonData;

        QJsonObject row1;
        row1["id"] = 1;
        row1["name"] = "张三";
        row1["age"] = 25;
        row1["active"] = true;
        row1["progress"] = 75;
        row1["status"] = "正常";
        jsonData.append(row1);

        QJsonObject row2;
        row2["id"] = 2;
        row2["name"] = "李四";
        row2["age"] = 30;
        row2["active"] = false;
        row2["progress"] = 45;
        row2["status"] = "等待";
        jsonData.append(row2);

        QJsonObject row3;
        row3["id"] = 3;
        row3["name"] = "王五";
        row3["age"] = 28;
        row3["active"] = true;
        row3["progress"] = 100;
        row3["status"] = "正常";
        jsonData.append(row3);

        QJsonObject row4;
        row4["id"] = 4;
        row4["name"] = "赵六";
        row4["age"] = 35;
        row4["active"] = false;
        row4["progress"] = 20;
        row4["status"] = "失败";
        jsonData.append(row4);

        QJsonObject row5;
        row5["id"] = 5;
        row5["name"] = "孙七";
        row5["age"] = 22;
        row5["active"] = true;
        row5["progress"] = 60;
        row5["status"] = "正常";
        jsonData.append(row5);

        m_table->setTableData(jsonData);

        // 设置行颜色规则：状态为"失败"的行显示红色背景
        m_table->setRowColorRule([](const TableRowData& row) -> QColor {
            QString status = row.get("status").toString();
            if (status == "失败") {
                return QColor(255, 200, 200); // 浅红色
            }
            if (status == "等待") {
                return QColor(255, 240, 200); // 浅黄色
            }
            return QColor(); // 无效颜色，使用默认
        });

        updateStatus("数据加载完成，共 " + QString::number(m_table->getRowCount()) + " 行");
    }

private slots:
    void onFilter()
    {
        QString text = m_filterEdit->text().trimmed();
        if (text.isEmpty()) {
            m_table->clearFilter();
            updateStatus("已清除筛选");
        } else {
            m_table->filterColumn("status", text);
            updateStatus("筛选: '" + text + "', 结果: " + QString::number(m_table->getRowCount()) + " 行");
        }
    }

    void onClearFilter()
    {
        m_filterEdit->clear();
        m_table->clearFilter();
        updateStatus("已清除筛选，共 " + QString::number(m_table->getRowCount()) + " 行");
    }

    void onFind()
    {
        QString text = m_findEdit->text().trimmed();
        if (text.isEmpty()) {
            updateStatus("请输入查找关键词");
            return;
        }

        QList<int> indices = m_table->findRowsByColumn("name", text);
        if (indices.isEmpty()) {
            updateStatus("未找到包含 '" + text + "' 的行");
        } else {
            QStringList rows;
            for (int idx : indices) {
                rows << QString::number(idx);
            }
            updateStatus("找到行: " + rows.join(", "));

            // 高亮显示找到的第一行
            if (!indices.isEmpty()) {
                m_table->selectRow(indices.first());
            }
        }
    }

    void onUpdate()
    {
        QString text = m_updateEdit->text().trimmed();
        if (text.isEmpty()) {
            updateStatus("请输入查找关键词");
            return;
        }

        QMap<QString, QVariant> updates;
        updates["status"] = "已更新";

        int count = m_table->updateRowsByColumn("name", text, updates);
        updateStatus("更新了 " + QString::number(count) + " 行 (name包含 '" + text + "')");
    }

    void onAddRow()
    {
        static int counter = 100;
        counter++;

        TableRowData row;
        row.set("id", counter);
        row.set("name", "新用户" + QString::number(counter));
        row.set("age", 20 + (counter % 20));
        row.set("active", counter % 2 == 0);
        row.set("progress", counter % 100);
        row.set("status", (counter % 3 == 0) ? "正常" : (counter % 3 == 1) ? "等待" : "失败");

        m_table->addRows({row});
        updateStatus("已追加行 " + QString::number(counter) + ", 当前共 " +
                    QString::number(m_table->getRowCount()) + " 行");
    }

    void onClearData()
    {
        m_table->clearData();
        updateStatus("已清空所有数据");
    }

    void onPrintJson()
    {
        QJsonArray data = m_table->getTableData();
        QJsonDocument doc(data);
        QString jsonStr = doc.toJson(QJsonDocument::Indented);

        qDebug() << "========== Table JSON Data ==========";
        qDebug().noquote() << jsonStr;
        qDebug() << "======================================";

        updateStatus("已打印JSON到控制台 (共 " + QString::number(data.size()) + " 行)");
    }

    void onGetSelected()
    {
        QList<TableRowData> rows = m_table->getSelectedRows();
        if (rows.isEmpty()) {
            updateStatus("未选中任何行");
            return;
        }

        QStringList info;
        for (const auto& row : rows) {
            QString name = row.get("name").toString();
            QString status = row.get("status").toString();
            info << name + "(" + status + ")";
        }
        updateStatus("选中 " + QString::number(rows.size()) + " 行: " + info.join(", "));
    }

    void onRowClicked(const TableRowData& rowData, int rowIndex)
    {
        QString name = rowData.get("name").toString();
        QString status = rowData.get("status").toString();
        updateStatus("单击行 " + QString::number(rowIndex) + ": " + name + " [" + status + "]");
    }

    void onRowDoubleClicked(const TableRowData& rowData, int rowIndex)
    {
        QString name = rowData.get("name").toString();
        QString status = rowData.get("status").toString();
        updateStatus("双击行 " + QString::number(rowIndex) + ": " + name + " [" + status + "]");
    }

    void onRowCountChanged()
    {
        updateStatus("行数变更: " + QString::number(m_table->getRowCount()) + " 行");
    }

    void onSelectionChanged()
    {
        // 仅更新状态，避免与getSelected冲突
        int count = m_table->getSelectedRows().size();
        if (count > 0) {
            m_statusLabel->setText(m_statusLabel->text() + " | 已选 " + QString::number(count) + " 行");
        }
    }

    void updateStatus(const QString& msg)
    {
        m_statusLabel->setText(msg);
    }
};

