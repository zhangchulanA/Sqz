#include "SuperTableAll.h"
#include <QPainter>
#include <QHeaderView>
#include <QStringList>
#include <QLineEdit>
#include <QMouseEvent>

// ====================== SuperTableModel ======================
/**
 * @brief SuperTableModel构造函数
 * @param parent 父对象
 * 初始化QAbstractTableModel父类，无额外初始化逻辑
 */
namespace Sqz {
SuperTableModel::SuperTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

/**
 * @brief 设置表格列配置
 * @param cols 列配置列表
 * 触发模型重置前的通知，更新列配置，触发模型重置后的通知，视图同步刷新列结构
 */
void SuperTableModel::setColumns(const QList<TableColumnConfig> &cols)
{
    beginResetModel();
    m_columns = cols;
    endResetModel();
}

/**
 * @brief 获取当前列配置列表
 * @return 列配置列表的拷贝
 */
QList<TableColumnConfig> SuperTableModel::getColumns() const
{
    return m_columns;
}

/**
 * @brief 清空所有表格数据
 * 触发模型重置前的通知，清空原始数据和展示索引，触发模型重置后的通知，视图同步清空
 */
void SuperTableModel::clearAllData()
{
    beginResetModel();
    m_originData.clear();
    m_showIndex.clear();
    endResetModel();
}

/**
 * @brief 追加行数据到表格
 * @param rows 要追加的行数据列表
 * 修复：原实现在 beginInsertRows/endInsertRows 之间调用 doFilter()，导致声明行数
 *       与实际行数不一致（有筛选时新行可能不匹配），视图状态错乱甚至崩溃。
 * 新方案：增量筛选——只追加命中筛选的行到 m_showIndex，保留视图选中/滚动状态。
 */
void SuperTableModel::appendRows(const QList<TableRowData> &rows)
{
    if (rows.isEmpty()) return;

    // 原始数据总是全量保存
    int originStart = m_originData.size();
    m_originData.append(rows);

    // 增量计算新增行中命中筛选的部分（不重扫全表）
    QList<int> visibleNewIndices;
    visibleNewIndices.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        if (rowMatchesFilter(rows[i]))
            visibleNewIndices.append(originStart + i);
    }

    if (visibleNewIndices.isEmpty())
        return;  // 视图层一行都不用插

    const int first = m_showIndex.size();
    const int last  = first + visibleNewIndices.size() - 1;
    beginInsertRows(QModelIndex(), first, last);
    m_showIndex.append(visibleNewIndices);
    endInsertRows();
}

/**
 * @brief 获取指定行的展示数据
 * @param idx 行索引（展示数据的索引）
 * @return 该行数据，索引越界返回空TableRowData
 * 修复：通过 m_showIndex 间接访问 m_originData，不再使用 m_showData 副本
 */
TableRowData SuperTableModel::getRow(int idx) const
{
    if (idx < 0 || idx >= m_showIndex.size()) return {};
    return m_originData[m_showIndex[idx]];
}

/**
 * @brief 获取所有展示数据行
 * @return 展示数据列表（筛选后的结果）
 * 修复：通过 m_showIndex 从 m_originData 构建列表
 */
QList<TableRowData> SuperTableModel::getAllRows() const
{
    QList<TableRowData> result;
    result.reserve(m_showIndex.size());
    for (int idx : m_showIndex)
        result.append(m_originData[idx]);
    return result;
}

/**
 * @brief 设置列筛选条件
 * @param colKey 要筛选的列名
 * @param filter 筛选关键词（自动去除首尾空格）
 * 保存筛选条件，触发模型重置前的通知，执行筛选，触发模型重置后的通知，视图同步刷新筛选结果
 */
void SuperTableModel::setFilterText(const QString &colKey, const QString &filter)
{
    m_filterCol = colKey;
    m_filterText = filter.trimmed();
    beginResetModel();
    doFilter();
    endResetModel();
}

/**
 * @brief 清空筛选条件
 * 清空筛选列和筛选关键词，触发模型重置前的通知，执行筛选（展示所有数据），触发模型重置后的通知
 */
void SuperTableModel::clearFilter()
{
    m_filterCol.clear();
    m_filterText.clear();
    beginResetModel();
    doFilter();
    endResetModel();
}

/**
 * @brief 设置行背景色自定义回调函数
 * @param func 行染色回调函数
 * 保存回调函数，后续绘制行时会调用该函数获取背景色
 */
void SuperTableModel::setRowColorRule(const RowColorFunc &func)
{
    m_rowColorFunc = func;
}

/**
 * @brief 根据指定列的值查找匹配的行索引（展示数据中的索引）
 */
QList<int> SuperTableModel::findRowsByColumn(const QString& colKey, const QString& value) const
{
    QList<int> matchedRows;
    if (colKey.isEmpty() || value.isEmpty())
        return matchedRows;

    for (int i = 0; i < m_showIndex.size(); ++i)
    {
        const TableRowData& row = m_originData[m_showIndex[i]];
        QString cellValue = row.get(colKey).toString();
        if (cellValue.contains(value, Qt::CaseInsensitive))
            matchedRows.append(i);
    }
    return matchedRows;
}

/**
 * @brief 根据指定列的值精确查找匹配的行索引
 */
QList<int> SuperTableModel::findRowsByColumnExact(const QString& colKey, const QString& value) const
{
    QList<int> matchedRows;
    if (colKey.isEmpty())
        return matchedRows;

    for (int i = 0; i < m_showIndex.size(); ++i)
    {
        const TableRowData& row = m_originData[m_showIndex[i]];
        QString cellValue = row.get(colKey).toString();
        if (cellValue == value)
            matchedRows.append(i);
    }
    return matchedRows;
}

/**
 * @brief 更新指定展示行中某列的数据
 */
bool SuperTableModel::updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue)
{
    if (rowIndex < 0 || rowIndex >= m_showIndex.size() || colKey.isEmpty())
        return false;

    int originIdx = m_showIndex[rowIndex];
    m_originData[originIdx].set(colKey, newValue);

    // 找到该行对应的列索引，触发数据变更通知
    for (int c = 0; c < m_columns.size(); ++c)
    {
        if (m_columns[c].name == colKey)
        {
            QModelIndex idx = index(rowIndex, c);
            emit dataChanged(idx, idx);
            return true;
        }
    }
    return false;
}

/**
 * @brief 根据某列值查找并更新该行中其他列的数据
 */
int SuperTableModel::updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                                        const QMap<QString, QVariant>& updates)
{
    if (searchColKey.isEmpty() || updates.isEmpty())
        return 0;

    QList<int> matchedRows = findRowsByColumn(searchColKey, searchValue);
    if (matchedRows.isEmpty())
        return 0;

    int updateCount = 0;
    for (int rowIndex : matchedRows)
    {
        int originIdx = m_showIndex[rowIndex];
        // 更新多个列
        for (auto it = updates.begin(); it != updates.end(); ++it)
        {
            m_originData[originIdx].set(it.key(), it.value());
        }
        // 触发整个行的数据变更通知
        for (int c = 0; c < m_columns.size(); ++c)
        {
            QModelIndex idx = index(rowIndex, c);
            emit dataChanged(idx, idx);
        }
        ++updateCount;
    }
    return updateCount;
}

/**
 * @brief 根据某列值精确查找并更新该行中其他列的数据
 */
int SuperTableModel::updateRowsByColumnExact(const QString& searchColKey, const QString& searchValue,
                                             const QMap<QString, QVariant>& updates)
{
    if (searchColKey.isEmpty() || updates.isEmpty())
        return 0;

    QList<int> matchedRows = findRowsByColumnExact(searchColKey, searchValue);
    if (matchedRows.isEmpty())
        return 0;

    int updateCount = 0;
    for (int rowIndex : matchedRows)
    {
        int originIdx = m_showIndex[rowIndex];
        for (auto it = updates.begin(); it != updates.end(); ++it)
        {
            m_originData[originIdx].set(it.key(), it.value());
        }
        for (int c = 0; c < m_columns.size(); ++c)
        {
            QModelIndex idx = index(rowIndex, c);
            emit dataChanged(idx, idx);
        }
        ++updateCount;
    }
    return updateCount;
}



/**
 * @brief 获取表格行数（展示数据）
 * @param parent 父索引（表格中无效）
 * @return 展示索引的行数，父索引有效则返回0（表格非树形结构）
 */
int SuperTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_showIndex.size();
}

/**
 * @brief 获取表格列数
 * @param parent 父索引（表格中无效）
 * @return 列配置的列数，父索引有效则返回0（表格非树形结构）
 */
int SuperTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_columns.size();
}

/**
 * @brief 获取单元格指定角色的数据
 * @param index 单元格索引
 * @param role 数据角色
 * @return 对应角色的数据，索引越界返回空QVariant
 * 处理的角色：
 * - Qt::DisplayRole：返回单元格展示文本
 * - Qt::UserRole：返回单元格类型（TableCellType枚举值）
 * - Qt::BackgroundRole：返回行背景色（若回调函数有效且返回有效颜色）
 */
QVariant SuperTableModel::data(const QModelIndex &index, int role) const
{
    int r = index.row();
    int c = index.column();
    if (r < 0 || r >= m_showIndex.size() || c < 0 || c >= m_columns.size())
        return QVariant();

    // 修复：通过 m_showIndex 间接访问 m_originData（单一存储）
    const TableRowData& row = m_originData[m_showIndex[r]];
    const TableColumnConfig& colCfg = m_columns[c];

    if (role == Qt::DisplayRole)
    {
        return row.get(colCfg.name);
    }
    else if (role == Qt::UserRole)
    {
        return static_cast<int>(colCfg.type);
    }
    else if (role == Qt::BackgroundRole && m_rowColorFunc)
    {
        QColor bg = m_rowColorFunc(row);
        if (bg.isValid()) return bg;
    }
    return QVariant();
}

/**
 * @brief 获取表头数据
 * @param section 表头列索引
 * @param orientation 表头方向（仅处理水平表头）
 * @param role 数据角色（仅处理展示角色）
 * @return 列表头显示文本，索引越界或条件不满足返回父类默认值
 */
QVariant SuperTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (section >=0 && section < m_columns.size())
            return m_columns[section].title;
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

/**
 * @brief 获取单元格标志
 * @param index 单元格索引
 * @return 单元格标志：可选中、可启用、可编辑
 */
Qt::ItemFlags SuperTableModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

/**
 * @brief 设置单元格数据（编辑）
 * @param index 单元格索引
 * @param value 新值
 * @param role 数据角色（仅处理编辑角色）
 * @return 是否设置成功
 * 步骤：
 * 1. 校验角色和索引有效性，无效则返回false
 * 2. 更新展示数据中对应单元格的值
 * 3. 同步更新原始数据中同索引行的对应单元格值（解决数据不一致问题）
 * 4. 触发数据变更通知，视图刷新该单元格
 */
bool SuperTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    int r = index.row();
    int c = index.column();
    if (r < 0 || r >= m_showIndex.size() || c < 0 || c >= m_columns.size())
        return false;

    TableColumnConfig col = m_columns[c];

    // 修复：通过 m_showIndex 反查原始数据下标，直接修改 m_originData
    // 单一存储结构天然保证展示数据和原始数据同步，无需手动维护两份副本
    int originIdx = m_showIndex[r];
    m_originData[originIdx].set(col.name, value);

    emit dataChanged(index, index);
    return true;
}

/**
 * @brief 执行数据筛选逻辑
 * 重建 m_showIndex 映射数组：无筛选条件则映射所有原始行；
 * 有筛选条件则只映射匹配行（列值包含关键词，大小写不敏感）
 * 修复：不再拷贝行数据，只重建索引数组，零拷贝、O(n)
 */
void SuperTableModel::doFilter()
{
    m_showIndex.clear();
    m_showIndex.reserve(m_originData.size());
    for (int i = 0; i < m_originData.size(); ++i)
    {
        if (rowMatchesFilter(m_originData[i]))
            m_showIndex.append(i);
    }
}

/**
 * @brief 判断单行数据是否匹配当前筛选条件
 * @param row 待检查的行数据
 * @return true 表示匹配（应展示），false 表示不匹配
 * 修复：提取为纯函数，doFilter 和 appendRows 共用，确保筛选逻辑一致性
 */
bool SuperTableModel::rowMatchesFilter(const TableRowData& row) const
{
    // 无筛选条件则全部匹配
    if (m_filterText.isEmpty() || m_filterCol.isEmpty())
        return true;
    QString val = row.get(m_filterCol).toString();
    return val.contains(m_filterText, Qt::CaseInsensitive);
}

/**
 * @brief 根据列索引获取列配置
 * @param sec 列索引
 * @return 列配置，索引越界返回空配置
 */
TableColumnConfig SuperTableModel::getColumnBySection(int sec) const
{
    if (sec >=0 && sec < m_columns.size())
        return m_columns[sec];
    return {};
}

// ====================== SuperTableDelegate ======================

/**
 * @brief 统一判断单元格文本是否表示复选框"选中"状态
 * @param text 单元格文本
 * @return true 表示选中
 * 修复：paint 和 editorEvent 共用此函数，确保判断逻辑一致
 * 支持 "true"(大小写不敏感) 和 "1" 两种表示
 */
static bool isCheckboxChecked(const QString& text)
{
    return text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
            || text == QLatin1String("1");
}

/**
 * @brief SuperTableDelegate构造函数
 * @param parent 父对象
 * 初始化QStyledItemDelegate父类，无额外初始化逻辑
 */
SuperTableDelegate::SuperTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

/**
 * @brief 绘制单元格
 * @param painter 绘制器对象
 * @param option 单元格样式选项
 * @param index 单元格索引
 * 绘制流程：
 * 1. 保存绘制器状态，开启抗锯齿（提升绘制效果）
 * 2. 绘制单元格背景（选中状态/自定义背景色/默认白色）
 * 3. 获取单元格类型和文本，按类型调用对应绘制函数
 * 4. 恢复绘制器状态
 */
void SuperTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 修复：缓存 index.data() 结果，避免在 paint 中重复虚拟调用
    QVariant bgVar = index.data(Qt::BackgroundRole);
    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, QColor(220,240,255));
    else if (bgVar.isValid())
        painter->fillRect(option.rect, bgVar.value<QColor>());
    else
        painter->fillRect(option.rect, Qt::white);

    // 缓存 cellType 和 cellText，减少 data() 调用次数
    TableCellType cellType = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    QString cellText = index.data(Qt::DisplayRole).toString();

    switch (cellType)
    {
    case TableCellType::CheckBox:
        // 修复：统一复选框判断逻辑（paint 和 editorEvent 一致）
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

/**
 * @brief 获取单元格尺寸提示
 * @param option 单元格样式选项
 * @param index 单元格索引
 * @return 单元格推荐尺寸（宽度使用默认，高度固定32像素）
 */
QSize SuperTableDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    // 修复：原版返回 option.rect.width()，初始布局时可能为 0
    // 返回 -1 让 QTableView 使用默认列宽/sizeHintForColumn 的默认行为
    return QSize(-1, 32);
}

/**
 * @brief 处理编辑器事件
 * @param event 事件对象
 * @param model 数据模型
 * @param option 单元格样式选项
 * @param index 单元格索引
 * @return 是否消费该事件
 * 仅处理复选框单元格的鼠标左键按下事件：
 * 1. 判断事件类型和鼠标按键，非左键按下则放行
 * 2. 计算复选框区域，点击位置不在复选框内则放行
 * 3. 切换复选框状态，更新模型数据，返回true消费事件（阻止默认行为）
 * 其他事件均放行，使用父类默认处理
 */
bool SuperTableDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    TableCellType type = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    if(type != TableCellType::CheckBox)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    // 修复：用 static_cast 替代 dynamic_cast，先检查事件类型再转换
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease)
        return false;

    QMouseEvent* mouseEv = static_cast<QMouseEvent*>(event);
    if(!mouseEv)
        return false;

    // 仅在鼠标【左键按下】时执行切换，松开不处理
    if (mouseEv->button() == Qt::LeftButton && mouseEv->type() == QEvent::MouseButtonPress)
    {
        int boxSize = 16;
        QRect boxRect(option.rect.center().x()-boxSize/2, option.rect.center().y()-boxSize/2, boxSize, boxSize);
        if(!boxRect.contains(mouseEv->pos()))
            return false;

        // 修复：统一复选框判断逻辑（与 paint 一致）
        bool isChecked = isCheckboxChecked(index.data(Qt::DisplayRole).toString());
        model->setData(index, isChecked ? "false" : "true", Qt::EditRole);
        // 返回true：消费本次鼠标事件，阻止视图默认刷新回弹
        return true;
    }
    // 其他鼠标事件全部放行
    return false;
}

QWidget *SuperTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 对于复选框类型，不创建编辑器（使用点击切换）
    TableCellType cellType = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    if (cellType == TableCellType::CheckBox) {
        return nullptr;
    }
    // 其他类型使用默认编辑器（QLineEdit）
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void SuperTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // 从模型中获取当前数据并设置到编辑器
    QString value = index.data(Qt::DisplayRole).toString();
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
    if (lineEdit) {
        lineEdit->setText(value);
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void SuperTableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
    if (lineEdit) {
        model->setData(index, lineEdit->text(), Qt::EditRole);
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

/**
 * @brief 绘制普通文本单元格
 * @param p 绘制器对象
 * @param opt 单元格样式选项
 * @param text 要绘制的文本
 * 文本区域：左右缩进8像素，上下缩进4像素，水平居中+垂直居中
 */
void SuperTableDelegate::drawText(QPainter *p, const QStyleOptionViewItem &opt, const QString &text) const
{
    QRect rc = opt.rect.adjusted(8,4,-8,-4);
    p->drawText(rc, Qt::AlignVCenter | Qt::AlignHCenter, text);
}

/**
 * @brief 绘制复选框单元格
 * @param p 绘制器对象
 * @param opt 单元格样式选项
 * @param checked 是否勾选
 * 复选框位置：单元格居中，尺寸16x16像素；勾选状态绘制蓝色对勾
 */
void SuperTableDelegate::drawCheckBox(QPainter *p, const QStyleOptionViewItem &opt, bool checked) const
{
    QRect rc = opt.rect;
    int boxSize = 16;
    QRect boxRc(rc.center().x()-boxSize/2, rc.center().y()-boxSize/2, boxSize, boxSize);
    p->drawRect(boxRc);
    if (checked)
    {
        p->setPen(Qt::darkBlue);
        p->drawLine(boxRc.topLeft()+QPoint(3,3), boxRc.bottomRight()-QPoint(3,3));
        p->drawLine(boxRc.topRight()+QPoint(-3,3), boxRc.bottomLeft()-QPoint(-3,3));
    }
}

/**
 * @brief 绘制进度条单元格
 * @param p 绘制器对象
 * @param opt 单元格样式选项
 * @param val 当前进度值
 * @param max 进度最大值
 * 进度条区域：上下左右缩进8像素，绘制外框；按比例绘制蓝色填充进度，居中显示百分比文本
 * 修复：钳制 val 和 max 防止除零、负宽、溢出
 */
void SuperTableDelegate::drawProgress(QPainter *p, const QStyleOptionViewItem &opt, int val, int max) const
{
    QRect rc = opt.rect.adjusted(8,8,-8,-8);
    p->drawRect(rc);
    // 修复：钳制 max 防止除零，钳制 val 防止负宽和溢出
    if (max <= 0) max = 1;
    val = qBound(0, val, max);
    // 用 qint64 防止大数相乘溢出
    int w = static_cast<int>(qint64(rc.width()) * val / max);
    QRect fillRc(rc.x(), rc.y(), w, rc.height());
    p->fillRect(fillRc, QColor(60,160,220));
    p->drawText(rc, Qt::AlignCenter, QString("%1%").arg(val));
}

/**
 * @brief 绘制状态标签单元格
 * @param p 绘制器对象
 * @param opt 单元格样式选项
 * @param text 状态文本
 * 标签区域：上下缩进6像素，左右缩进8像素；不同状态对应不同颜色，圆角矩形（半径4像素），白色居中文本
 */
void SuperTableDelegate::drawStateTag(QPainter *p, const QStyleOptionViewItem &opt, const QString &text) const
{
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

// ====================== SuperTableWidget 外层控件 ======================
/**
 * @brief SuperTableWidget构造函数
 * @param parent 父控件
 * 初始化流程：
 * 1. 创建数据模型和绘制代理，设置父子关系
 * 2. 绑定模型和代理到QTableView
 * 3. 配置表格基础样式和行为：隐藏垂直表头、显示网格线、行选择模式、表头自适应、平滑滚动等
 */
SuperTableWidget::SuperTableWidget(QWidget *parent)
    : QTableView(parent)
{
    m_model = new SuperTableModel(this);
    m_delegate = new SuperTableDelegate(this);

    setModel(m_model);
    setItemDelegate(m_delegate);

    // 基础性能
    verticalHeader()->hide();
    setShowGrid(true);
    setGridStyle(Qt::DotLine);

    // 选择模式
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 表头基础
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // 滚动使用平滑像素滚动，减少重绘卡顿
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setEditTriggers(QAbstractItemView::DoubleClicked);

    // 连接双击信号
    // 🔥 统一使用 Qt 的信号
            connect(this, &QTableView::clicked,
                    this, &SuperTableWidget::onClicked);
            connect(this, &QTableView::doubleClicked,
                    this, &SuperTableWidget::onDoubleClicked);


}

/**
 * @brief 设置表格列配置和表头
 * @param cols 列配置列表
 * 步骤：
 * 1. 将列配置同步到模型
 * 2. 遍历列配置，设置列宽度，隐藏需要隐藏的列
 */
void SuperTableWidget::setHeaders(const QList<TableColumnConfig> &cols)
{
    m_model->setColumns(cols);
    for (int i=0; i<cols.size(); i++)
    {
        setColumnWidth(i, cols[i].width);
        if (cols[i].hidden) hideColumn(i);
    }
}

/**
 * @brief 清空表格数据
 * 调用模型的清空接口，简化上层调用
 */
void SuperTableWidget::clearData()
{
    m_model->clearAllData();
}

/**
 * @brief 追加行数据到表格
 * @param rows 要追加的行数据列表
 * 调用模型的追加接口，简化上层调用
 */
void SuperTableWidget::addRows(const QList<TableRowData> &rows)
{
    m_model->appendRows(rows);
}

/**
 * @brief 设置行背景色自定义规则
 * @param func 行染色回调函数
 * 调用模型的设置接口，简化上层调用
 */
void SuperTableWidget::setRowColorRule(const RowColorFunc &func)
{
    m_model->setRowColorRule(func);
}

/**
 * @brief 按指定列筛选数据
 * @param colKey 要筛选的列名
 * @param text 筛选关键词
 * 调用模型的筛选接口，简化上层调用
 */
void SuperTableWidget::filterColumn(const QString &colKey, const QString &text)
{
    m_model->setFilterText(colKey, text);
}

/**
 * @brief 清空筛选条件
 * 调用模型的清空筛选接口，简化上层调用
 */
void SuperTableWidget::clearFilter()
{
    m_model->clearFilter();
}

/**
 * @brief 获取选中的行数据
 * @return 选中行的展示数据列表
 * 遍历选中的行索引，从模型中获取对应行数据，返回列表
 */
QList<TableRowData> SuperTableWidget::getSelectedRows() const
{
    QList<TableRowData> res;
    auto idxList = selectionModel()->selectedRows();
    for (const auto& idx : idxList)
        res.append(m_model->getRow(idx.row()));
    return res;
}

TableRowData SuperTableWidget::getRow(int index)
{
    return m_model->getRow(index);
}

// ========== 新增尺寸控制接口实现 ==========
/**
 * @brief 设置全局统一行高
 * @param h 行高，单位像素
 * 配置垂直表头的默认行高，所有行使用该高度
 */
void SuperTableWidget::setGlobalRowHeight(int h)
{
    verticalHeader()->setDefaultSectionSize(h);
}

/**
 * @brief 设置指定列的宽度
 * @param colIdx 列索引
 * @param w 列宽度，单位像素
 * 索引有效时设置列宽度，无效则不处理
 */
void SuperTableWidget::setColWidth(int colIdx, int w)
{
    if (colIdx >=0) setColumnWidth(colIdx, w);
}

/**
 * @brief 设置表格整体固定尺寸
 * @param w 宽度，单位像素
 * @param h 高度，单位像素
 * 设置控件的固定尺寸，替代自适应布局
 */
void SuperTableWidget::setTableSize(int w, int h)
{
    this->setFixedSize(w, h);
}

/**
 * @brief 设置表格最小尺寸
 * @param w 最小宽度，单位像素
 * @param h 最小高度，单位像素
 * 设置控件的最小尺寸，尺寸不会小于该值
 */
void SuperTableWidget::setTableMinSize(int w, int h)
{
    this->setMinimumSize(w, h);
}

/**
 * @brief 设置表头高度
 * @param h 表头高度，单位像素
 * 设置水平表头的固定高度
 */
void SuperTableWidget::setHeaderHeight(int h)
{
    horizontalHeader()->setFixedHeight(h);
}

/**
 * @brief 设置表格样式表
 * @param qss Qt样式表字符串
 * 直接设置控件的样式表，支持自定义表格样式
 */
void SuperTableWidget::setTableStyleSheet(const QString &qss)
{
    this->setStyleSheet(qss);
}
/*
* @brief 根据列值查找匹配的行索引（转发到模型）
*/
QList<int> SuperTableWidget::findRowsByColumn(const QString& colKey, const QString& value) const
{
    return m_model->findRowsByColumn(colKey, value);
}

/**
* @brief 根据某列值查找并更新该行中其他列的数据（转发到模型）
*/
int SuperTableWidget::updateRowsByColumn(const QString& searchColKey, const QString& searchValue,
                                         const QMap<QString, QVariant>& updates)
{
    return m_model->updateRowsByColumn(searchColKey, searchValue, updates);
}

/**
* @brief 更新指定行的某列数据（转发到模型）
*/
bool SuperTableWidget::updateRowCell(int rowIndex, const QString& colKey, const QVariant& newValue)
{
    return m_model->updateRowCell(rowIndex, colKey, newValue);
}



//void SuperTableWidget::mouseReleaseEvent(QMouseEvent *event)
//{
//    // 调用父类处理，确保选择等行为正常
//    QTableView::mouseReleaseEvent(event);

//    // 获取点击位置的索引
//    QModelIndex index = indexAt(event->pos());
//    if (index.isValid())
//    {
//        // 只处理左键单击
//        if (event->button() == Qt::LeftButton)
//        {
//            emitRowClickedSignal(index);
//        }else if (event->button() == Qt::RightButton) {
//            // 可以添加右键信号
////            emit rowRightClicked(data, row);
//        }
//    }
//}

void SuperTableWidget::emitRowClickedSignal(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    int row = index.row();
    TableRowData rowData = m_model->getRow(row);
    emit rowClicked(rowData, row);
    emit rowClicked(rowData);
}

void SuperTableWidget::onClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    int row = index.row();
    TableRowData data = m_model->getRow(index.row());
    emit rowClicked(data, row);
}

void SuperTableWidget::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid())return;

    int row = index.row();
    TableRowData rowData = m_model->getRow(row);
    emit rowDoubleClicked(rowData, row);
}
}
