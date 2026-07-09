#ifndef FRAMELESSWIDGET_H
#define FRAMELESSWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QGraphicsDropShadowEffect>
#include <QSettings>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>         // 新增：定时器（动画/防抖）
#include <QColor>         // 新增：颜色处理
#include "SqzGlobal.h"
/**
 * @class TitleBar
 * @brief 自定义标题栏控件类
 * @details 该类继承自QWidget，实现了可自定义样式的窗口标题栏，包含窗口图标、标题文本、最小化/最大化/关闭按钮，
 *          并提供了鼠标交互相关的信号（点击、双击、拖动等），支持自定义背景色、文字色、高度等样式属性。
 *          【扩展功能】：按钮禁用/启用、标题居中/居左、按钮hover动画、标题栏透明度设置、自定义按钮扩展接口
 *
 * 特点：
 * 1. 默认隐藏左侧图标，避免原生标题栏的黑方块问题；
 * 2. 按钮样式美化，hover状态有视觉反馈，关闭按钮hover变红更符合用户习惯；
 * 3. 提供丰富的样式自定义接口，适配不同UI风格；
 * 4. 完整的鼠标事件转发，支持标题栏拖动、双击最大化等原生窗口行为；
 * 5. 浅灰色默认背景，适配浅色UI体系，避免白底突兀；
 * 6. 新增右键菜单支持、标题对齐方式、按钮状态控制、透明度调节等扩展能力。
 */
namespace Sqz::Widget {
class SQZ_FRAMEWORK_API TitleBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int opacity READ opacity WRITE setOpacity) // 新增：透明度属性（支持动画）
public:
    /**
     * @brief 构造函数
     * @param parent 父控件指针，默认nullptr
     */
    explicit TitleBar(QWidget *parent = nullptr);

    /**
     * @brief 设置标题栏背景色
     * @param color 颜色值（支持CSS格式，如#F5F7FA、rgb(255,255,255)等）
     */
    void setTitleBarBgColor(const QString &color);

    /**
     * @brief 设置标题文本颜色
     * @param color 颜色值（支持CSS格式）
     */
    void setTitleTextColor(const QString &color);

    /**
     * @brief 设置标题栏高度
     * @param h 高度值（像素）
     */
    void setTitleBarHeight(int h);

    /**
     * @brief 设置窗口标题文本
     * @param title 标题字符串
     */
    void setWindowTitle(const QString &title);

    /**
     * @brief 设置标题栏左侧图标
     * @param pix 图标像素图（会自动缩放到16x16）
     */
    void setIcon(const QPixmap &pix);

    /**
     * @brief 隐藏标题栏左侧图标
     */
    void hideIcon();

    /**
     * @brief 获取最小化按钮指针
     * @return QPushButton* 最小化按钮指针
     */
    QPushButton* minBtn() { return m_btnMin; }

    /**
     * @brief 获取最大化按钮指针
     * @return QPushButton* 最大化按钮指针
     */
    QPushButton* maxBtn() { return m_btnMax; }

    /**
     * @brief 获取关闭按钮指针
     * @return QPushButton* 关闭按钮指针
     */
    QPushButton* closeBtn() { return m_btnClose; }

    // ========== 新增扩展接口 ==========
    /**
     * @brief 设置标题文本对齐方式
     * @param align 对齐方式（Qt::AlignLeft/AlignCenter/AlignRight）
     */
    void setTitleAlign(Qt::Alignment align);

    /**
     * @brief 禁用/启用窗口控制按钮
     * @param type 按钮类型（1-最小化，2-最大化，3-关闭）
     * @param enable true-启用，false-禁用
     */
    void setControlBtnEnable(int type, bool enable);

    /**
     * @brief 设置标题栏透明度
     * @param opa 透明度（0.0-1.0，1.0为不透明）
     */
    void setOpacity(qreal opa);

    /**
     * @brief 获取标题栏透明度
     * @return qreal 当前透明度值
     */
    qreal opacity() const { return m_opacity; }

    /**
     * @brief 给标题栏添加自定义按钮
     * @param btn 自定义按钮指针
     * @param index 插入位置（-1表示追加到按钮组最右侧）
     */
    void addCustomBtn(QPushButton* btn, int index = -1);

signals:
    /**
     * @brief 最小化按钮点击信号
     */
    void sigMinClicked();

    /**
     * @brief 最大化按钮点击信号
     */
    void sigMaxClicked();

    /**
     * @brief 关闭按钮点击信号
     */
    void sigCloseClicked();

    /**
     * @brief 标题栏双击信号（用于触发窗口最大化/还原）
     */
    void sigDoubleClick();

    /**
     * @brief 标题栏鼠标移动信号
     * @param pos 鼠标全局坐标
     */
    void sigMouseMove(const QPoint &pos);

    /**
     * @brief 标题栏鼠标按下信号
     * @param pos 鼠标全局坐标
     */
    void sigMousePress(const QPoint &pos);

    /**
     * @brief 标题栏鼠标释放信号
     */
    void sigMouseRelease();

protected:
    /**
     * @brief 重写鼠标按下事件
     * @param event 鼠标事件对象
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 重写鼠标移动事件
     * @param event 鼠标事件对象
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 重写鼠标双击事件
     * @param event 鼠标事件对象
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * @brief 重写鼠标释放事件
     * @param event 鼠标事件对象
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel* m_labIcon;          // 标题栏左侧图标标签
    QLabel* m_labTitle;         // 标题文本标签
    QPushButton* m_btnMin;      // 最小化按钮
    QPushButton* m_btnMax;      // 最大化/还原按钮
    QPushButton* m_btnClose;    // 关闭按钮
    QHBoxLayout* m_layout;      // 标题栏水平布局
    int m_barHeight = 32;       // 标题栏默认高度（像素）
    qreal m_opacity = 1.0;      // 新增：标题栏透明度（默认不透明）
    QList<QPushButton*> m_customBtns; // 新增：自定义按钮列表
};

/**
 * @class FramelessWidget
 * @brief 无边框窗口基类
 * @details 该类继承自QWidget，封装了无边框窗口的核心功能，包括自定义标题栏、窗口圆角、阴影效果、八方向缩放、
 *          窗口拖动（标题栏/空白区域）、窗口状态持久化（位置/大小/最大化状态）等，适配Linux/Windows等系统，
 *          解决了原生无边框窗口的各种兼容性问题。
 *          【扩展功能】：新增窗口动画（显示/隐藏）、窗口置顶、缩放防抖、内容区域自定义、多显示器适配、窗口透明度调节
 *
 * 特点：
 * 1. 完整的无边框窗口能力：支持拖动、八方向缩放、最小化/最大化/关闭、双击标题栏最大化；
 * 2. 样式可定制：圆角半径、阴影开关、标题栏显隐/样式、背景色等均可配置；
 * 3. 系统兼容性强：针对Linux虚拟机做了渲染优化，避免图形驱动崩溃；
 * 4. 性能优化：缩放/拖动时临时关闭阴影，减少渲染撕裂，结束后恢复；
 * 5. 灵活的拖动逻辑：支持标题栏拖动，无标题栏时可配置空白区域拖动；
 * 6. 窗口状态持久化：可保存/恢复窗口位置、大小、最大化状态到配置文件；
 * 7. 圆角渲染优化：仅刷新必要区域，减少全局重绘，提升流畅度；
 * 8. 最大化自适应：最大化时自动去除边距，还原时恢复，视觉无断层；
 * 9. 新增扩展能力：窗口动画、置顶、缩放防抖、多显示器适配、全局透明度调节等。
 */
class SQZ_FRAMEWORK_API FramelessWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal windowOpacity READ windowOpacity WRITE setWindowOpacity) // 新增：窗口透明度属性
public:
    /**
     * @brief 构造函数
     * @param parent 父控件指针，默认nullptr
     */
    explicit FramelessWidget(QWidget *parent = nullptr);

    /**
     * @brief 设置窗口圆角半径
     * @param radius 圆角半径（像素）
     */
    void setWindowRadius(int radius);

    /**
     * @brief 设置阴影效果是否启用
     * @param enable true-启用，false-禁用
     */
    void setShadowEnable(bool enable);

    /**
     * @brief 设置窗口是否允许缩放
     * @param enable true-允许，false-禁止
     */
    void setResizeEnable(bool enable);

    /**
     * @brief 设置是否保存窗口状态（位置/大小/最大化）
     * @param enable true-启用保存，false-禁用
     * @param saveKey 保存状态的唯一标识（用于区分不同窗口）
     */
    void setSaveWindowState(bool enable, const QString &saveKey);

    /**
     * @brief 获取标题栏控件指针
     * @return TitleBar* 标题栏指针
     */
    TitleBar* titleBar() { return m_titleBar; }

    /**
     * @brief 设置标题栏是否可见
     * @param visible true-显示，false-隐藏
     */
    void setTitleBarVisible(bool visible);

    /**
     * @brief 获取标题栏可见状态
     * @return bool true-可见，false-隐藏
     */
    bool isTitleBarVisible() const;

    /**
     * @brief 设置无标题栏时，是否允许点击窗口空白处拖动
     * @param enable true-允许，false-禁止
     */
    void setDragByEmptyArea(bool enable);

    // ========== 新增扩展接口 ==========
    /**
     * @brief 设置窗口是否置顶
     * @param top true-置顶，false-取消置顶
     */
    void setWindowAlwaysOnTop(bool top);

    /**
     * @brief 获取窗口是否置顶
     * @return bool true-已置顶，false-未置顶
     */
    bool isWindowAlwaysOnTop() const { return m_alwaysOnTop; }

    /**
     * @brief 设置窗口显示/隐藏动画
     * @param duration 动画时长（毫秒，0表示关闭动画）
     * @param type 动画类型（0-淡入淡出，1-缩放+淡入淡出）
     */
    void setWindowAnimation(int duration, int type = 0);

    /**
     * @brief 设置窗口整体透明度
     * @param opa 透明度（0.0-1.0，1.0为不透明）
     */
    void setWindowOpacity(qreal opa);

    /**
     * @brief 获取窗口整体透明度
     * @return qreal 当前透明度值
     */
    qreal windowOpacity() const { return m_windowOpacity; }

    /**
     * @brief 设置缩放防抖阈值（避免快速缩放导致窗口抖动）
     * @param threshold 防抖阈值（像素，默认5）
     */
    void setResizeDebounce(int threshold);

    /**
     * @brief 设置内容区域控件（替换默认的空白contentWidget）
     * @param widget 自定义内容控件指针
     */
    void setContentWidget(QWidget* widget);

    /**
     * @brief 获取内容区域控件指针
     * @return QWidget* 内容区域指针
     */
    QWidget* contentWidget() const { return m_contentWidget; }

    /**
     * @brief 多显示器适配：将窗口移动到指定显示器
     * @param screenIndex 显示器索引（从0开始）
     */
    void moveToScreen(int screenIndex);

protected:
    /**
     * @brief 重写鼠标按下事件（处理窗口拖动/缩放的起始逻辑）
     * @param event 鼠标事件对象
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 重写鼠标移动事件（处理窗口拖动/缩放的过程逻辑）
     * @param event 鼠标事件对象
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 重写鼠标释放事件（结束窗口拖动/缩放，恢复阴影和光标）
     * @param event 鼠标事件对象
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief 重写绘制事件（绘制窗口圆角背景）
     * @param event 绘制事件对象
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief 重写窗口状态变化事件（处理最大化/还原时的边距适配）
     * @param event 事件对象
     */
    void changeEvent(QEvent *event) override;

    /**
     * @brief 重写关闭事件（关闭时保存窗口状态）
     * @param event 关闭事件对象
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief 重写离开事件（恢复默认光标）
     * @param event 离开事件对象
     */
    void leaveEvent(QEvent *event) override;

    /**
     * @brief 重写显示事件（新增：支持显示动画）
     * @param event 显示事件对象
     */
    void showEvent(QShowEvent *event) override;

    /**
     * @brief 重写隐藏事件（新增：支持隐藏动画）
     * @param event 隐藏事件对象
     */
    void hideEvent(QHideEvent *event) override;

private:
    /**
     * @brief 判断鼠标是否在窗口缩放边缘
     * @param pos 鼠标本地坐标
     * @param edgeType 输出参数：边缘类型（0-无，1-左，2-右，3-上，4-下，5-左上，6-右上，7-左下，8-右下）
     * @return bool true-在边缘，false-不在边缘
     */
    bool isOnEdge(const QPoint &pos, int &edgeType);

    /**
     * @brief 根据边缘类型更新鼠标光标样式
     * @param edgeType 边缘类型（同isOnEdge的edgeType）
     */
    void updateCursor(int edgeType);

    /**
     * @brief 从配置文件恢复窗口状态（位置/大小/最大化）
     */
    void restoreWindowState();

    /**
     * @brief 将窗口状态保存到配置文件
     */
    void saveWindowState();

    // ========== 新增扩展私有方法 ==========
    /**
     * @brief 窗口动画执行函数（显示/隐藏）
     * @param show true-显示动画，false-隐藏动画
     */
    void runWindowAnimation(bool show);

private slots:
    /**
     * @brief 最小化按钮点击槽函数
     */
    void onMinClicked();

    /**
     * @brief 最大化按钮点击槽函数（切换最大化/还原）
     */
    void onMaxClicked();

    /**
     * @brief 关闭按钮点击槽函数
     */
    void onCloseClicked();

    /**
     * @brief 标题栏双击槽函数（切换最大化/还原）
     */
    void onTitleDoubleClick();

    /**
     * @brief 标题栏鼠标按下槽函数（记录拖动起始位置）
     * @param globalPos 鼠标全局坐标
     */
    void onTitleMousePress(const QPoint &globalPos);

    /**
     * @brief 标题栏鼠标移动槽函数（执行窗口拖动）
     * @param globalPos 鼠标全局坐标
     */
    void onTitleMouseMove(const QPoint &globalPos);

    /**
     * @brief 标题栏鼠标释放槽函数（结束窗口拖动）
     */
    void onTitleMouseRelease();

private:
    TitleBar* m_titleBar;               // 自定义标题栏控件
    QWidget* m_contentWidget;           // 窗口内容区域控件
    QVBoxLayout* m_mainLayout;          // 窗口主布局（垂直）

    // 样式配置
    int m_radius = 8;                   // 窗口圆角半径（默认8像素）
    bool m_shadowEnable = true;         // 是否启用阴影效果（默认启用）
    bool m_resizeEnable = true;         // 是否允许窗口缩放（默认允许）
    QGraphicsDropShadowEffect* m_shadowEffect; // 阴影效果对象

    // 拖拽缩放数据
    QPoint m_mousePressPos;             // 鼠标按下时的全局坐标
    QRect m_windowGeoPress;             // 鼠标按下时的窗口几何区域
    int m_edgeType = 0;                 // 边缘类型（0-无，1-左，2-右，3-上，4-下，5-左上，6-右上，7-左下，8-右下）
    bool m_isDragWindow = false;        // 是否正在拖动窗口
    bool m_dragEmptyArea = true;        // 无标题栏时空白区域是否可拖动（默认允许）

    // 持久化配置
    bool m_saveState = false;           // 是否启用窗口状态保存（默认禁用）
    QString m_saveKey;                  // 窗口状态保存的唯一标识

    bool m_fixLinuxRoundBug = true;     // 是否修复Linux系统圆角渲染bug（默认启用）
    bool m_isOperating = false;         // 是否正在拖动/缩放

    // ========== 新增扩展成员变量 ==========
    bool m_alwaysOnTop = false;         // 窗口是否置顶（默认否）
    qreal m_windowOpacity = 1.0;        // 窗口整体透明度（默认不透明）
    int m_animDuration = 0;             // 窗口动画时长（默认0，关闭动画）
    int m_animType = 0;                 // 窗口动画类型（0-淡入淡出，1-缩放+淡入淡出）
    int m_resizeDebounce = 5;           // 缩放防抖阈值（默认5像素）
    QTimer* m_resizeTimer;              // 缩放防抖定时器
    QRect m_targetGeo;                  // 缩放目标区域（防抖用）
};
}
#endif // FRAMELESSWIDGET_H
