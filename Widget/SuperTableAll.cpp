#include "SuperTableAll.h"
#include <QPainter>
#include <QHeaderView>
#include <QStringList>
#include <QCheckBox>

// ====================== SuperTableModel ======================
/**
 * @brief SuperTableModel构造函数
 * @param parent 父对象
 * 初始化QAbstractTableModel父类，无额外初始化逻辑
 */
namespace Sqz::Widget {
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
 * 触发模型重置前的通知，清空原始数据和展示数据，触发模型重置后的通知，视图同步清空
 */
void SuperTableModel::clearAllData()
{
    beginResetModel();
    m_originData.clear();
    m_showData.clear();
    endResetModel();
}

/**
 * @brief 追加行数据到表格
 * @param rows 要追加的行数据列表
 * 空列表直接返回；否则触发行插入前的通知，将数据追加到原始数据，执行筛选，触发行插入后的通知
 */
void SuperTableModel::appendRows(const QList<TableRowData> &rows)
{
    if (rows.isEmpty()) return;
    beginInsertRows(QModelIndex(), m_showData.size(), m_showData.size() + rows.size() - 1);
    m_originData.append(rows);
    doFilter();
    endInsertRows();
}

/**
 * @brief 获取指定行的展示数据
 * @param idx 行索引（展示数据的索引）
 * @return 该行数据，索引越界返回空TableRowData
 */
TableRowData SuperTableModel::getRow(int idx) const
{
    if (idx < 0 || idx >= m_showData.size()) return {};
    return m_showData[idx];
}

/**
 * @brief 获取所有展示数据行
 * @return 展示数据列表（筛选后的结果）
 */
QList<TableRowData> SuperTableModel::getAllRows() const
{
    return m_showData;
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
 * @brief 获取表格行数（展示数据）
 * @param parent 父索引（表格中无效）
 * @return 展示数据的行数，父索引有效则返回0（表格非树形结构）
 */
int SuperTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_showData.size();
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
    if (r < 0 || r >= m_showData.size() || c <0 || c >= m_columns.size())
        return QVariant();

    const TableRowData& row = m_showData[r];
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
        QColor c = m_rowColorFunc(row);
        if (c.isValid()) return c;
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
    if (r < 0 || r >= m_showData.size() || c < 0 || c >= m_columns.size())
        return false;

    TableColumnConfig col = m_columns[c];
    // 1. 更新当前展示行
    m_showData[r].set(col.name, value);

    // 2. 直接同步原始数据同下标（不再根据id匹配，彻底解决匹配失败回弹）
    if (r < m_originData.size())
    {
        m_originData[r].set(col.name, value);
    }

    emit dataChanged(index, index);
    return true;
}

/**
 * @brief 执行数据筛选逻辑
 * 清空展示数据，无筛选条件则直接复制原始数据到展示数据；
 * 有筛选条件则遍历原始数据，将匹配的行添加到展示数据（列值包含关键词，大小写不敏感）
 */
void SuperTableModel::doFilter()
{
    m_showData.clear();
    if (m_filterText.isEmpty() || m_filterCol.isEmpty())
    {
        m_showData = m_originData;
        return;
    }
    for (const auto& row : m_originData)
    {
        QString val = row.get(m_filterCol).toString();
        if (val.contains(m_filterText, Qt::CaseInsensitive))
        {
            m_showData.append(row);
        }
    }
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

    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, QColor(220,240,255));
    else if (index.data(Qt::BackgroundRole).isValid())
        painter->fillRect(option.rect, index.data(Qt::BackgroundRole).value<QColor>());
    else
        painter->fillRect(option.rect, Qt::white);

    TableCellType cellType = static_cast<TableCellType>(index.data(Qt::UserRole).toInt());
    QString cellText = index.data(Qt::DisplayRole).toString();

    switch (cellType)
    {
    case TableCellType::CheckBox:
        drawCheckBox(painter, option, cellText.compare("true", Qt::CaseInsensitive) == 0);
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
    return QSize(option.rect.width(), 32);
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

    QMouseEvent* mouseEv = dynamic_cast<QMouseEvent*>(event);
    if(!mouseEv)
        return false;

    // 仅在鼠标【左键按下】时执行切换，松开不处理
    if (mouseEv->button() == Qt::LeftButton && mouseEv->type() == QEvent::MouseButtonPress)
    {
        int boxSize = 16;
        QRect boxRect(option.rect.center().x()-boxSize/2, option.rect.center().y()-boxSize/2, boxSize, boxSize);
        if(!boxRect.contains(mouseEv->pos()))
            return false;

        bool isChecked = (index.data(Qt::DisplayRole).toString() == "true" || index.data(Qt::DisplayRole).toString() == "1");
        model->setData(index, isChecked ? "false" : "true", Qt::EditRole);
        // 返回true：消费本次鼠标事件，阻止视图默认刷新回弹
        return true;
    }
    // 其他鼠标事件全部放行
    return false;
}

/**
 * @brief 绘制普通文本单元格
 * @param p 绘制器对象
 * @param opt 单元格样式选项
 * @param text 要绘制的文本
 * 文本区域：左右缩进8像素，上下缩进4像素，左对齐+垂直居中
 */
void SuperTableDelegate::drawText(QPainter *p, const QStyleOptionViewItem &opt, const QString &text) const
{
    QRect rc = opt.rect.adjusted(8,4,-8,-4);
    p->drawText(rc, Qt::AlignVCenter | Qt::AlignLeft, text);
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
 */
void SuperTableDelegate::drawProgress(QPainter *p, const QStyleOptionViewItem &opt, int val, int max) const
{
    QRect rc = opt.rect.adjusted(8,8,-8,-8);
    p->drawRect(rc);
    if (max <=0) return;
    int w = rc.width() * val / max;
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

    setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked);
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
}
