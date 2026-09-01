#include "SuperListAll.h"
#include <QPainter>
#include <QMouseEvent>
#include <QJsonArray>
#include <QJsonObject>

namespace Sqz {

// ====================== SuperListModel 实现 ======================
SuperListModel::SuperListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void SuperListModel::setColumns(const QList<TableColumnConfig>& cols)
{
    if (cols.isEmpty()) return;
    m_columnConfig = cols.first();  // 列表只取第一列
    // 重置模型（清空数据）
    beginResetModel();
    m_originData.clear();
    m_showIndex.clear();
    endResetModel();
    emit jsonDataChanged();
}

QList<TableColumnConfig> SuperListModel::getColumns() const
{
    return {m_columnConfig};
}

void SuperListModel::clearAllData()
{
    beginResetModel();
    m_originData.clear();
    m_showIndex.clear();
    endResetModel();
    emit jsonDataChanged();
}

void SuperListModel::appendRows(const QList<TableRowData>& rows)
{
    if (rows.isEmpty()) return;

    int originStart = m_originData.size();
    m_originData.append(rows);

    // 增量筛选
    QList<int> visibleNewIndices;
    visibleNewIndices.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        if (rowMatchesFilter(rows[i]))
            visibleNewIndices.append(originStart + i);
    }

    if (visibleNewIndices.isEmpty()) return;

    const int first = m_showIndex.size();
    const int last  = first + visibleNewIndices.size() - 1;
    beginInsertRows(QModelIndex(), first, last);
    m_showIndex.append(visibleNewIndices);
    endInsertRows();
    emit jsonDataChanged();
}

TableRowData SuperListModel::getRow(int idx) const
{
    if (idx < 0 || idx >= m_showIndex.size()) return {};
    return m_originData[m_showIndex[idx]];
}

QList<TableRowData> SuperListModel::getVisibleRows() const
{
    QList<TableRowData> result;
    result.reserve(m_showIndex.size());
    for (int idx : m_showIndex)
        result.append(m_originData[idx]);
    return result;
}

QList<TableRowData> SuperListModel::getAllOriginRows() const
{
    return m_originData;
}

void SuperListModel::setFilterText(const QString& colKey, const QString& filter)
{
    m_filterCol = colKey;
    m_filterText = filter.trimmed();
    beginResetModel();
    doFilter();
    endResetModel();
    emit jsonDataChanged();
}

void SuperListModel::clearFilter()
{
    m_filterCol.clear();
    m_filterText.clear();
    beginResetModel();
    doFilter();
    endResetModel();
    emit jsonDataChanged();
}

void SuperListModel::setRowColorRule(const RowColorFunc& func)
{
    m_rowColorFunc = func;
    // 刷新所有可见行
    for (int i = 0; i < m_showIndex.size(); ++i) {
        QModelIndex idx = index(i);
        emit dataChanged(idx, idx);
    }
}

// ---------- JSON 序列化 ----------
QJsonObject SuperListModel::rowToJson(const TableRowData& row) const
{
    QJsonObject obj;
    QVariant val = row.get(m_columnConfig.name);
    if (val.type() == QVariant::String)
        obj[m_columnConfig.name] = val.toString();
    else if (val.type() == QVariant::Int || val.type() == QVariant::UInt)
        obj[m_columnConfig.name] = val.toInt();
    else if (val.type() == QVariant::Double)
        obj[m_columnConfig.name] = val.toDouble();
    else if (val.type() == QVariant::Bool)
        obj[m_columnConfig.name] = val.toBool();
    else if (val.type() != QVariant::Invalid)
        obj[m_columnConfig.name] = val.toString();
    return obj;
}

TableRowData SuperListModel::jsonToRow(const QJsonObject& json) const
{
    TableRowData row;
    if (json.contains(m_columnConfig.name)) {
        QJsonValue jv = json.value(m_columnConfig.name);
        QVariant val;
        if (jv.isString()) val = jv.toString();
        else if (jv.isDouble()) val = jv.toDouble();
        else if (jv.isBool()) val = jv.toBool();
        else if (!jv.isNull()) val = jv.toVariant().toString();
        row.set(m_columnConfig.name, val);
    }
    return row;
}

QJsonArray SuperListModel::getJsonData() const
{
    QJsonArray arr;
    for (const auto& row : m_originData)
        arr.append(rowToJson(row));
    return arr;
}

void SuperListModel::setJsonData(const QJsonArray& data)
{
    beginResetModel();
    m_originData.clear();
    m_originData.reserve(data.size());
    for (const auto& item : data) {
        if (item.isObject())
            m_originData.append(jsonToRow(item.toObject()));
    }
    doFilter();
    endResetModel();
    emit jsonDataChanged();
}

QJsonObject SuperListModel::getRowJson(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= m_showIndex.size()) return {};
    return rowToJson(m_originData[m_showIndex[rowIndex]]);
}

bool SuperListModel::setRowJson(int rowIndex, const QJsonObject& data)
{
    if (rowIndex < 0 || rowIndex >= m_showIndex.size()) return false;
    int originIdx = m_showIndex[rowIndex];
    m_originData[originIdx] = jsonToRow(data);
    emit dataChanged(index(rowIndex), index(rowIndex));
    emit jsonDataChanged();
    return true;
}

// ---------- 查找与更新 ----------
QList<int> SuperListModel::findRowsByColumn(const QString& colKey, const QString& value) const
{
    QList<int> matched;
    if (colKey.isEmpty() || value.isEmpty()) return matched;
    for (int i = 0; i < m_showIndex.size(); ++i) {
        QString cell = m_originData[m_showIndex[i]].get(colKey).toString();
        if (cell.contains(value, Qt::CaseInsensitive))
            matched.append(i);
    }
    return matched;
}

QList<int> SuperListModel::findRowsByColumnExact(const QString& colKey, const QString& value) const
{
    QList<int> matched;
    if (colKey.isEmpty()) return matched;
    for (int i = 0; i < m_showIndex.size(); ++i) {
        QString cell = m_originData[m_showIndex[i]].get(colKey).toString();
        if (cell == value) matched.append(i);
    }
    return matched;
}

bool SuperListModel::updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue)
{
    if (rowIndex < 0 || rowIndex >= m_showIndex.size() || colKey.isEmpty()) return false;
    int originIdx = m_showIndex[rowIndex];
    m_originData[originIdx].set(colKey, newValue);
    emit dataChanged(index(rowIndex), index(rowIndex));
    emit jsonDataChanged();
    return true;
}

int SuperListModel::updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                                       const QMap<QString, QVariant>& updates)
{
    QList<int> matched = findRowsByColumn(searchColKey, searchValue);
    if (matched.isEmpty()) return 0;
    for (int rowIdx : matched) {
        int originIdx = m_showIndex[rowIdx];
        for (auto it = updates.begin(); it != updates.end(); ++it)
            m_originData[originIdx].set(it.key(), it.value());
        emit dataChanged(index(rowIdx), index(rowIdx));
    }
    emit jsonDataChanged();
    return matched.size();
}

int SuperListModel::updateRowsByColumnExact(const QString& searchColKey, const QString& searchValue,
                                            const QMap<QString, QVariant>& updates)
{
    QList<int> matched = findRowsByColumnExact(searchColKey, searchValue);
    if (matched.isEmpty()) return 0;
    for (int rowIdx : matched) {
        int originIdx = m_showIndex[rowIdx];
        for (auto it = updates.begin(); it != updates.end(); ++it)
            m_originData[originIdx].set(it.key(), it.value());
        emit dataChanged(index(rowIdx), index(rowIdx));
    }
    emit jsonDataChanged();
    return matched.size();
}

// ---------- 重写 QAbstractListModel ----------
int SuperListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_showIndex.size();
}

QVariant SuperListModel::data(const QModelIndex &index, int role) const
{
    int r = index.row();
    if (r < 0 || r >= m_showIndex.size()) return QVariant();

    const TableRowData& row = m_originData[m_showIndex[r]];

    if (role == Qt::DisplayRole) {
        return row.get(m_columnConfig.name);
    } else if (role == Qt::UserRole) {
        return static_cast<int>(m_columnConfig.type);
    } else if (role == Qt::BackgroundRole && m_rowColorFunc) {
        QColor bg = m_rowColorFunc(row);
        if (bg.isValid()) return bg;
    }
    return QVariant();
}

bool SuperListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole && role != Qt::DisplayRole) return false;
    int r = index.row();
    if (r < 0 || r >= m_showIndex.size()) return false;

    int originIdx = m_showIndex[r];
    m_originData[originIdx].set(m_columnConfig.name, value);
    emit dataChanged(index, index);
    emit jsonDataChanged();
    return true;
}

Qt::ItemFlags SuperListModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

// ---------- 内部筛选逻辑 ----------
void SuperListModel::doFilter()
{
    m_showIndex.clear();
    m_showIndex.reserve(m_originData.size());
    for (int i = 0; i < m_originData.size(); ++i) {
        if (rowMatchesFilter(m_originData[i]))
            m_showIndex.append(i);
    }
}

bool SuperListModel::rowMatchesFilter(const TableRowData& row) const
{
    if (m_filterText.isEmpty() || m_filterCol.isEmpty()) return true;
    QString val = row.get(m_filterCol).toString();
    return val.contains(m_filterText, Qt::CaseInsensitive);
}

// ====================== SuperListDelegate 实现 ======================
static bool isCheckboxChecked(const QString& text) {
    return text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
            || text == QLatin1String("1");
}

SuperListDelegate::SuperListDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void SuperListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 获取模型数据
    TableCellType cellType = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    QString cellText = index.data(Qt::DisplayRole).toString();

    // 背景
    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, QColor(220,240,255));
    else if (index.data(Qt::BackgroundRole).isValid())
        painter->fillRect(option.rect, index.data(Qt::BackgroundRole).value<QColor>());
    else
        painter->fillRect(option.rect, Qt::white);

    switch (cellType) {
    case TableCellType::CheckBox:
        drawCheckBox(painter, option, isCheckboxChecked(cellText));
        break;
    case TableCellType::Progress:
        drawProgress(painter, option, cellText.toInt(), 100);
        break;
    case TableCellType::StateTag:
        drawStateTag(painter, option, cellText);
        break;
    default:
        drawText(painter, option, cellText);
        break;
    }
    painter->restore();
}

QSize SuperListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option); Q_UNUSED(index);
    return QSize(-1, m_itemHeight);
}

bool SuperListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    TableCellType type = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    if (type != TableCellType::CheckBox)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease)
        return false;

    QMouseEvent* mouseEv = static_cast<QMouseEvent*>(event);
    if (!mouseEv || mouseEv->button() != Qt::LeftButton || event->type() != QEvent::MouseButtonPress)
        return false;

    int boxSize = 16;
    QRect boxRect(option.rect.center().x()-boxSize/2, option.rect.center().y()-boxSize/2, boxSize, boxSize);
    if (!boxRect.contains(mouseEv->pos())) return false;

    bool checked = isCheckboxChecked(index.data(Qt::DisplayRole).toString());
    model->setData(index, checked ? "false" : "true", Qt::EditRole);
    return true;
}

// 绘制函数（与表格委托完全一致，略... 直接拷贝上述实现）
void SuperListDelegate::drawText(QPainter *p, const QStyleOptionViewItem &opt, const QString &text) const {
    QRect rc = opt.rect.adjusted(8,4,-8,-4);
    p->drawText(rc, Qt::AlignVCenter | Qt::AlignHCenter, text);
}
void SuperListDelegate::drawCheckBox(QPainter *p, const QStyleOptionViewItem &opt, bool checked) const {
    QRect rc = opt.rect;
    int boxSize = 16;
    QRect boxRc(rc.center().x()-boxSize/2, rc.center().y()-boxSize/2, boxSize, boxSize);
    p->drawRect(boxRc);
    if (checked) {
        p->setPen(Qt::darkBlue);
        p->drawLine(boxRc.topLeft()+QPoint(3,3), boxRc.bottomRight()-QPoint(3,3));
        p->drawLine(boxRc.topRight()+QPoint(-3,3), boxRc.bottomLeft()-QPoint(-3,3));
    }
}
void SuperListDelegate::drawProgress(QPainter *p, const QStyleOptionViewItem &opt, int val, int max) const {
    QRect rc = opt.rect.adjusted(8,8,-8,-8);
    p->drawRect(rc);
    if (max <= 0) max = 1;
    val = qBound(0, val, max);
    int w = static_cast<int>(qint64(rc.width()) * val / max);
    QRect fillRc(rc.x(), rc.y(), w, rc.height());
    p->fillRect(fillRc, QColor(60,160,220));
    p->drawText(rc, Qt::AlignCenter, QString("%1%").arg(val));
}
void SuperListDelegate::drawStateTag(QPainter *p, const QStyleOptionViewItem &opt, const QString &text) const {
    QRect rc = opt.rect.adjusted(8,6,-8,-6);
    QColor tagColor;
    if (text.compare("正常", Qt::CaseInsensitive) == 0) tagColor = QColor(70,190,90);
    else if (text.compare("失败", Qt::CaseInsensitive) ==0) tagColor = QColor(220,70,70);
    else if (text.compare("等待", Qt::CaseInsensitive)==0) tagColor = QColor(230,180,40);
    else tagColor = Qt::gray;
    p->setBrush(tagColor);
    p->setPen(Qt::transparent);
    p->drawRoundedRect(rc,4,4);
    p->setPen(Qt::white);
    p->drawText(rc, Qt::AlignCenter, text);
}

// ====================== SuperListWidget 实现 ======================
SuperListWidget::SuperListWidget(QWidget *parent)
    : QListView(parent)
{
    m_model = new SuperListModel(this);
    m_delegate = new SuperListDelegate(this);

    setModel(m_model);
    setItemDelegate(m_delegate);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // 连接信号 - 统一通过 onModelDataChanged 处理
    connect(m_model, &SuperListModel::jsonDataChanged,
            this, &SuperListWidget::onModelDataChanged);
    connect(m_model, &QAbstractListModel::rowsInserted,
            this, &SuperListWidget::onRowsInserted);
    connect(m_model, &QAbstractListModel::modelReset,
            this, &SuperListWidget::onRowsInserted);

    // 连接点击信号
    connect(this, &QListView::clicked, [this](const QModelIndex& idx) {
        if (!idx.isValid()) return;
        TableRowData data = m_model->getRow(idx.row());
        emit rowClickedIndex(data, idx.row());
    });
    connect(this, &QListView::doubleClicked, [this](const QModelIndex& idx) {
        if (!idx.isValid()) return;
        TableRowData data = m_model->getRow(idx.row());
        emit rowDoubleClicked(data, idx.row());
    });
}

void SuperListWidget::setHeaders(const QList<TableColumnConfig>& cols)
{
    m_model->setColumns(cols);
}

void SuperListWidget::clearData()
{
    m_model->clearAllData();
    selectionModel()->clear();
}

void SuperListWidget::addRows(const QList<TableRowData>& rows)
{
    m_model->appendRows(rows);
}

void SuperListWidget::setRowColorRule(const RowColorFunc& func)
{
    m_model->setRowColorRule(func);
}

void SuperListWidget::filterColumn(const QString &colKey, const QString &text)
{
    m_model->setFilterText(colKey, text);
}

void SuperListWidget::clearFilter()
{
    m_model->clearFilter();
}

QList<TableRowData> SuperListWidget::getSelectedRows() const
{
    QList<TableRowData> res;
    auto idxList = selectionModel()->selectedRows();
    for (const auto& idx : idxList)
        res.append(m_model->getRow(idx.row()));
    return res;
}

TableRowData SuperListWidget::getRow(int index) const
{
    return m_model->getRow(index);
}

QJsonArray SuperListWidget::getListData() const
{
    return m_model->getJsonData();
}

void SuperListWidget::setListData(const QJsonArray& data)
{
    m_model->setJsonData(data);
    selectionModel()->clear();  // 数据重置后清除选择状态
}

QJsonArray SuperListWidget::getSelectedRowsJson() const
{
    QJsonArray arr;
    auto idxList = selectionModel()->selectedRows();
    for (const auto& idx : idxList) {
        QJsonObject obj = m_model->getRowJson(idx.row());
        if (!obj.isEmpty()) arr.append(obj);
    }
    return arr;
}

void SuperListWidget::setItemHeight(int h)
{
    m_delegate->setItemHeight(h);
    // QListView 不会自动调整项大小，需要设置网格尺寸或统一大小
    setGridSize(QSize(-1, h));  // 使用网格模式强制统一高度
    setUniformItemSizes(true);
}

void SuperListWidget::setListSize(int w, int h)
{
    setFixedSize(w, h);
}

void SuperListWidget::setListMinSize(int w, int h)
{
    setMinimumSize(w, h);
}

void SuperListWidget::setListStyleSheet(const QString &qss)
{
    setStyleSheet(qss);
}

QList<int> SuperListWidget::findRowsByColumn(const QString& colKey, const QString& value) const
{
    return m_model->findRowsByColumn(colKey, value);
}

int SuperListWidget::updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                                        const QMap<QString, QVariant>& updates)
{
    return m_model->updateRowsByColumn(searchColKey, searchValue, updates);
}

bool SuperListWidget::updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue)
{
    return m_model->updateRowCell(rowIndex, colKey, newValue);
}

void SuperListWidget::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    emit SelectionChanged();
}

void SuperListWidget::onModelDataChanged()
{
    emit listDataChanged();
    // 注意：rowCount 可能没有变化，但为了安全，在 rowsInserted/modelReset 中会单独发射
    // 这里不再重复发射 rowCountChanged，避免不必要的信号
}

void SuperListWidget::onRowsInserted()
{
    emit rowCountChanged();
}

} // namespace Sqz
