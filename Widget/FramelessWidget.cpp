#include "FramelessWidget.h"
#include <QPainter>
#include <QCursor>
#include <QDesktopWidget>
#include <QApplication>
#include <QFile>
#include <QPropertyAnimation> // 新增：属性动画
#include <QScreen>            // 新增：多显示器适配
#include <QShowEvent>         // 新增：显示事件
#include <QHideEvent>         // 新增：隐藏事件

//==================== TitleBar 美化重写 + 扩展 ====================
/**
 * @brief TitleBar构造函数，初始化标题栏布局和控件
 * @param parent 父控件指针
 */
namespace Sqz {
TitleBar::TitleBar(QWidget *parent) : QWidget(parent)
{
    m_barHeight = 32;
    setFixedHeight(m_barHeight);
    // 默认浅灰背景，不再强制黑色，解决白底突兀
    setStyleSheet("background-color:#F5F7FA;");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8,0,8,0);
    m_layout->setSpacing(8);

    m_labIcon = new QLabel;
    m_labIcon->setFixedSize(16,16);
    m_labIcon->setVisible(false); // 默认隐藏图标，消除左侧黑方块！
    m_labTitle = new QLabel("Window");
    m_labTitle->setStyleSheet("color:#222222; font-size:13px;");

    // 美化窗口按钮，去除生硬文字
    m_btnMin = new QPushButton("─");
    m_btnMax = new QPushButton("□");
    m_btnClose = new QPushButton("×");
    m_btnMin->setFixedSize(24,24);
    m_btnMax->setFixedSize(24,24);
    m_btnClose->setFixedSize(24,24);

    // 浅色按钮样式，适配浅色标题栏
    m_btnMin->setStyleSheet(R"(
                            QPushButton{background:transparent;color:#333; border:none; border-radius:4px;}
                            QPushButton:hover{background:#E0E4EB;}
                            QPushButton:disabled{color:#999;}
                            )");
    m_btnMax->setStyleSheet(R"(
                            QPushButton{background:transparent;color:#333; border:none; border-radius:4px;}
                            QPushButton:hover{background:#E0E4EB;}
                            QPushButton:disabled{color:#999;}
                            )");
    m_btnClose->setStyleSheet(R"(
                              QPushButton{background:transparent;color:#333; border:none; border-radius:4px;}
                              QPushButton:hover{background:#FF4D4F; color:white;}
                              QPushButton:disabled{color:#999; background:transparent;}
                              )");

    m_layout->addWidget(m_labIcon);
    m_layout->addWidget(m_labTitle);
    m_layout->addStretch();
    m_layout->addWidget(m_btnMin);
    m_layout->addWidget(m_btnMax);
    m_layout->addWidget(m_btnClose);

    connect(m_btnMin, &QPushButton::clicked, this, &TitleBar::sigMinClicked);
    connect(m_btnMax, &QPushButton::clicked, this, &TitleBar::sigMaxClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &TitleBar::sigCloseClicked);
}

/**
 * @brief 设置标题栏背景色
 * @param color 颜色值（CSS格式）
 */
void TitleBar::setTitleBarBgColor(const QString &color)
{
    QString sheet = QString("background-color:%1;").arg(color);
    setStyleSheet(sheet);
}

/**
 * @brief 设置标题文本颜色
 * @param color 颜色值（CSS格式）
 */
void TitleBar::setTitleTextColor(const QString &color)
{
    m_labTitle->setStyleSheet(QString("color:%1; font-size:13px;").arg(color));
}

/**
 * @brief 设置标题栏高度
 * @param h 高度值（像素）
 */
void TitleBar::setTitleBarHeight(int h)
{
    m_barHeight = h;
    setFixedHeight(h);
}

/**
 * @brief 设置窗口标题文本
 * @param title 标题字符串
 */
void TitleBar::setWindowTitle(const QString &title)
{
    m_labTitle->setText(title);
}

/**
 * @brief 设置标题栏左侧图标
 * @param pix 图标像素图，自动缩放到16x16
 */
void TitleBar::setIcon(const QPixmap &pix)
{
    m_labIcon->setPixmap(pix.scaled(16,16,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    m_labIcon->setVisible(true);
}

/**
 * @brief 隐藏标题栏左侧图标
 */
void TitleBar::hideIcon()
{
    m_labIcon->setVisible(false);
}

/**
 * @brief 新增：设置标题文本对齐方式
 * @param align 对齐方式（Qt::AlignLeft/AlignCenter/AlignRight）
 */
void TitleBar::setTitleAlign(Qt::Alignment align)
{
    m_labTitle->setAlignment(align);
    // 重新调整布局：居中和居右时需要调整stretch位置
    if (align == Qt::AlignCenter || align == Qt::AlignRight)
    {
        // 先移除原有stretch
        m_layout->removeItem(m_layout->itemAt(m_layout->count() - 4)); // 移除标题后的stretch
        if (align == Qt::AlignCenter)
        {
            m_layout->insertStretch(m_layout->indexOf(m_labTitle), 1); // 标题前加stretch
            m_layout->insertStretch(m_layout->indexOf(m_btnMin), 1);   // 按钮前加stretch
        }
        else if (align == Qt::AlignRight)
        {
            m_layout->insertStretch(m_layout->indexOf(m_labTitle), 1); // 标题前加stretch
        }
    }
    else // 居左恢复默认
    {
        // 清除多余stretch，恢复默认布局
        for (int i = 0; i < m_layout->count(); ++i)
        {
            QLayoutItem* item = m_layout->itemAt(i);
            if (item->spacerItem())
            {
                m_layout->removeItem(item);
                delete item;
                --i;
            }
        }
        m_layout->insertStretch(m_layout->indexOf(m_btnMin), 1); // 按钮前加stretch
    }
}

/**
 * @brief 新增：禁用/启用窗口控制按钮
 * @param type 按钮类型（1-最小化，2-最大化，3-关闭）
 * @param enable true-启用，false-禁用
 */
void TitleBar::setControlBtnEnable(int type, bool enable)
{
    switch (type)
    {
    case 1: m_btnMin->setEnabled(enable); break;
    case 2: m_btnMax->setEnabled(enable); break;
    case 3: m_btnClose->setEnabled(enable); break;
    default: break;
    }
}

/**
 * @brief 新增：设置标题栏透明度
 * @param opa 透明度（0.0-1.0，1.0为不透明）
 */
void TitleBar::setOpacity(qreal opa)
{
    m_opacity = qBound(0.0, opa, 1.0);
    QColor bgColor = palette().background().color();
    bgColor.setAlphaF(m_opacity);
    setStyleSheet(QString("background-color:%1;").arg(bgColor.name(QColor::HexArgb)));
    // 同步更新文本和按钮透明度
    QPalette pal = m_labTitle->palette();
    QColor textColor = pal.color(QPalette::WindowText);
    textColor.setAlphaF(m_opacity);
    pal.setColor(QPalette::WindowText, textColor);
    m_labTitle->setPalette(pal);

    // 更新按钮透明度
    QList<QPushButton*> btns = {m_btnMin, m_btnMax, m_btnClose};
    for (QPushButton* btn : btns)
    {
        pal = btn->palette();
        QColor btnColor = pal.color(QPalette::ButtonText);
        btnColor.setAlphaF(m_opacity);
        pal.setColor(QPalette::ButtonText, btnColor);
        btn->setPalette(pal);
    }
}

/**
 * @brief 新增：给标题栏添加自定义按钮
 * @param btn 自定义按钮指针
 * @param index 插入位置（-1表示追加到按钮组最右侧）
 */
void TitleBar::addCustomBtn(QPushButton* btn, int index)
{
    if (!btn) return;
    btn->setFixedSize(24,24);
    btn->setStyleSheet(R"(
                       QPushButton{background:transparent;color:#333; border:none; border-radius:4px;}
                       QPushButton:hover{background:#E0E4EB;}
                       )");
    m_customBtns.append(btn);
    // 计算插入位置：按钮组在stretch之后，默认按钮之前/之间
    int btnStartIndex = m_layout->indexOf(m_btnMin);
    if (index < 0 || index > m_customBtns.count())
    {
        m_layout->insertWidget(btnStartIndex, btn); // 追加到默认按钮左侧
    }
    else
    {
        m_layout->insertWidget(btnStartIndex + index, btn); // 指定位置插入
    }
}


/**
 * @brief 重写鼠标按下事件，转发按下信号
 * @param event 鼠标事件对象
 */
void TitleBar::mousePressEvent(QMouseEvent *event)
{
    emit sigMousePress(event->globalPos());
    QWidget::mousePressEvent(event);
}

/**
 * @brief 重写鼠标移动事件，转发移动信号
 * @param event 鼠标事件对象
 */
void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    emit sigMouseMove(event->globalPos());
    QWidget::mouseMoveEvent(event);
}

/**
 * @brief 重写鼠标双击事件，转发双击信号
 * @param event 鼠标事件对象
 */
void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit sigDoubleClick();
    QWidget::mouseDoubleClickEvent(event);
}

/**
 * @brief 重写鼠标释放事件，转发释放信号
 * @param event 鼠标事件对象
 */
void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    emit sigMouseRelease();
    QWidget::mouseReleaseEvent(event);
}

//==================== FramelessWidget 修复核心拖动逻辑 + 扩展 ====================
/**
 * @brief FramelessWidget构造函数，初始化无边框窗口核心配置
 * @param parent 父控件指针
 */
FramelessWidget::FramelessWidget(QWidget *parent)
    : QWidget(parent),
      m_shadowEffect(new QGraphicsDropShadowEffect(this)),
      m_resizeTimer(new QTimer(this)) // 新增：初始化缩放防抖定时器
{
    // 设置无边框窗口标志，保留系统菜单和最小化/最大化按钮能力
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    // 设置透明背景，用于渲染圆角和阴影
    setAttribute(Qt::WA_TranslucentBackground);

    // 新增：Linux虚拟机关闭窗口渲染缓存，避免图形驱动崩溃
#ifdef Q_OS_LINUX
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_StyledBackground, true);
#endif

    // 阴影配置
    m_shadowEffect->setBlurRadius(12);
    m_shadowEffect->setXOffset(0);
    m_shadowEffect->setYOffset(0);
    m_shadowEffect->setColor(QColor(0,0,0,120));
    setGraphicsEffect(m_shadowEffect);
    // 缩放时禁止实时渲染阴影，停止拖拽后再刷新阴影（提升性能）
    m_shadowEffect->setEnabled(false);

    // 主布局初始化
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4,4,4,4);
    m_mainLayout->setSpacing(0);

    m_titleBar = new TitleBar;
    m_contentWidget = new QWidget;
    m_contentWidget->setStyleSheet("background:#ffffff;");

    m_mainLayout->addWidget(m_titleBar);
    m_mainLayout->addWidget(m_contentWidget);

    // 新增：开启悬浮鼠标移动检测，不按左键也触发mouseMove
    setMouseTracking(true);
    // 标题栏信号绑定（新增拖动坐标信号）
    connect(m_titleBar, &TitleBar::sigMinClicked, this, &FramelessWidget::onMinClicked);
    connect(m_titleBar, &TitleBar::sigMaxClicked, this, &FramelessWidget::onMaxClicked);
    connect(m_titleBar, &TitleBar::sigCloseClicked, this, &FramelessWidget::onCloseClicked);
    connect(m_titleBar, &TitleBar::sigDoubleClick, this, &FramelessWidget::onTitleDoubleClick);
    connect(m_titleBar, &TitleBar::sigMousePress, this, &FramelessWidget::onTitleMousePress);
    connect(m_titleBar, &TitleBar::sigMouseMove, this, &FramelessWidget::onTitleMouseMove);
    connect(m_titleBar, &TitleBar::sigMouseRelease, this, &FramelessWidget::onTitleMouseRelease);

    // 新增：缩放防抖定时器配置
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(30); // 30ms防抖间隔
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() {
        if (!m_targetGeo.isNull())
        {
            setGeometry(m_targetGeo); // 防抖结束后设置最终尺寸
            m_targetGeo = QRect();
        }
    });
}

/**
 * @brief 设置窗口圆角半径
 * @param radius 圆角半径（像素）
 */
void FramelessWidget::setWindowRadius(int radius)
{
    m_radius = radius;
    update(); // 触发重绘，应用新的圆角
}

/**
 * @brief 设置阴影效果是否启用
 * @param enable true-启用，false-禁用
 */
void FramelessWidget::setShadowEnable(bool enable)
{
    m_shadowEnable = enable;
    m_shadowEffect->setEnabled(enable);
}

/**
 * @brief 设置窗口是否允许缩放
 * @param enable true-允许，false-禁止
 */
void FramelessWidget::setResizeEnable(bool enable)
{
    m_resizeEnable = enable;
}

/**
 * @brief 设置无标题栏时，是否允许点击空白区域拖动窗口
 * @param enable true-允许，false-禁止
 */
void FramelessWidget::setDragByEmptyArea(bool enable)
{
    m_dragEmptyArea = enable;
}

/**
 * @brief 设置是否保存窗口状态，并指定保存标识
 * @param enable true-启用保存，false-禁用
 * @param saveKey 保存状态的唯一标识
 */
void FramelessWidget::setSaveWindowState(bool enable, const QString &saveKey)
{
    m_saveState = enable;
    m_saveKey = saveKey;
    if(m_saveState) restoreWindowState(); // 启用后立即恢复之前的窗口状态
}

/**
 * @brief 设置标题栏是否可见，并适配布局边距
 * @param visible true-显示，false-隐藏
 */
void FramelessWidget::setTitleBarVisible(bool visible)
{
    m_titleBar->setVisible(visible);
    if (!visible)
    {
        m_mainLayout->setContentsMargins(4, 0, 4, 4); // 隐藏标题栏后，顶部边距置0
    }
    else
    {
        m_mainLayout->setContentsMargins(4, 4, 4, 4); // 显示标题栏时恢复顶部边距
    }
    // 仅保留窗口重绘，移除layout->activate()，杜绝无限循环渲染崩溃
    this->update();
}

/**
 * @brief 获取标题栏可见状态
 * @return bool true-可见，false-隐藏
 */
bool FramelessWidget::isTitleBarVisible() const
{
    return m_titleBar->isVisible();
}

/**
 * @brief 新增：设置窗口是否置顶
 * @param top true-置顶，false-取消置顶
 */
void FramelessWidget::setWindowAlwaysOnTop(bool top)
{
    m_alwaysOnTop = top;
    if (m_alwaysOnTop)
    {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }
    else
    {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }
    show(); // 重新显示窗口使置顶标志生效
}

/**
 * @brief 新增：设置窗口显示/隐藏动画
 * @param duration 动画时长（毫秒，0表示关闭动画）
 * @param type 动画类型（0-淡入淡出，1-缩放+淡入淡出）
 */
void FramelessWidget::setWindowAnimation(int duration, int type)
{
    m_animDuration = duration;
    m_animType = type;
}

/**
 * @brief 新增：设置窗口整体透明度
 * @param opa 透明度（0.0-1.0，1.0为不透明）
 */
void FramelessWidget::setWindowOpacity(qreal opa)
{
    m_windowOpacity = qBound(0.0, opa, 1.0);
    setWindowOpacity(m_windowOpacity); // 调用QWidget的透明度接口
}

/**
 * @brief 新增：设置缩放防抖阈值（避免快速缩放导致窗口抖动）
 * @param threshold 防抖阈值（像素，默认5）
 */
void FramelessWidget::setResizeDebounce(int threshold)
{
    m_resizeDebounce = qMax(1, threshold); // 最小阈值为1
}

/**
 * @brief 新增：设置内容区域控件（替换默认的空白contentWidget）
 * @param widget 自定义内容控件指针
 */
void FramelessWidget::setContentWidget(QWidget* widget)
{
    if (!widget || widget == m_contentWidget) return;
    m_mainLayout->removeWidget(m_contentWidget);
    delete m_contentWidget;
    m_contentWidget = widget;
    m_contentWidget->setStyleSheet("background:#ffffff;");
    m_mainLayout->addWidget(m_contentWidget);
}

/**
 * @brief 新增：多显示器适配：将窗口移动到指定显示器
 * @param screenIndex 显示器索引（从0开始）
 */
void FramelessWidget::moveToScreen(int screenIndex)
{
    QList<QScreen*> screens = QApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.count()) return;
    QScreen* targetScreen = screens[screenIndex];
    QRect screenGeo = targetScreen->availableGeometry();
    // 窗口居中显示在目标显示器
    QRect winGeo = frameGeometry();
    winGeo.moveCenter(screenGeo.center());
    setGeometry(winGeo);
}

/**
 * @brief 新增：窗口动画执行函数（显示/隐藏）
 * @param show true-显示动画，false-隐藏动画
 */
void FramelessWidget::runWindowAnimation(bool show)
{
    if (m_animDuration <= 0) return;

    QPropertyAnimation* anim = new QPropertyAnimation(this, show ? "windowOpacity" : "geometry");
    anim->setDuration(m_animDuration);
    anim->setEasingCurve(QEasingCurve::InOutQuad);

    if (show)
    {
        // 显示动画
        setWindowOpacity(0.0);
        anim->setStartValue(0.0);
        anim->setEndValue(m_windowOpacity);
        if (m_animType == 1)
        {
            // 缩放+淡入淡出
            anim->setTargetObject(this);
            anim->setPropertyName("geometry");
            QRect targetGeo = frameGeometry();
            QRect startGeo = targetGeo;
            startGeo.setSize(QSize(targetGeo.width()*0.8, targetGeo.height()*0.8));
            startGeo.moveCenter(targetGeo.center());
            anim->setStartValue(startGeo);
            anim->setEndValue(targetGeo);

            QPropertyAnimation* opaAnim = new QPropertyAnimation(this, "windowOpacity");
            opaAnim->setDuration(m_animDuration);
            opaAnim->setStartValue(0.0);
            opaAnim->setEndValue(m_windowOpacity);
            opaAnim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    else
    {
        // 隐藏动画
        anim->setStartValue(m_windowOpacity);
        anim->setEndValue(0.0);
        if (m_animType == 1)
        {
            anim->setTargetObject(this);
            anim->setPropertyName("geometry");
            QRect startGeo = frameGeometry();
            QRect endGeo = startGeo;
            endGeo.setSize(QSize(startGeo.width()*0.8, startGeo.height()*0.8));
            endGeo.moveCenter(startGeo.center());
            anim->setStartValue(startGeo);
            anim->setEndValue(endGeo);

            QPropertyAnimation* opaAnim = new QPropertyAnimation(this, "windowOpacity");
            opaAnim->setDuration(m_animDuration);
            opaAnim->setStartValue(m_windowOpacity);
            opaAnim->setEndValue(0.0);
            connect(opaAnim, &QPropertyAnimation::finished, this, &FramelessWidget::hide);
            opaAnim->start(QAbstractAnimation::DeleteWhenStopped);
            return;
        }
        connect(anim, &QPropertyAnimation::finished, this, &FramelessWidget::hide);
    }
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

/**
 * @brief 新增：重写显示事件，支持显示动画
 * @param event 显示事件对象
 */
void FramelessWidget::showEvent(QShowEvent *event)
{
    runWindowAnimation(true);
    QWidget::showEvent(event);
}

/**
 * @brief 新增：重写隐藏事件，支持隐藏动画
 * @param event 隐藏事件对象
 */
void FramelessWidget::hideEvent(QHideEvent *event)
{
    if (m_animDuration > 0)
    {
        runWindowAnimation(false);
        event->ignore(); // 阻止直接隐藏，由动画完成后处理
    }
    else
    {
        QWidget::hideEvent(event);
    }
}

/**
 * @brief 标题栏鼠标按下事件处理，记录拖动起始位置
 * @param globalPos 鼠标按下时的全局坐标
 */
void FramelessWidget::onTitleMousePress(const QPoint &globalPos)
{
    if (isMaximized()) return; // 最大化状态下不允许拖动
    m_windowGeoPress = frameGeometry(); // 记录按下时的窗口几何区域
    m_isDragWindow = true; // 标记开始拖动窗口
    if(m_shadowEnable) m_shadowEffect->setEnabled(false);
    m_mousePressPos = globalPos;
    m_isOperating = true;
}

/**
 * @brief 标题栏鼠标移动事件处理，执行窗口拖动
 * @param globalPos 鼠标移动后的全局坐标
 */
void FramelessWidget::onTitleMouseMove(const QPoint &globalPos)
{
    if (!m_isDragWindow || isMaximized()) return; // 非拖动状态/最大化状态不处理
    QPoint delta = globalPos - m_mousePressPos; // 计算鼠标移动偏移量
    move(m_windowGeoPress.topLeft() + delta); // 移动窗口到新位置
}

/**
 * @brief 标题栏鼠标释放事件处理，结束窗口拖动
 */
void FramelessWidget::onTitleMouseRelease()
{
    m_isDragWindow = false; // 取消拖动标记
    // 新增：拖动结束恢复阴影
        if(m_shadowEnable)
        {
            m_shadowEffect->setEnabled(true);
            update();
        }
        m_isOperating = false;
}

/**
 * @brief 重写鼠标按下事件，处理无标题栏时的拖动和缩放起始逻辑
 * @param event 鼠标事件对象
 */
void FramelessWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_mousePressPos = event->globalPos();
        m_windowGeoPress = this->frameGeometry();
        isOnEdge(event->pos(), m_edgeType); // 判断鼠标是否在缩放边缘

        // 拖拽/缩放开始，关闭阴影，避免实时缩放撕裂
        if(m_shadowEnable)
            m_shadowEffect->setEnabled(false);

        // 无标题栏空白拖动逻辑：非边缘、非最大化、允许空白拖动时，标记为拖动状态
        if (!m_titleBar->isVisible() && m_dragEmptyArea && m_edgeType == 0 && !isMaximized())
        {
            m_isDragWindow = true;
        }
    }
    m_isOperating = true;
    QWidget::mousePressEvent(event); // 转发事件给父类
}

/**
 * @brief 重写鼠标移动事件，处理窗口拖动和八方向缩放（新增防抖）
 * @param event 鼠标事件对象
 */
void FramelessWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint delta = event->globalPos() - m_mousePressPos;
    if(isMaximized()) return; // 最大化状态不处理拖动/缩放

    // 1. 窗口拖动逻辑：标记为拖动时，移动窗口
    if (m_isDragWindow)
    {
        move(m_windowGeoPress.topLeft() + delta);
        return;
    }

    // 2. 八方向缩放逻辑：鼠标在边缘且允许缩放时，调整窗口大小（新增防抖）
    if(m_edgeType != 0 && m_resizeEnable)
    {
        QRect newGeo = m_windowGeoPress;
        switch(m_edgeType)
        {
        case 1: newGeo.setLeft(newGeo.left() + delta.x()); break; // 左边缘：调整左边界
        case 2: newGeo.setRight(newGeo.right() + delta.x()); break; // 右边缘：调整右边界
        case 3: newGeo.setTop(newGeo.top() + delta.y()); break; // 上边缘：调整上边界
        case 4: newGeo.setBottom(newGeo.bottom() + delta.y()); break; // 下边缘：调整下边界
        case 5: newGeo.setLeft(newGeo.left() + delta.x()); newGeo.setTop(newGeo.top() + delta.y()); break; // 左上
        case 6: newGeo.setRight(newGeo.right() + delta.x()); newGeo.setTop(newGeo.top() + delta.y()); break; // 右上
        case 7: newGeo.setLeft(newGeo.left() + delta.x()); newGeo.setBottom(newGeo.bottom() + delta.y()); break; // 左下
        case 8: newGeo.setRight(newGeo.right() + delta.x()); newGeo.setBottom(newGeo.bottom() + delta.y()); break; // 右下
        }

        // 新增：缩放防抖处理
        int deltaX = qAbs(newGeo.width() - m_windowGeoPress.width());
        int deltaY = qAbs(newGeo.height() - m_windowGeoPress.height());
        if (deltaX >= m_resizeDebounce || deltaY >= m_resizeDebounce)
        {
            m_targetGeo = newGeo;
            m_resizeTimer->start(); // 启动防抖定时器
        }
        else
        {
            setGeometry(newGeo); // 小幅度缩放直接生效
        }

        // 仅刷新底部圆角区域，不全局重绘，减少撕裂
        update(QRect(0, rect().height() - m_radius, rect().width(), m_radius));
    }
    else
    {
        int tempEdge = 0;
        isOnEdge(event->pos(), tempEdge);
        updateCursor(tempEdge);
    }
    QWidget::mouseMoveEvent(event); // 转发事件给父类
}

/**
 * @brief 重写鼠标释放事件，结束拖动/缩放，恢复阴影和光标
 * @param event 鼠标事件对象
 */
void FramelessWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_edgeType = 0; // 重置边缘类型
    m_isDragWindow = false; // 重置拖动标记
    setCursor(Qt::ArrowCursor); // 恢复默认光标

    // 缩放/拖动结束，恢复阴影并重绘完整圆角
    if(m_shadowEnable)
    {
        m_shadowEffect->setEnabled(true);
        update();
    }
    m_isOperating = false;
    QWidget::mouseReleaseEvent(event); // 转发事件给父类
}

/**
 * @brief 重写绘制事件，绘制窗口圆角背景
 * @param event 绘制事件对象
 */
void FramelessWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 启用抗锯齿，让圆角更平滑
    // 开启平滑缩放抗锯齿
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 填充整个窗口矩形，无偏移，纯白底色完全覆盖窗口
    QRect drawRect = this->rect();
    painter.setBrush(Qt::white);
    painter.setPen(Qt::transparent);
    painter.drawRoundedRect(drawRect, m_radius, m_radius); // 绘制圆角矩形

    // 拖动/缩放时关闭抗锯齿，直接画矩形，减少绘制耗时
       if(m_isOperating)
       {
           painter.drawRect(drawRect);
       }
       else
       {
           painter.setRenderHint(QPainter::Antialiasing);
           painter.setRenderHint(QPainter::SmoothPixmapTransform);
           painter.drawRoundedRect(drawRect, m_radius, m_radius);
       }
    QWidget::paintEvent(event); // 转发事件给父类
}

/**
 * @brief 重写窗口状态变化事件，处理最大化/还原时的布局边距适配
 * @param event 事件对象
 */
void FramelessWidget::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::WindowStateChange)
    {
        bool max = isMaximized();
        int topMargin = m_titleBar->isVisible() ? 4 : 0;
        if(max)
        {
            m_mainLayout->setContentsMargins(0, 0, 0, 0); // 最大化时移除所有边距，全屏显示
        }
        else
        {
            m_mainLayout->setContentsMargins(4, topMargin, 4, 4); // 还原时恢复边距
        }
        // 更新最大化按钮文本：最大化时显示"□"，还原时显示"▢"
        m_titleBar->maxBtn()->setText(max ? "□" : "▢");
        update(); // 触发重绘
    }
    QWidget::changeEvent(event); // 转发事件给父类
}

/**
 * @brief 重写关闭事件，关闭时保存窗口状态（如果启用了保存）
 * @param event 关闭事件对象
 */
void FramelessWidget::closeEvent(QCloseEvent *event)
{
    if(m_saveState) saveWindowState(); // 保存窗口位置、大小、最大化状态
    QWidget::closeEvent(event); // 转发事件给父类
}

void FramelessWidget::leaveEvent(QEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

/**
 * @brief 判断鼠标是否在窗口缩放边缘，并返回边缘类型
 * @param pos 鼠标本地坐标
 * @param edgeType 输出参数：边缘类型（0-无，1-左，2-右，3-上，4-下，5-左上，6-右上，7-左下，8-右下）
 * @return bool true-在边缘，false-不在边缘
 */
bool FramelessWidget::isOnEdge(const QPoint &pos, int &edgeType)
{
    const int border = 8; // 缩放边缘检测宽度（8像素）
    edgeType = 0;
    int x = pos.x();
    int y = pos.y();
    int w = width();
    int h = height();

    bool left = x < border;
    bool right = x > w - border;
    bool top = y < border;
    bool bottom = y > h - border;

    // 判断具体的边缘类型
    if(left && top) edgeType = 5;
    else if(right && top) edgeType = 6;
    else if(left && bottom) edgeType = 7;
    else if(right && bottom) edgeType = 8;
    else if(left) edgeType = 1;
    else if(right) edgeType = 2;
    else if(top) edgeType = 3;
    else if(bottom) edgeType = 4;

    return edgeType != 0;
}

/**
 * @brief 根据边缘类型更新鼠标光标样式，适配缩放操作
 * @param edgeType 边缘类型（同isOnEdge的edgeType）
 */
void FramelessWidget::updateCursor(int edgeType)
{
    switch (edgeType)
    {
    case 1: case 2: setCursor(Qt::SizeHorCursor); break; // 左右边缘：水平缩放光标
    case 3: case 4: setCursor(Qt::SizeVerCursor); break; // 上下边缘：垂直缩放光标
    case 5: case 8: setCursor(Qt::SizeFDiagCursor); break; // 左上/右下：对角线缩放光标（\方向）
    case 6: case 7: setCursor(Qt::SizeBDiagCursor); break; // 右上/左下：对角线缩放光标（/方向）
    default: setCursor(Qt::ArrowCursor); break; // 非边缘：默认光标
    }
}

/**
 * @brief 从配置文件（FramelessConfig.ini）恢复窗口状态
 * @details 恢复窗口的几何位置（大小/位置）和最大化状态
 */
void FramelessWidget::restoreWindowState()
{
    QSettings set("FramelessConfig.ini", QSettings::IniFormat);
    QString key = m_saveKey + "/geo";
    if(set.contains(key))
    {
        QByteArray geo = set.value(key).toByteArray();
        restoreGeometry(geo); // 恢复窗口位置和大小
    }
    QString maxKey = m_saveKey + "/max";
    if(set.value(maxKey, false).toBool())
        showMaximized(); // 恢复最大化状态
}

/**
 * @brief 将窗口状态保存到配置文件（FramelessConfig.ini）
 * @details 保存窗口的几何位置（大小/位置）和最大化状态
 */
void FramelessWidget::saveWindowState()
{
    QSettings set("FramelessConfig.ini", QSettings::IniFormat);
    QString key = m_saveKey + "/geo";
    set.setValue(key, saveGeometry()); // 保存窗口位置和大小
    set.setValue(m_saveKey + "/max", isMaximized()); // 保存最大化状态
}

/**
 * @brief 最小化按钮点击槽函数，将窗口最小化
 */
void FramelessWidget::onMinClicked()
{
    showMinimized();
}

/**
 * @brief 最大化按钮点击槽函数，切换窗口最大化/还原状态
 */
void FramelessWidget::onMaxClicked()
{
    if(isMaximized()) showNormal();
    else showMaximized();
}

/**
 * @brief 关闭按钮点击槽函数，关闭窗口
 */
void FramelessWidget::onCloseClicked()
{
    close();
}

/**
 * @brief 标题栏双击槽函数，切换窗口最大化/还原状态
 */
void FramelessWidget::onTitleDoubleClick()
{
    onMaxClicked();
}
}
