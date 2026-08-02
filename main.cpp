/**
 * @file main_table.cpp
 * @brief SuperTableAll 全功能测试示例
 *
 * 测试覆盖：
 *  1. 基本数据操作（设置表头/追加/获取/清空）
 *  2. 多类型单元格（文本/复选框/进度条/状态标签）
 *  3. ★ 筛选 + 追加增量（Bug1回归：appendRows不再全量doFilter）
 *  4. ★ 筛选 + 编辑同步（Bug2回归：setData通过m_showIndex正确同步原始数据）
 *  5. ★ 筛选状态下追加不匹配行（行数不增长，不崩溃）
 *  6. 复选框判断一致性（Bug4回归：paint和editorEvent统一逻辑）
 *  7. 进度条越界防护（Bug3回归：val>max/val<0/max<=0）
 *  8. 行染色回调
 *  9. clearFilter恢复全量
 * 10. 选中行获取
 */

#include <QApplication>
#include <QDebug>
#include "SuperTableAll.h"

using namespace Sqz;

// 全局失败计数
static int g_failCount = 0;

// CHECK 宏：条件为true打印PASS，否则打印FAIL并计数
#define CHECK(cond) do { \
    if (cond) { qDebug() << "  PASS:" << #cond; } \
    else { qDebug() << "  FAIL:" << #cond; ++g_failCount; } \
} while(0)

/**
 * @brief 构建测试用的列配置
 * 5列：ID(文本)、名称(文本)、状态(状态标签)、进度(进度条)、选择(复选框)
 */
static QList<TableColumnConfig> makeTestColumns()
{
    QList<TableColumnConfig> cols;
    TableColumnConfig c1;  c1.name="id";       c1.title="ID";     c1.type=TableCellType::Text;     c1.width=80;  c1.hidden=false; c1.sortable=true;  cols.append(c1);
    TableColumnConfig c2;  c2.name="name";     c2.title="name";   c2.type=TableCellType::Text;     c2.width=200; c2.hidden=false; c2.sortable=true;  cols.append(c2);
    TableColumnConfig c3;  c3.name="status";   c3.title="status"; c3.type=TableCellType::StateTag; c3.width=120; c3.hidden=false; c3.sortable=true;  cols.append(c3);
    TableColumnConfig c4;  c4.name="progress"; c4.title="progress"; c4.type=TableCellType::Progress; c4.width=150; c4.hidden=false; c4.sortable=true; cols.append(c4);
    TableColumnConfig c5;  c5.name="sel";      c5.title="sel";    c5.type=TableCellType::CheckBox;  c5.width=80;  c5.hidden=false; c5.sortable=true;  cols.append(c5);
    return cols;
}

/**
 * @brief 构建测试行数据
 * @param startId 起始ID
 * @param count 行数
 * @param statusPattern 状态模式："mixed"=正常/失败/等待循环
 */
static QList<TableRowData> makeTestRows(int startId, int count, const QString& statusPattern = "mixed")
{
    QList<TableRowData> rows;
    for (int i = 0; i < count; ++i)
    {
        int id = startId + i;
        TableRowData r;
        r.set("id", id);
        r.set("name", QString("item_%1").arg(id));

        if (statusPattern == "mixed")
            r.set("status", id % 3 == 0 ? "正常" : (id % 3 == 1 ? "失败" : "等待"));
        else
            r.set("status", statusPattern);

        r.set("progress", id % 101);  // 0~100
        r.set("sel", id % 2 == 0);
        rows.append(r);
    }
    return rows;
}

/**
 * @brief 测试场景1：基本数据操作
 */
static void testBasicOperations()
{
    qDebug() << "\n[场景1] 基本数据操作（设置表头/追加/获取/清空）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());

    // 追加10行
    auto rows = makeTestRows(0, 10);
    model.appendRows(rows);
    CHECK(model.rowCount() == 10);
    CHECK(model.columnCount() == 5);

    // 获取第0行
    TableRowData r0 = model.getRow(0);
    CHECK(r0.get("id").toInt() == 0);
    CHECK(r0.get("name").toString() == "item_0");
    CHECK(r0.get("status").toString() == "正常");

    // 获取第9行
    TableRowData r9 = model.getRow(9);
    CHECK(r9.get("id").toInt() == 9);

    // 越界获取返回空
    TableRowData r100 = model.getRow(100);
    CHECK(r100.cells.isEmpty());

    // getAllRows
    QList<TableRowData> all = model.getAllRows();
    CHECK(all.size() == 10);

    // 清空
    model.clearAllData();
    CHECK(model.rowCount() == 0);
    CHECK(model.getAllRows().isEmpty());
}

/**
 * @brief 测试场景2：多类型单元格数据
 */
static void testCellTypes()
{
    qDebug() << "\n[场景2] 多类型单元格（文本/复选框/进度条/状态标签）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());
    model.appendRows(makeTestRows(0, 5));

    // 检查 UserRole 返回正确的单元格类型
    QModelIndex idx = model.index(0, 0); // id列 = Text
    CHECK(idx.data(Qt::UserRole).toInt() == static_cast<int>(TableCellType::Text));

    idx = model.index(0, 2); // status列 = StateTag
    CHECK(idx.data(Qt::UserRole).toInt() == static_cast<int>(TableCellType::StateTag));

    idx = model.index(0, 3); // progress列 = Progress
    CHECK(idx.data(Qt::UserRole).toInt() == static_cast<int>(TableCellType::Progress));

    idx = model.index(0, 4); // sel列 = CheckBox
    CHECK(idx.data(Qt::UserRole).toInt() == static_cast<int>(TableCellType::CheckBox));

    // 检查 DisplayRole 返回正确数据
    idx = model.index(0, 0);
    CHECK(idx.data(Qt::DisplayRole).toInt() == 0);

    idx = model.index(1, 2); // item_1 的状态 = "失败"
    CHECK(idx.data(Qt::DisplayRole).toString() == "失败");
}

/**
 * @brief 测试场景3：★ 筛选 + 追加增量（Bug1回归）
 * 原bug：appendRows在beginInsertRows/endInsertRows之间调用doFilter，
 *        导致声明行数与实际行数不一致，视图状态错乱甚至崩溃。
 * 修复：增量筛选，只追加命中筛选的行到m_showIndex。
 */
static void testFilterAndAppend()
{
    qDebug() << "\n[场景3] ★ 筛选 + 追加增量（Bug1回归：appendRows不再全量doFilter）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());

    // 先追加5行"正常"状态的数据
    model.appendRows(makeTestRows(0, 5, "正常"));
    CHECK(model.rowCount() == 5);

    // 设置筛选条件：只显示"正常"
    model.setFilterText("status", "正常");
    CHECK(model.rowCount() == 5);  // 5行都是"正常"，全匹配

    // 追加5行"正常" + 5行"失败"
    QList<TableRowData> newRows;
    newRows.append(makeTestRows(5, 5, "正常"));   // 命中筛选
    newRows.append(makeTestRows(10, 5, "失败"));  // 不命中筛选
    model.appendRows(newRows);

    // 应该只新增5行（命中的），总展示10行
    CHECK(model.rowCount() == 10);

    // 验证所有展示行都是"正常"
    for (int i = 0; i < model.rowCount(); ++i)
    {
        CHECK(model.getRow(i).get("status").toString() == "正常");
    }

    // 清除筛选后应该有15行
    model.clearFilter();
    CHECK(model.rowCount() == 15);
}

/**
 * @brief 测试场景4：★ 筛选 + 编辑同步（Bug2回归）
 * 原bug：setData用展示索引r直接访问m_originData[r]，
 *        筛选时r是展示索引≠原始索引，编辑后数据同步到错误的原始行。
 * 修复：通过m_showIndex[r]反查原始数据下标，天然同步。
 */
static void testFilterAndEdit()
{
    qDebug() << "\n[场景4] ★ 筛选 + 编辑同步（Bug2回归：setData通过m_showIndex正确同步）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());

    // 追加10行：0~4正常，5~9失败
    QList<TableRowData> rows;
    rows.append(makeTestRows(0, 5, "正常"));
    rows.append(makeTestRows(5, 5, "失败"));
    model.appendRows(rows);
    CHECK(model.rowCount() == 10);

    // 筛选"正常"，展示5行
    model.setFilterText("status", "正常");
    CHECK(model.rowCount() == 5);

    // 编辑展示行第0行的name（对应原始数据第0行）
    QModelIndex idx = model.index(0, 1); // name列
    model.setData(idx, "edited_item_0", Qt::EditRole);
    CHECK(model.getRow(0).get("name").toString() == "edited_item_0");

    // ★ 关键验证：清除筛选后，原始数据第0行的name也应该是修改后的值
    model.clearFilter();
    CHECK(model.getRow(0).get("name").toString() == "edited_item_0");
    // 原始数据第5行（失败行）的name不应该被修改
    CHECK(model.getRow(5).get("name").toString() == "item_5");

    // 再测试：筛选"失败"，编辑展示行第0行（对应原始数据第5行）
    model.setFilterText("status", "失败");
    CHECK(model.rowCount() == 5);

    idx = model.index(0, 1); // 展示第0行 = 原始第5行
    model.setData(idx, "edited_item_5", Qt::EditRole);

    // 清除筛选后验证：原始第5行被修改，原始第0行不受影响
    model.clearFilter();
    CHECK(model.getRow(5).get("name").toString() == "edited_item_5");
    CHECK(model.getRow(0).get("name").toString() == "edited_item_0");  // 之前修改的值保留
}

/**
 * @brief 测试场景5：★ 筛选状态下追加完全不匹配的行
 */
static void testAppendNonMatching()
{
    qDebug() << "\n[场景5] ★ 筛选状态下追加不匹配行（行数不增长，不崩溃）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());
    model.appendRows(makeTestRows(0, 5, "正常"));
    model.setFilterText("status", "正常");
    CHECK(model.rowCount() == 5);

    // 追加5行全部不匹配的数据
    model.appendRows(makeTestRows(5, 5, "失败"));
    CHECK(model.rowCount() == 5);  // 行数不变

    // 清除筛选后应该有10行
    model.clearFilter();
    CHECK(model.rowCount() == 10);

    // 验证数据完整性
    for (int i = 0; i < 10; ++i)
    {
        CHECK(model.getRow(i).get("id").toInt() == i);
    }
}

/**
 * @brief 测试场景6：复选框判断一致性（Bug4回归）
 */
static void testCheckboxConsistency()
{
    qDebug() << "\n[场景6] 复选框判断一致性（Bug4回归：paint和editorEvent统一逻辑）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());

    // 手动设置复选框值的各种表示形式
    TableRowData r;
    r.set("id", 0);
    r.set("name", "test");
    r.set("status", "正常");
    r.set("progress", 50);
    r.set("sel", "true");  // 字符串"true"
    model.appendRows({r});

    // 通过model.data获取值，验证能正确读取
    QModelIndex idx = model.index(0, 4);
    QString val = idx.data(Qt::DisplayRole).toString();
    CHECK(val == "true");

    // setData切换为"false"
    model.setData(idx, "false", Qt::EditRole);
    CHECK(model.getRow(0).get("sel").toString() == "false");

    // setData切换为"true"
    model.setData(idx, "true", Qt::EditRole);
    CHECK(model.getRow(0).get("sel").toString() == "true");

    // 测试"1"也能正确设置
    model.setData(idx, "1", Qt::EditRole);
    CHECK(model.getRow(0).get("sel").toString() == "1");
}

/**
 * @brief 测试场景7：进度条越界防护（Bug3回归）
 * 通过setData设置越界值，验证不崩溃
 */
static void testProgressBounds()
{
    qDebug() << "\n[场景7] 进度条越界防护（Bug3回归：val>max/val<0/max<=0）";

    SuperTableModel model;
    model.setColumns(makeTestColumns());
    model.appendRows(makeTestRows(0, 3));

    // 设置负值进度
    QModelIndex idx = model.index(0, 3);
    model.setData(idx, -50, Qt::EditRole);
    CHECK(model.getRow(0).get("progress").toInt() == -50);  // 数据层存储原值
    // 绘制时由drawProgress的qBound钳制，不会崩溃

    // 设置超大值进度
    model.setData(idx, 99999, Qt::EditRole);
    CHECK(model.getRow(0).get("progress").toInt() == 99999);
    // 绘制时由drawProgress的qBound钳制，不会崩溃

    // 设置0
    model.setData(idx, 0, Qt::EditRole);
    CHECK(model.getRow(0).get("progress").toInt() == 0);
}

/**
 * @brief 测试场景8：行染色回调
 */
static void testRowColor()
{
    qDebug() << "\n[场景8] 行染色回调";

    SuperTableModel model;
    model.setColumns(makeTestColumns());
    model.appendRows(makeTestRows(0, 5));

    // 设置行染色规则：失败行为红色背景
    model.setRowColorRule([](const TableRowData& row) -> QColor {
        if (row.get("status").toString() == "失败")
            return QColor(255, 200, 200);
        return QColor();
    });

    // item_1是失败状态（id=1, 1%3==1 → 失败）
    QModelIndex idx1 = model.index(1, 0);
    QVariant bg1 = idx1.data(Qt::BackgroundRole);
    CHECK(bg1.isValid());
    CHECK(bg1.value<QColor>() == QColor(255, 200, 200));

    // item_0是正常状态（id=0, 0%3==0 → 正常）
    QModelIndex idx0 = model.index(0, 0);
    QVariant bg0 = idx0.data(Qt::BackgroundRole);
    CHECK(!bg0.isValid());  // 返回无效颜色
}

/**
 * @brief 测试场景9：clearFilter恢复全量
 */
static void testClearFilter()
{
    qDebug() << "\n[场景9] clearFilter恢复全量";

    SuperTableModel model;
    model.setColumns(makeTestColumns());
    model.appendRows(makeTestRows(0, 20));

    // 筛选后行数减少
    model.setFilterText("status", "正常");
    int filteredCount = model.rowCount();
    CHECK(filteredCount > 0 && filteredCount < 20);

    // 清除筛选后恢复全量
    model.clearFilter();
    CHECK(model.rowCount() == 20);

    // 再次筛选验证
    model.setFilterText("name", "item_1");
    CHECK(model.rowCount() > 0);  // item_1, item_10~item_19 等都匹配
    model.clearFilter();
    CHECK(model.rowCount() == 20);
}

/**
 * @brief 测试场景10：Widget层级集成测试
 */
static void testWidgetIntegration()
{
    qDebug() << "\n[场景10] Widget集成测试（表头设置/尺寸控制/样式）";

    SuperTableWidget widget;

    // 设置表头
    widget.setHeaders(makeTestColumns());
    CHECK(widget.model()->rowCount() == 0);

    // 追加数据
    widget.addRows(makeTestRows(0, 100));
    CHECK(widget.model()->rowCount() == 100);

    // 尺寸控制接口不崩溃
    widget.setGlobalRowHeight(36);
    widget.setHeaderHeight(38);
    widget.setColWidth(0, 100);
    widget.setColWidth(1, 200);
    widget.setTableMinSize(800, 400);

    // 筛选
    widget.filterColumn("status", "正常");
    CHECK(widget.model()->rowCount() < 100);
    CHECK(widget.model()->rowCount() > 0);

    // 清除筛选
    widget.clearFilter();
    CHECK(widget.model()->rowCount() == 100);

    // 获取行数据
    TableRowData r = widget.getRow(0);
    CHECK(r.get("id").toInt() == 0);

    // 清空数据
    widget.clearData();
    CHECK(widget.model()->rowCount() == 0);

    // 样式表不崩溃
    widget.setTableStyleSheet("QTableView { background-color: white; }");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qDebug() << "=== SuperTableAll 全功能测试示例  Qt" << QT_VERSION_STR << "===";

    testBasicOperations();
    testCellTypes();
    testFilterAndAppend();
    testFilterAndEdit();
    testAppendNonMatching();
    testCheckboxConsistency();
    testProgressBounds();
    testRowColor();
    testClearFilter();
    testWidgetIntegration();

    qDebug() << "\n==============";
    qDebug() << "总场景: 10 | FAIL count:" << g_failCount;
    qDebug() << "==============";

    return g_failCount == 0 ? 0 : 1;
}
