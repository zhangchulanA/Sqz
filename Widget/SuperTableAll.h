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
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
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
 * 9. JSON序列化支持：通过Q_PROPERTY属性实现表格数据的JSON序列化读写，方便与QML/JS交互及数据持久化
 */

// ====================== 基础数据结构 ======================
/**
 * @brief 单元格类型枚举
 * 定义表格支持的所有单元格展示类型
 */
namespace Sqz {
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
    /**
     * @brief jsonData 属性：表格所有原始数据的JSON序列化表示
     * 读取时返回当前所有原始数据（未筛选）的JSON数组
     * 写入时清空现有数据并解析JSON数组填充，同时重新应用筛选条件
     * 该属性方便与QML/JS交互，以及数据的导入导出和持久化存储
     */
    Q_PROPERTY(QJsonArray jsonData READ getJsonData WRITE setJsonData NOTIFY jsonDataChanged)

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

    // ========================== JSON序列化接口 ==========================
    /**
     * @brief 获取所有原始数据的JSON数组表示
     * @return 包含所有原始数据行的QJsonArray，每行数据转换为QJsonObject
     * @note 返回的是原始数据（未筛选），通过Q_PROPERTY暴露为jsonData属性
     * @see setJsonData()
     */
    QJsonArray getJsonData() const;

    /**
     * @brief 从JSON数组设置表格数据
     * @param data 包含行数据的QJsonArray，每个元素为QJsonObject
     * 会清空现有数据并填充新数据，重新应用筛选条件，触发模型重置和属性变更通知
     * @note 通过Q_PROPERTY暴露为jsonData属性，支持QML直接赋值
     * @see getJsonData()
     */
    void setJsonData(const QJsonArray& data);

    /**
     * @brief 获取指定展示行的JSON对象表示
     * @param rowIndex 展示数据中的行索引
     * @return 该行数据的QJsonObject，索引越界返回空对象
     * @note 用于单行数据的快速读取，不影响整体属性
     */
    QJsonObject getRowJson(int rowIndex) const;

    /**
     * @brief 通过原始数据索引获取行的JSON对象
     * @param originIdx 原始数据中的索引（非展示索引）
     * @return 该行数据的QJsonObject，索引越界返回空对象
     * @note 用于内部操作和特殊场景
     */
    QJsonObject getRowJsonByOriginIndex(int originIdx) const;

    /**
     * @brief 更新指定展示行的数据（通过JSON对象）
     * @param rowIndex 展示数据中的行索引
     * @param data 新的行数据JSON对象
     * @return 是否更新成功
     * @note 更新后会触发视图刷新和数据变更通知
     */
    bool setRowJson(int rowIndex, const QJsonObject& data);

    // ========================== 查找和更新功能 ==========================
    /**
     * @brief 根据指定列的值查找匹配的行索引（展示数据中的索引）
     * @param colKey 列名
     * @param value 要匹配的值
     * @return 匹配的行索引列表（展示数据中的行号），未找到返回空列表
     * @note 支持模糊匹配（值包含），大小写不敏感
     */
    QList<int> findRowsByColumn(const QString& colKey, const QString& value) const;

    /**
     * @brief 根据指定列的值精确查找匹配的行索引
     * @param colKey 列名
     * @param value 要精确匹配的值
     * @return 匹配的行索引列表（展示数据中的行号），未找到返回空列表
     */
    QList<int> findRowsByColumnExact(const QString& colKey, const QString& value) const;

    /**
     * @brief 更新指定展示行中某列的数据
     * @param rowIndex 展示数据中的行索引
     * @param colKey 要更新的列名
     * @param newValue 新值
     * @return 是否更新成功
     */
    bool updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue);

    /**
     * @brief 根据某列值查找并更新该行中其他列的数据
     * @param searchColKey 用于查找的列名
     * @param searchValue 查找值
     * @param updates 要更新的列名-新值映射
     * @return 成功更新的行数
     */
    int updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                           const QMap<QString, QVariant>& updates);

    /**
     * @brief 根据某列值精确查找并更新该行中其他列的数据
     * @param searchColKey 用于查找的列名
     * @param searchValue 查找值（精确匹配）
     * @param updates 要更新的列名-新值映射
     * @return 成功更新的行数
     */
    int updateRowsByColumnExact(const QString& searchColKey, const QString& searchValue,
                                const QMap<QString, QVariant>& updates);

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

signals:
    /**
     * @brief JSON数据变更信号
     * 当通过setJsonData()或任何修改原始数据的操作导致JSON表示变化时触发
     * @note 用于Q_PROPERTY的NOTIFY信号，支持属性绑定
     */
    void jsonDataChanged();

private:
    /**
     * @brief 执行数据筛选逻辑
     * 重建 m_showIndex 映射数组，根据筛选条件从原始数据中筛选出匹配的行索引
     * 筛选规则：列值包含关键词（大小写不敏感），无筛选条件则展示所有原始数据
     */
    void doFilter();

    /**
     * @brief 判断单行数据是否匹配当前筛选条件
     * @param row 待检查的行数据
     * @return true 表示匹配（应展示），false 表示不匹配
     * @note 纯函数，doFilter 和 appendRows 共用，确保筛选逻辑一致性
     */
    bool rowMatchesFilter(const TableRowData& row) const;

    /**
     * @brief 根据列索引获取列配置
     * @param sec 列索引
     * @return 列配置，索引越界返回空配置
     */
    TableColumnConfig getColumnBySection(int sec) const;

    /**
     * @brief 将TableRowData转换为QJsonObject
     * @param row 待转换的行数据
     * @return 对应的JSON对象，键为列名，值为对应的数据
     * @note 支持String、Int、Double、Bool等基本类型，其他类型转为字符串
     * @see jsonToRow()
     */
    QJsonObject rowToJson(const TableRowData& row) const;

    /**
     * @brief 将QJsonObject转换为TableRowData
     * @param json 待转换的JSON对象
     * @return 对应的行数据
     * @note 支持String、Double、Bool等JSON基本类型，Null值会被跳过
     * @see rowToJson()
     */
    TableRowData jsonToRow(const QJsonObject& json) const;

    // ★ 单一存储结构：m_originData 是唯一数据源，m_showIndex 是展示索引映射
    // 修复：移除 m_showData 副本，消除"双重存储需手动同步"的 bug 温床
    // data()/setData()/getRow() 等全部通过 m_showIndex 间接访问 m_originData
    QList<TableRowData> m_originData;    // 原始数据（唯一数据源，single source of truth）
    QList<int>           m_showIndex;     // 展示索引映射：m_showIndex[displayRow] = m_originData 下标
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

    /**
     * @brief 创建编辑器
     * @param parent 父控件
     * @param option 样式选项
     * @param index 单元格索引
     * @return 编辑器控件
     */
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 设置编辑器数据
     * @param editor 编辑器控件
     * @param index 单元格索引
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief 设置模型数据
     * @param editor 编辑器控件
     * @param model 数据模型
     * @param index 单元格索引
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

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
    /**
     * @brief tableData 属性：表格所有原始数据的JSON序列化表示
     * 直接转发到模型的jsonData属性，方便QML和外部代码通过属性方式读写表格数据
     * 读取时获取所有原始数据的JSON数组，写入时清空并填充新数据
     * @see SuperTableModel::jsonData
     */
    Q_PROPERTY(QJsonArray tableData READ getTableData WRITE setTableData NOTIFY tableDataChanged)

    /**
     * @brief rowCount 属性：当前表格展示的行数（只读）
     * 返回筛选后的展示数据行数，当数据变化时自动更新
     * @note 只读属性，用于QML绑定和外部查询
     */
    Q_PROPERTY(int rowCount READ getRowCount NOTIFY rowCountChanged)

    /**
     * @brief selectedRows 属性：当前选中的行数据（只读）
     * 返回所有选中行的JSON数组表示，支持多行选择
     * @note 只读属性，当选择变化时自动更新
     */
    Q_PROPERTY(QJsonArray selectedRows READ getSelectedRowsJson NOTIFY SelectionChanged)

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

    /**
     * @brief getRow 获取某一行数据
     * @param index 展示数据中的行索引
     * @return 该行数据
     */
    TableRowData getRow(int index);

    // ========== JSON序列化属性接口 ==========
    /**
     * @brief 获取表格所有原始数据的JSON数组
     * @return 包含所有原始数据的QJsonArray
     * @note 对应Q_PROPERTY的READ函数，返回原始数据（未筛选）
     * @see setTableData()
     */
    QJsonArray getTableData() const;

    /**
     * @brief 从JSON数组设置表格数据
     * @param data 包含行数据的JSON数组
     * 转发到模型的setJsonData()，会清空现有数据并填充新数据
     * @note 对应Q_PROPERTY的WRITE函数
     * @see getTableData()
     */
    void setTableData(const QJsonArray& data);

    /**
     * @brief 获取当前选中行的JSON数组表示
     * @return 所有选中行的QJsonArray，每行数据为QJsonObject
     * @note 对应Q_PROPERTY的READ函数（只读属性），支持多行选择
     */
    QJsonArray getSelectedRowsJson() const;

    /**
     * @brief 获取当前展示数据的行数
     * @return 筛选后的行数
     * @note 对应Q_PROPERTY的READ函数（只读属性）
     */
    int getRowCount() const { return m_model->rowCount(); }

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

    // ========== 查找和更新功能 ==========
    /**
     * @brief 根据列值查找匹配的行索引（模糊匹配）
     * @param colKey 列名
     * @param value 要匹配的值
     * @return 匹配的行索引列表
     */
    QList<int> findRowsByColumn(const QString& colKey, const QString& value) const;

    /**
     * @brief 根据某列值查找并更新该行中其他列的数据（模糊匹配）
     * @param searchColKey 用于查找的列名
     * @param searchValue 查找值
     * @param updates 要更新的列名-新值映射
     * @return 成功更新的行数
     */
    int updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                           const QMap<QString, QVariant>& updates);

    /**
     * @brief 更新指定行的某列数据
     * @param rowIndex 行索引
     * @param colKey 列名
     * @param newValue 新值
     * @return 是否更新成功
     */
    bool updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue);

signals:
    /**
     * @brief 表格数据变更信号
     * 当通过setTableData()或模型数据发生变化时触发
     * @note 对应Q_PROPERTY的NOTIFY信号，支持属性绑定
     */
    void tableDataChanged();

    /**
     * @brief 行数变更信号
     * 当表格行数（筛选后）发生变化时触发
     * @note 对应Q_PROPERTY的NOTIFY信号，支持属性绑定
     */
    void rowCountChanged();

    /**
     * @brief 选中行变更信号
     * 重新定义QTableView的SelectionChanged，适配Q_PROPERTY的NOTIFY信号
     * @note 对应Q_PROPERTY的NOTIFY信号，支持属性绑定
     */
    void SelectionChanged();

    /**
     * @brief 单击某行时触发的信号
     * @param rowData 被单击行的完整数据
     * @param rowIndex 被单击行的索引（展示数据中的行号）
     */
    void rowClickedIndex(const TableRowData& rowData, int rowIndex);

//    /**
//     * @brief 单击某行时触发的信号（仅包含行数据）
//     * @param rowData 被单击行的完整数据
//     */
//    void rowClicked(const TableRowData& rowData);

    /**
     * @brief 双击某行时触发的信号
     * @param rowData 被双击行的完整数据
     * @param rowIndex 被双击行的索引
     */
    void rowDoubleClicked(const TableRowData& rowData, int rowIndex);

protected:
    /**
     * @brief 重写选中变更事件，转发SelectionChanged信号
     * @param selected 新选中的项目
     * @param deselected 取消选中的项目
     * 调用父类处理，然后发射SelectionChanged信号通知属性绑定
     */
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

private slots:
    /**
     * @brief 行数变更槽函数
     * 当模型的行数发生变化时触发，发射rowCountChanged信号
     */
    void onRowCountChanged();

private:
    /**
     * @brief 内部方法，用于发送行点击信号
     */
    void emitRowClickedSignal(const QModelIndex& index);

    /**
     * @brief 单击事件槽函数
     */
    void onClicked(const QModelIndex& index);

    /**
     * @brief 双击事件槽函数
     */
    void onDoubleClicked(const QModelIndex& index);

private:
    SuperTableModel* m_model;       // 表格数据模型
    SuperTableDelegate* m_delegate; // 表格绘制代理
};
}
#endif // SUPERTABLEALL_H
