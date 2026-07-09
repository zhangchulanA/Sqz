#ifndef SUPERTABLEALL_H
#define SUPERTABLEALL_H

#include <QTableView>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QMap>
#include <QList>
#include <QString>
#include <QColor>
#include <functional>
#include "SqzGlobal.h"
/**
 * @brief 超级表格组件总头文件
 * 该组件是基于Qt的QTableView封装的高性能、高扩展表格控件，包含数据模型(SuperTableModel)、绘制代理(SuperTableDelegate)、外层封装控件(SuperTableWidget)三部分
 *
 * 核心特点：
 * 1. 多类型单元格支持：文本、复选框、进度条、状态标签色块，无需自定义控件即可实现复杂单元格展示
 * 2. 数据筛选功能：支持按指定列关键词模糊筛选，大小写不敏感
 * 3. 自定义行颜色：通过回调函数实现行级别的背景色自定义，支持动态染色
 * 4. 高性能数据处理：分离原始数据和展示数据，筛选时仅刷新展示数据，减少重绘开销
 * 5. 灵活的尺寸控制：支持全局行高、单列宽度、表格整体尺寸、表头高度等精细化尺寸配置
 * 6. 便捷的交互支持：复选框单元格可直接点击切换状态，选中行获取、多行选择等交互优化
 * 7. 样式自定义：支持通过QSS设置表格整体样式，兼容Qt样式体系
 * 8. 数据编辑支持：单元格可编辑，编辑后自动同步原始数据，避免数据不一致
 */

// ====================== 基础数据结构 ======================
/**
 * @brief 单元格类型枚举
 * 定义表格支持的所有单元格展示类型
 */
namespace Sqz::Widget {
enum class SQZ_FRAMEWORK_API TableCellType
{
    Text,        // 普通文本单元格（默认类型）
    CheckBox,    // 复选框单元格（展示勾选/未勾选状态，支持点击切换）
    Progress,    // 进度条单元格（展示百分比进度，含进度数值文本）
    StateTag     // 状态标签色块（不同文本对应不同颜色的圆角矩形标签）
};

/**
 * @brief 表格单行数据结构
 * 存储一行中所有列的键值对数据，提供便捷的get/set方法访问指定列数据
 */
struct SQZ_FRAMEWORK_API TableRowData
{
    QMap<QString, QVariant> cells; // 列名-值 映射表，存储该行所有列数据

    /**
     * @brief 获取指定列名对应的单元格值
     * @param key 列名（对应TableColumnConfig的name字段）
     * @return 单元格值，若列名不存在返回空QVariant
     */
    QVariant get(const QString& key) const { return cells.value(key); }

    /**
     * @brief 设置指定列名的单元格值
     * @param key 列名（对应TableColumnConfig的name字段）
     * @param val 要设置的单元格值
     */
    void set(const QString& key, const QVariant& val) { cells[key] = val; }
};

/**
 * @brief 表格列配置结构
 * 定义每一列的属性，包括名称、标题、类型、宽度、是否隐藏、是否可排序等
 */
struct SQZ_FRAMEWORK_API TableColumnConfig
{
    QString name;          // 列唯一标识（对应TableRowData中的key）
    QString title;         // 列表头显示文本
    TableCellType type = TableCellType::Text; // 单元格类型，默认普通文本
    int width = 120;       // 列默认宽度，单位像素
    bool hidden = false;   // 是否隐藏该列，默认不隐藏
    bool sortable = true;  // 是否支持排序，默认支持（当前版本暂未实现排序逻辑）
};

/**
 * @brief 行背景色回调函数类型
 * 用于自定义行背景色，入参为当前行数据，返回该行的背景色（无效颜色则使用默认背景）
 */
using RowColorFunc = std::function<QColor(const TableRowData&)>;

// ====================== Model 数据模型 ======================
/**
 * @brief 表格数据模型类
 * 继承自QAbstractTableModel，实现Qt MVC架构中的数据模型层，负责数据存储、筛选、编辑、数据交互等核心逻辑
 * 核心职责：管理原始数据和展示数据、处理筛选逻辑、提供数据读写接口、通知视图数据变更
 */
class SQZ_FRAMEWORK_API SuperTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象，用于Qt父子对象内存管理
     */
    explicit SuperTableModel(QObject *parent = nullptr);

    /**
     * @brief 设置表格列配置
     * @param cols 列配置列表，会重置模型并刷新视图
     */
    void setColumns(const QList<TableColumnConfig>& cols);

    /**
     * @brief 获取当前列配置列表
     * @return 列配置列表的拷贝
     */
    QList<TableColumnConfig> getColumns() const;

    /**
     * @brief 清空所有表格数据（原始数据+展示数据）
     * 会触发模型重置，视图同步清空
     */
    void clearAllData();

    /**
     * @brief 追加行数据到表格
     * @param rows 要追加的行数据列表，空列表则不处理
     * 会触发视图行插入通知，仅刷新新增行区域，性能优化
     */
    void appendRows(const QList<TableRowData>& rows);

    /**
     * @brief 获取指定行的展示数据
     * @param idx 行索引（基于展示数据的索引，非原始数据）
     * @return 该行数据，索引越界返回空TableRowData
     */
    TableRowData getRow(int idx) const;

    /**
     * @brief 获取所有展示数据行
     * @return 展示数据列表（筛选后的结果）
     */
    QList<TableRowData> getAllRows() const;

    /**
     * @brief 设置列筛选条件
     * @param colKey 要筛选的列名（对应TableColumnConfig的name字段）
     * @param filter 筛选关键词，大小写不敏感，模糊匹配
     * 会触发模型重置，重新筛选数据并刷新视图
     */
    void setFilterText(const QString& colKey, const QString& filter);

    /**
     * @brief 清空筛选条件
     * 会触发模型重置，展示所有原始数据
     */
    void clearFilter();

    /**
     * @brief 设置行背景色自定义回调函数
     * @param func 行染色回调函数，入参为行数据，返回背景色
     * 设置后每行绘制时会调用该函数，返回有效颜色则使用该颜色作为行背景
     */
    void setRowColorRule(const RowColorFunc& func);

    /**
     * @brief 重写QAbstractTableModel接口：获取行数
     * @param parent 父索引（树形结构用，表格中无效）
     * @return 展示数据的行数
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 重写QAbstractTableModel接口：获取列数
     * @param parent 父索引（树形结构用，表格中无效）
     * @return 列配置的列数
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 重写QAbstractTableModel接口：获取单元格数据
     * @param index 单元格索引（行+列）
     * @param role 数据角色（展示、用户自定义、背景色等）
     * @return 对应角色的单元格数据，索引越界返回空QVariant
     * 支持的角色：
     * - Qt::DisplayRole：单元格展示文本
     * - Qt::UserRole：单元格类型（TableCellType枚举值）
     * - Qt::BackgroundRole：行背景色（若设置了行染色回调）
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief 重写QAbstractTableModel接口：获取表头数据
     * @param section 表头列索引
     * @param orientation 表头方向（仅处理水平表头）
     * @param role 数据角色（仅处理展示角色）
     * @return 列表头显示文本，索引越界返回父类默认值
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief 重写QAbstractTableModel接口：获取单元格标志
     * @param index 单元格索引
     * @return 单元格标志（可选中、可启用、可编辑）
     */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /**
     * @brief 重写QAbstractTableModel接口：设置单元格数据
     * @param index 单元格索引
     * @param value 要设置的新值
     * @param role 数据角色（仅处理编辑角色）
     * @return 是否设置成功
     * 编辑后会同步更新展示数据和原始数据（同索引），并触发数据变更通知
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
private:
    /**
     * @brief 执行数据筛选逻辑
     * 根据当前设置的筛选列和筛选关键词，从原始数据中筛选出匹配的行到展示数据
     * 筛选规则：列值包含关键词（大小写不敏感），无筛选条件则展示所有原始数据
     */
    void doFilter();

    /**
     * @brief 根据列索引获取列配置
     * @param sec 列索引
     * @return 列配置，索引越界返回空配置
     */
    TableColumnConfig getColumnBySection(int sec) const;

    QList<TableRowData> m_originData;    // 原始数据（未筛选的完整数据）
    QList<TableRowData> m_showData;      // 展示数据（筛选后的结果）
    QList<TableColumnConfig> m_columns;  // 列配置列表
    QString m_filterCol;                 // 筛选列名
    QString m_filterText;                // 筛选关键词
    RowColorFunc m_rowColorFunc;         // 行背景色回调函数
};

// ====================== Delegate 绘制代理 ======================
/**
 * @brief 表格绘制代理类
 * 继承自QStyledItemDelegate，负责单元格的自定义绘制和交互处理
 * 核心职责：实现不同类型单元格（复选框、进度条、状态标签）的绘制逻辑，处理复选框点击交互
 */
class SQZ_FRAMEWORK_API SuperTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象，用于Qt父子对象内存管理
     */
    explicit SuperTableDelegate(QObject *parent = nullptr);

    /**
     * @brief 重写QStyledItemDelegate接口：绘制单元格
     * @param painter 绘制器对象
     * @param option 单元格样式选项（位置、状态、大小等）
     * @param index 单元格索引
     * 按单元格类型分别绘制：文本、复选框、进度条、状态标签，同时处理选中状态和自定义背景色
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 重写QStyledItemDelegate接口：获取单元格尺寸提示
     * @param option 单元格样式选项
     * @param index 单元格索引
     * @return 单元格推荐尺寸（固定行高32像素，宽度使用默认）
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 重写QStyledItemDelegate接口：处理编辑器事件（鼠标/键盘事件）
     * @param event 事件对象
     * @param model 数据模型
     * @param option 单元格样式选项
     * @param index 单元格索引
     * @return 是否消费该事件（true=阻止默认处理，false=放行）
     * 仅处理复选框单元格的鼠标左键按下事件，实现点击切换勾选状态
     */
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;
private:
    /**
     * @brief 绘制普通文本单元格
     * @param p 绘制器对象
     * @param opt 单元格样式选项
     * @param text 要绘制的文本
     * 文本左对齐、垂直居中，左右留8像素边距，上下留4像素边距
     */
    void drawText(QPainter* p, const QStyleOptionViewItem& opt, const QString& text) const;

    /**
     * @brief 绘制复选框单元格
     * @param p 绘制器对象
     * @param opt 单元格样式选项
     * @param checked 是否勾选
     * 复选框居中显示，尺寸16x16像素，勾选状态绘制蓝色对勾
     */
    void drawCheckBox(QPainter* p, const QStyleOptionViewItem& opt, bool checked) const;

    /**
     * @brief 绘制进度条单元格
     * @param p 绘制器对象
     * @param opt 单元格样式选项
     * @param val 当前进度值
     * @param max 进度最大值（默认100）
     * 进度条外框+蓝色填充进度，居中显示百分比文本，上下左右留8像素边距
     */
    void drawProgress(QPainter* p, const QStyleOptionViewItem& opt, int val, int max) const;

    /**
     * @brief 绘制状态标签单元格
     * @param p 绘制器对象
     * @param opt 单元格样式选项
     * @param text 状态文本
     * 不同状态对应不同颜色：正常(绿色)、失败(红色)、等待(黄色)、其他(灰色)，圆角矩形（半径4像素），白色居中文本
     */
    void drawStateTag(QPainter* p, const QStyleOptionViewItem& opt, const QString& text) const;
};

// ====================== 外层封装控件 SuperTableWidget ======================
/**
 * @brief 表格外层封装控件
 * 继承自QTableView，整合数据模型和绘制代理，提供简洁的对外接口，封装底层实现细节
 * 核心职责：对外提供统一的表格操作接口，初始化表格默认样式和行为，简化上层使用
 */
class SQZ_FRAMEWORK_API SuperTableWidget : public QTableView
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口/控件，用于Qt父子对象内存管理
     * 初始化数据模型和绘制代理，设置表格默认样式和交互行为
     */
    explicit SuperTableWidget(QWidget *parent = nullptr);

    /**
     * @brief 设置表格列配置和表头
     * @param cols 列配置列表
     * 同步设置模型的列配置，初始化列宽度，隐藏需要隐藏的列
     */
    void setHeaders(const QList<TableColumnConfig>& cols);

    /**
     * @brief 清空表格所有数据
     * 调用模型的清空接口，视图同步清空
     */
    void clearData();

    /**
     * @brief 追加行数据到表格
     * @param rows 要追加的行数据列表
     * 调用模型的追加接口，简化上层调用
     */
    void addRows(const QList<TableRowData>& rows);

    /**
     * @brief 设置行背景色自定义规则
     * @param func 行染色回调函数
     * 调用模型的设置接口，简化上层调用
     */
    void setRowColorRule(const RowColorFunc& func);

    /**
     * @brief 按指定列筛选数据
     * @param colKey 要筛选的列名
     * @param text 筛选关键词
     * 调用模型的筛选接口，简化上层调用
     */
    void filterColumn(const QString &colKey, const QString &text);

    /**
     * @brief 清空筛选条件
     * 调用模型的清空筛选接口，简化上层调用
     */
    void clearFilter();

    /**
     * @brief 获取选中的行数据
     * @return 选中行的展示数据列表
     * 支持多行选择，返回所有选中行的数据
     */
    QList<TableRowData> getSelectedRows() const;

    // ========== 尺寸控制接口 ==========
    /**
     * @brief 设置全局统一行高
     * @param h 行高，单位像素
     * 所有行使用统一高度，替代默认行高
     */
    void setGlobalRowHeight(int h);

    /**
     * @brief 设置指定列的宽度
     * @param colIdx 列索引（从0开始）
     * @param w 列宽度，单位像素
     * 索引无效则不处理
     */
    void setColWidth(int colIdx, int w);

    /**
     * @brief 设置表格整体固定尺寸
     * @param w 表格宽度，单位像素
     * @param h 表格高度，单位像素
     * 表格控件本身使用固定尺寸，不再自适应父控件
     */
    void setTableSize(int w, int h);

    /**
     * @brief 设置表格最小尺寸
     * @param w 最小宽度，单位像素
     * @param h 最小高度，单位像素
     * 表格尺寸不会小于该值，可自适应更大尺寸
     */
    void setTableMinSize(int w, int h);

    /**
     * @brief 设置表头高度
     * @param h 表头高度，单位像素
     * 仅设置水平表头高度
     */
    void setHeaderHeight(int h);

    /**
     * @brief 设置表格样式表
     * @param qss Qt样式表字符串
     * 支持通过QSS自定义表格样式（如网格线、选中色、字体等）
     */
    void setTableStyleSheet(const QString &qss);

private:
    SuperTableModel* m_model;       // 表格数据模型
    SuperTableDelegate* m_delegate; // 表格绘制代理
};
}
#endif // SUPERTABLEALL_H


#if A
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
#endif

