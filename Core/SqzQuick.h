// SqzQuick.h
#ifndef SqzQuick_H
#define SqzQuick_H

#include <QObject>
#include <QtQuick/QQuickView>
#include "SqzHub.h"
#include "SqzGlobal.h"
/**
 * @class SqzQuick
 * @brief QML 窗口界面的逻辑基类，继承自 QObject。
 *        提供与 SqzWidget 同名的接口，
 *        内部持有 QQuickWindow* 用于实际窗口操作。
 *        子类必须实现 className() 纯虚函数，并注册到 SqzHub。
 *        推荐使用 SqzHub::CreateQuick() 创建单例。
 */

namespace Sqz {
class SQZ_FRAMEWORK_API SqzQuick : public QObject
{
    Q_OBJECT
    friend class SqzHub;

public:
    explicit SqzQuick(QObject* parent = nullptr);
    virtual ~SqzQuick();

    // ========== 通用单例操作（与 SqzWidget/SqzService 同名） ==========

    /// @brief 打开视图（不存在则创建，存在则激活）
    void OpenView(const QString& className);

    /// @brief 关闭视图（立即销毁）
    void CloseView(const QString& className);

    /// @brief 延迟关闭视图（下一事件循环安全销毁）
    void CloseViewLater(const QString& className);

    /// @brief 重启视图（关闭后重新打开）
    void RestartView(const QString& className);

    /// @brief 检查视图是否存在
    bool HasView(const QString& className) const;

    // ========== 视图显隐/位置操作 ==========

    /// @brief 隐藏视图（不销毁）
    void HideView(const QString& className);

    /// @brief 显示视图（并提升到最前）
    void ShowView(const QString& className);

    /// @brief 切换视图显隐状态
    void ToggleView(const QString& className);

    /// @brief 视图是否可见
    bool IsViewVisible(const QString& className) const;

    /// @brief 设置视图置顶
    void SetViewTopMost(const QString& className, bool topMost);

    /// @brief 调整视图大小
    void ResizeView(const QString& className, int w, int h);

    /// @brief 移动视图位置
    void MoveView(const QString& className, int x, int y);

    // ========== 快捷操作（操作自身） ==========

    void OpenThis();      // 打开自身
    void CloseThis();     // 关闭自身
    void HideThis();      // 隐藏自身
    void ShowThis();      // 显示自身

protected:

    /**
     * 生命周期回调（由 SqzHub 调用）
     * 不要在构造函数或 onInit() 中调用 CallSelfView() 或 KillSelfView() 等依赖虚函数的方法。
     **/

    /// @brief 对象首次创建后回调
    virtual void onInit() {}

    /// @brief 对象即将销毁前回调
    virtual void onClose() {}

    /// @brief 获取子类名称（必须实现）
    virtual QString className() const = 0;

    /// @brief 获取qml路径（必须实现）
    virtual QString qmlSource() const = 0;

private:

    /// 基类提供的方法，用于初始化内部的 m_view
    void initializeView();

    /// 提供对 m_view 的只读访问（如果子类需要查询状态）
    QQuickWindow* window() const { return m_window; }

    QQuickWindow* m_window = nullptr;

    bool m_initialized = false;
};
}
#endif // SqzQuick_H
