#ifndef TRANSLATORMASK_H
#define TRANSLATORMASK_H

#include <QWidget>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>
#include "SqzGlobal.h"


namespace Sqz::Utils {
/**
 * @brief 全局轻量半透明遮罩（多屏兼容版）
 * 作用：语言切换过渡防闪烁/防撕裂，支持手动开关
 * 特性：鼠标穿透、无边框、置顶、双平台兼容、零性能损耗、自动适应多屏及屏幕变化
 * 适配：Qt 5.12 Windows/Ubuntu
 */
class SQZ_FRAMEWORK_API TranslatorMask : public QWidget
{
    Q_OBJECT
    static TranslatorMask* s_instance;

    // 私有构造（单例模式，全局唯一）
    explicit TranslatorMask(QWidget *parent = nullptr) : QWidget(parent)
    {
        // 窗口配置：无边框+置顶+不占用任务栏
        setWindowFlags(Qt::FramelessWindowHint
                     | Qt::WindowStaysOnTopHint
                     | Qt::Tool);

        // 半透明遮罩样式：rgba(黑,透明度0.08)，可自行调整
        setStyleSheet("background-color: rgba(0, 0, 0, 0.08);");

        // 关键设置：鼠标事件穿透（遮罩不拦截任何操作）
        setAttribute(Qt::WA_TransparentForMouseEvents);
        // 不抢占窗口焦点
        setAttribute(Qt::WA_ShowWithoutActivating);

        // 默认隐藏，首次显示时自动计算全屏几何
        hide();

        // 监听屏幕变化（分辨率、添加/移除显示器），自动更新遮罩大小
        connect(qApp, &QGuiApplication::primaryScreenChanged, this, &TranslatorMask::updateFullGeometry);
        // 注意：Qt 5.12 中 QScreen 没有 geometryChanged 信号，改用连接所有屏幕的 destroyed 和 应用主屏幕变化简单处理
        // 为了更全面的响应，重写 showEvent 每次显示时重新计算几何即可
    }

public:
    /**
     * @brief 获取单例对象（全局唯一）
     */
    static TranslatorMask* instance()
    {
        if (!s_instance)
            s_instance = new TranslatorMask;
        return s_instance;
    }

    // ====================== 自动调用（语言切换） ======================
    void showMask()
    {
        updateFullGeometry();   // 每次显示前重新计算多屏总几何，确保覆盖所有显示器
        if (!isVisible())
            show();
    }

    void hideMask()
    {
        if (isVisible())
            hide();
    }

    // ====================== 手动调用 ======================
    Q_INVOKABLE void manualShow() { showMask(); }
    Q_INVOKABLE void manualHide() { hideMask(); }

protected:
    void showEvent(QShowEvent *event) override
    {
        updateFullGeometry();   // 保险：显示时再次更新几何
        QWidget::showEvent(event);
    }

private:
    /**
     * @brief 计算所有屏幕的虚拟总几何区域（支持多显示器拼接）
     */
    void updateFullGeometry()
    {
        QRect totalRect;
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            totalRect = totalRect.united(screen->geometry());
        }
        if (totalRect.isValid()) {
            setGeometry(totalRect);
        } else if (QScreen* primary = QGuiApplication::primaryScreen()) {
            setGeometry(primary->geometry());   // 降级：仅主屏
        }
    }
};
}
#endif // TRANSLATORMASK_H
