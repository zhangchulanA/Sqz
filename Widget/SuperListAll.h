#ifndef SUPERLISTALL_H
#define SUPERLISTALL_H

#include <QListView>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QMap>
#include <QList>
#include <QString>
#include <QColor>
#include <functional>
#include <QJsonArray>
#include <QJsonObject>
#include "SuperTableAll.h"   // 复用 TableRowData / TableColumnConfig / TableCellType

namespace Sqz {

// ====================== SuperListModel (列表数据模型) ======================
/**
 * @brief 列表数据模型类
 * 继承自QAbstractListModel，与SuperTableModel保持完全一致的数据管理逻辑
 * 核心职责：管理原始数据和展示索引、处理筛选、提供数据读写、通知视图刷新
 */
class SuperListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QJsonArray jsonData READ getJsonData WRITE setJsonData NOTIFY jsonDataChanged)

public:
    explicit SuperListModel(QObject *parent = nullptr);

    // ---------- 与SuperTableModel完全对齐的接口 ----------
    void setColumns(const QList<TableColumnConfig>& cols);
    QList<TableColumnConfig> getColumns() const;
    void clearAllData();
    void appendRows(const QList<TableRowData>& rows);
    TableRowData getRow(int idx) const;
    QList<TableRowData> getVisibleRows() const;           // 获取所有可见行（筛选后）
    QList<TableRowData> getAllOriginRows() const;         // 获取所有原始数据行
    void setFilterText(const QString& colKey, const QString& filter);
    void clearFilter();
    void setRowColorRule(const RowColorFunc& func);

    // ---------- JSON序列化 ----------
    QJsonArray getJsonData() const;
    void setJsonData(const QJsonArray& data);
    QJsonObject getRowJson(int rowIndex) const;
    bool setRowJson(int rowIndex, const QJsonObject& data);

    // ---------- 查找与更新 ----------
    QList<int> findRowsByColumn(const QString& colKey, const QString& value) const;
    QList<int> findRowsByColumnExact(const QString& colKey, const QString& value) const;
    bool updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue);
    int updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                           const QMap<QString, QVariant>& updates);
    int updateRowsByColumnExact(const QString& searchColKey, const QString& searchValue,
                                const QMap<QString, QVariant>& updates);

    // ---------- 重写 QAbstractListModel 接口 ----------
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void jsonDataChanged();

private:
    // 内部辅助函数（与SuperTableModel完全一致）
    void doFilter();
    bool rowMatchesFilter(const TableRowData& row) const;
    QJsonObject rowToJson(const TableRowData& row) const;
    TableRowData jsonToRow(const QJsonObject& json) const;

    // ★ 单一存储结构（与SuperTableModel完全一致）
    QList<TableRowData> m_originData;    // 原始数据（唯一数据源）
    QList<int>          m_showIndex;     // 展示索引映射
    TableColumnConfig   m_columnConfig;  // 单列配置（列表只用一列）
    QString m_filterCol;                 // 筛选列名
    QString m_filterText;                // 筛选关键词
    RowColorFunc m_rowColorFunc;         // 行背景色回调
};

// ====================== SuperListDelegate (列表绘制委托) ======================
/**
 * @brief 列表项绘制代理
 * 与SuperTableDelegate的绘制逻辑一致，仅适配QListView的绘制上下文
 */
class SuperListDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SuperListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

    void setItemHeight(int h) { m_itemHeight = h; }

private:
    void drawText(QPainter* p, const QStyleOptionViewItem& opt, const QString& text) const;
    void drawCheckBox(QPainter* p, const QStyleOptionViewItem& opt, bool checked) const;
    void drawProgress(QPainter* p, const QStyleOptionViewItem& opt, int val, int max) const;
    void drawStateTag(QPainter* p, const QStyleOptionViewItem& opt, const QString& text) const;

    int m_itemHeight = 32;
};

// ====================== SuperListWidget (外层封装控件) ======================
/**
 * @brief 超级列表控件（基于QListView + SuperListModel）
 * 与SuperTableWidget拥有完全一致的对外接口和内部MVC分层
 */
class SuperListWidget : public QListView
{
    Q_OBJECT
    Q_PROPERTY(QJsonArray listData READ getListData WRITE setListData NOTIFY listDataChanged)
    Q_PROPERTY(int rowCount READ getRowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QJsonArray selectedRows READ getSelectedRowsJson NOTIFY SelectionChanged)

public:
    explicit SuperListWidget(QWidget *parent = nullptr);

    // ---------- 与SuperTableWidget完全对齐的接口 ----------
    void setHeaders(const QList<TableColumnConfig>& cols);
    void clearData();
    void addRows(const QList<TableRowData>& rows);
    void setRowColorRule(const RowColorFunc& func);
    void filterColumn(const QString &colKey, const QString &text);
    void clearFilter();
    QList<TableRowData> getSelectedRows() const;
    TableRowData getRow(int index) const;

    // JSON 属性
    QJsonArray getListData() const;
    void setListData(const QJsonArray& data);
    QJsonArray getSelectedRowsJson() const;
    int getRowCount() const { return m_model->rowCount(); }

    // 尺寸控制
    void setItemHeight(int h);
    void setListSize(int w, int h);
    void setListMinSize(int w, int h);
    void setListStyleSheet(const QString &qss);

    // 查找与更新
    QList<int> findRowsByColumn(const QString& colKey, const QString& value) const;
    int updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                           const QMap<QString, QVariant>& updates);
    bool updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue);

signals:
    void listDataChanged();
    void rowCountChanged();
    void SelectionChanged();
    void rowClickedIndex(const TableRowData& rowData, int rowIndex);
    void rowDoubleClicked(const TableRowData& rowData, int rowIndex);

protected:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

private slots:
    void onModelDataChanged();
    void onRowsInserted();

private:
    SuperListModel*   m_model;
    SuperListDelegate* m_delegate;
};

} // namespace Sqz
#endif // SUPERLISTALL_H
