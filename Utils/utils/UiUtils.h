#ifndef UiUtils_H
#define UiUtils_H

#include <QWidget>
#include <QLabel>
#include <QScreen>
#include <QImage>
#include <QDialog>
#include <QMessageBox>
#include <QFont>
#include <QRect>
#include <QTimer>
#include <QObject>
#include <QGraphicsOpacityEffect>
#include <QList>
#include <QMetaObject>
#include <QLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QCursor>
#include <QInputDialog>
#include <QPixmap>
#include <QFile>
#include <QPainter>
#include <QRegExpValidator>
#include <QSettings>
#include <QDebug>
#include <QGuiApplication>
#include <QPoint>
#include <QTransform>
#include "SqzGlobal.h"

namespace Sqz::Utils {
// 弹窗提示类型
enum class SQZ_FRAMEWORK_API TipType
{
    Success,
    Warn,
    Error,
    Info
};

// Toast悬浮窗弹出位置（6个方位）
enum class SQZ_FRAMEWORK_API ToastPos
{
    TopLeft,     // 左上角
    TopRight,    // 右上角
    BottomLeft,  // 左下角
    BottomRight, // 右下角
    TopCenter,   // 顶部居中
    BottomCenter // 底部居中
};

// Toast全局悬浮提示控件（内部私有）
class ToastWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToastWidget(const QString &msg, TipType type, ToastPos pos, int showMs = 2000);
    void showToast();

protected:
    // 窗口透明无边框绘制
    void paintEvent(QPaintEvent *event) override;

private slots:
    // 计算窗口最终坐标
    void calcToastPos();
    // 淡出动画
    void startFadeOut();
private:
    QLabel *m_label;
    ToastPos m_pos;
    TipType m_type;
    int m_showTime;
    QGraphicsOpacityEffect *m_opacityEffect;
    QTimer *m_fadeTimer;
    QTimer *m_delayTimer;

    // 根据TipType获取对应背景色
    QString getBgColor(TipType type);

};

// 无边框拖动辅助内部类
class MovableHelper : public QObject
{
    Q_OBJECT
public:
    explicit MovableHelper(QWidget *target);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    QWidget *m_target;
    QPoint m_mousePressPos;
    QPoint m_widgetOriginPos;
};

// UI静态工具类，全部静态方法
class SQZ_FRAMEWORK_API UiUtils
{
public:
    // ==================== 窗口通用（原有） ====================
    /// 窗口居中（支持主屏幕/指定父窗口）Qt5.12兼容
    static void centerWindow(QWidget *w, QWidget *parent = nullptr);
    /// 设置窗口最小/最大尺寸
    static void setWindowLimit(QWidget *w, int minW, int minH, int maxW = -1, int maxH = -1);
    /// 弹出标准模态提示框（MessageBox）
    static void showTip(const QString &msg, TipType type = TipType::Info, QWidget *parent = nullptr);
    /// 确认弹窗，返回true=点击确定
    static bool showConfirm(const QString &msg, const QString &title = "提示", QWidget *parent = nullptr);

    // ==================== 窗口增强扩展 ====================
    /// 设置无边框窗口可拖动
    static void setMovableWidget(QWidget *w, bool enable = true);
    /// 窗口淡入动画显示
    static void fadeIn(QWidget *w, int duration = 200);
    /// 窗口淡出动画关闭
    static void fadeOut(QWidget *w, int duration = 200);
    /// 设置窗口置顶/取消置顶
    static void setWindowTopmost(QWidget *w, bool top);
    /// 拦截关闭事件，禁止/允许窗口关闭
    static void preventClose(QWidget *w, bool prevent);
    /// 截取窗口画面返回QPixmap
    static QPixmap grabWindow(QWidget *w);
    /// 窗口截图保存到本地文件
    static void saveWindowScreenshot(QWidget *w, const QString &savePath);

    // ==================== 控件样式快捷（原有） ====================
    /// 给控件设置圆角+背景色+边框
    static void setWidgetStyle(QWidget *w, int radius, const QString &bgColor,
                               int borderW = 0, const QString &borderColor = "#cccccc");
    /// 一键设置字体大小加粗
    static void setWidgetFont(QWidget *w, int fontSize, bool bold = false);
    /// 清空控件所有样式表
    static void clearStyle(QWidget *w);

    // ==================== 控件样式扩展 ====================
    /// 给控件添加阴影效果
    static void setShadow(QWidget *w, int blur = 10, const QColor &color = QColor(0,0,0,80));
    /// 设置控件整体透明度 0~1
    static void setOpacity(QWidget *w, qreal opacity);
    /// 扁平化按钮通用样式
    static void setFlatButton(QPushButton *btn);
    /// 调试专用：给控件添加红色边框区分边界
    static void debugBorder(QWidget *w, const QColor &color = Qt::red);

    // ==================== 图片处理（原有） ====================
    /// QImage 等比例缩放
    static QImage scaleImage(const QImage &src, int w, int h);
    /// 图片转Base64字符串
    static QString imageToBase64(const QImage &img, const QString &fmt = "png");
    /// Base64转回图片
    static QImage base64ToImage(const QString &base64Str);
    /// Label加载图片自适应填充
    static void setLabelImage(QLabel *lab, const QString &imgPath, bool keepRatio = true);

    // ==================== 屏幕DPI适配（原有） ====================
    /// 获取主屏幕分辨率
    static QSize getScreenSize();
    /// 获取当前窗口所在屏幕缩放系数
    static qreal getScreenScale(QWidget *w = nullptr);

    // ==================== 屏幕DPI扩展 ====================
    /// 判断当前是否高分屏
    static bool isHighDpi();
    /// 根据屏幕缩放系数自动缩放像素尺寸
    static int dpiScale(int pixel);
    /// 获取鼠标当前所在屏幕
    static QScreen* currentScreen();

    // ==================== 全局悬浮Toast（原有） ====================
    /// 弹出无阻塞悬浮Toast
    /// @param msg 提示文本
    /// @param type 提示类型（成功/警告/错误/普通）
    /// @param pos 弹出方位
    /// @param showMs 停留毫秒，默认2000ms
    static void showToast(const QString &msg, TipType type = TipType::Info,
                          ToastPos pos = ToastPos::BottomRight, int showMs = 2000);

    // ==================== Toast快捷封装扩展 ====================
    static void showSuccessToast(const QString &msg, int ms = 2000);
    static void showErrorToast(const QString &msg, int ms = 2500);

    // ==================== 控件递归查找（新增基础） ====================
    /// 根据控件名称递归查找单个子控件（深层遍历）
    static QWidget* findWidgetByName(QWidget *parent, const QString &objName);
    /// 根据控件名称递归查找所有同名控件
    static QList<QWidget*> findWidgetsByName(QWidget *parent, const QString &objName);
    /// 根据控件类型递归查找所有子控件（深层遍历）
    template <typename T>
    static QList<T*> findWidgetsByType(QWidget *parent);
    /// 根据控件类型递归查找第一个匹配的子控件（深层遍历）
    template <typename T>
    static T* findWidgetByType(QWidget *parent);

    // ==================== 批量控件操作扩展 ====================
    /// 清空父容器内所有输入框内容 LineEdit/TextEdit/ComboBox/SpinBox/CheckBox
    static void clearAllInputs(QWidget *parent);
    /// 批量启用/禁用容器内所有控件
    static void setEnabledAll(QWidget *parent, bool enabled);
    /// 批量显示/隐藏容器内所有控件
    static void setVisibleAll(QWidget *parent, bool visible);
    /// 给指定名称输入框设置占位提示文字
    static void setPlaceHolder(QWidget *parent, const QString &objName, const QString &text);

    // ==================== 输入框限制扩展 ====================
    /// 输入框仅允许数字
    static void setDigitOnly(QLineEdit *edit);
    /// 输入框仅允许浮点数
    static void setDoubleOnly(QLineEdit *edit);
    /// 禁止输入中文
    static void setNoChinese(QLineEdit *edit);
    /// 切换密码明文/密文模式
    static void setPasswordMode(QLineEdit *edit, bool enable);

    // ==================== 布局快捷操作扩展 ====================
    /// 清空布局内所有控件，可选是否销毁控件
    static void clearLayout(QLayout *layout, bool deleteWidget = true);
    /// 销毁控件上绑定的布局并释放所有子控件
    static void destroyLayout(QWidget *w);
    /// 自动调整窗口大小适配内部内容
    static void adjustWindowSize(QWidget *w);

    // ==================== 控件动画扩展 ====================
    /// 控件位移动画
    static void moveAnimation(QWidget *w, const QPoint &endPos, int duration = 300);
    /// 控件缩放动画（Qt5.12兼容，无QGraphicsScaleEffect）
    static void scaleAnimation(QWidget *w, qreal startScale, qreal endScale, int duration = 300);
    /// 控件闪烁提醒动画
    static void blinkWidget(QWidget *w, int times = 3, int interval = 300);

    // ==================== 自定义输入弹窗扩展 ====================
    /// 弹出单行输入框，返回输入内容，取消返回空字符串
    static QString getInputText(const QString &title, const QString &label,
                                const QString &defaultText = "", QWidget *parent = nullptr);

    // ==================== 表单状态持久化扩展 ====================
    /// 保存容器内所有输入控件数据到本地配置
    static void saveInputsState(QWidget *parent, const QString &key);
    /// 从本地配置恢复输入控件数据
    static void restoreInputsState(QWidget *parent, const QString &key);

    // ==================== 调试工具扩展 ====================
    /// 递归打印控件树层级结构，用于调试界面层级
    static void dumpWidgetTree(QWidget *w, int indent = 0);

private:
    // 禁止实例化静态工具类
    UiUtils() = default;

    // 递归查找类型控件内部辅助
    template <typename T>
    static void recursiveFindWidgets(QWidget *parent, QList<T*> &results);
    // 递归查找同名控件辅助
    static void recursiveFindByName(QWidget *parent, const QString &objName, QList<QWidget*> &list);
    // 递归清空输入框辅助
    static void recursiveClearInput(QWidget *parent);
    // 递归批量启用/禁用控件辅助
    static void recursiveSetEnable(QWidget *parent, bool enable);
    // 递归批量显示/隐藏控件辅助
    static void recursiveSetVisible(QWidget *parent, bool visible);
    // 递归保存输入数据
    static void recursiveSaveInput(QWidget *parent, QSettings &settings, const QString &prefix);
    // 递归恢复输入数据
    static void recursiveRestoreInput(QWidget *parent, QSettings &settings, const QString &prefix);
};

// 模板函数实现（头文件内避免链接报错）
template <typename T>
QList<T*> UiUtils::findWidgetsByType(QWidget *parent)
{
    QList<T*> results;
    if (!parent) return results;
    recursiveFindWidgets(parent, results);
    return results;
}

template <typename T>
T* UiUtils::findWidgetByType(QWidget *parent)
{
    QList<T*> widgets = findWidgetsByType<T>(parent);
    return widgets.isEmpty() ? nullptr : widgets.first();
}

template <typename T>
void UiUtils::recursiveFindWidgets(QWidget *parent, QList<T*> &results)
{
    if (!parent) return;
    T* target = qobject_cast<T*>(parent);
    if (target)
        results.append(target);

    const QObjectList &children = parent->children();
    for (QObject *child : children)
    {
        QWidget *childWidget = qobject_cast<QWidget*>(child);
        if (childWidget)
            recursiveFindWidgets(childWidget, results);
    }
}
}
#endif // UiUtils_H
