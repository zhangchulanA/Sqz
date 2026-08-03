// SqzWidgetOps.h
// QWidget 系视图操作的统一实现（单一数据源）
//
// 设计目的：
//   SqzWidget 与 SqzMainWindow 的视图操作接口（OpenView/CloseView/HideView/...）
//   原本在两个 .cpp 中逐行重复实现，维护时极易遗漏一处。本头文件将所有转发逻辑
//   收敛为 inline 静态方法，两个基类的成员函数改为单行调用，实现"改一处即同步"。
//
// 对外接口零变化：SqzWidget::OpenView(...) 等调用方式完全不变，子类无感。
// 本结构体无 Q_OBJECT，不参与 moc，无多继承风险。
#ifndef SQZWIDGETOPS_H
#define SQZWIDGETOPS_H

#include <QString>
#include "SqzHub.h"   // 提供 SqzIn 宏与 SqzHub::Instance()

namespace Sqz {

/**
 * @brief QWidget 系视图操作的统一转发实现。
 *
 * 所有方法均为 inline static，不持有状态，仅做对 SqzHub 的薄封装。
 * Hub 入口固定为 Widget 系（CreateWidget/HideWidget/ShowWidget/...），
 * 因此仅适用于"底层是 QWidget 派生"的视图基类（SqzWidget / SqzMainWindow）。
 * SqzQuick 走 CreateQuick 系、SqzService 走 CreateObject 系，不适用本类。
 */
struct SqzWidgetOps
{
    // ---------- 通用单例操作 ----------

    /// 打开视图（不存在则创建，存在则激活）
    static void OpenView(const QString& className)        { SqzIn.CreateWidget(className); }

    /// 关闭视图（立即销毁）
    static void CloseView(const QString& className)        { SqzIn.CloseObj(className); }

    /// 延迟关闭视图（下一事件循环安全销毁）
    static void CloseViewLater(const QString& className)  { SqzIn.CloseObjLater(className); }

    /// 重启视图（关闭后重新打开）
    static void RestartView(const QString& className)     { SqzIn.ResetObj(className); }

    /// 检查视图是否存在
    static bool HasView(const QString& className)         { return SqzIn.IsExist(className); }

    // ---------- 视图显隐/位置操作 ----------

    /// 隐藏视图（不销毁）
    static void HideView(const QString& className)         { SqzIn.HideWidget(className); }

    /// 显示视图（并提升到最前）
    static void ShowView(const QString& className)         { SqzIn.ShowWidget(className); }

    /// 切换视图显隐状态
    static void ToggleView(const QString& className)       { SqzIn.ToggleWidget(className); }

    /// 视图是否可见
    static bool IsViewVisible(const QString& className)   { return SqzIn.IsWidgetVisible(className); }

    /// 设置视图置顶
    static void SetViewTopMost(const QString& className, bool topMost) { SqzIn.SetWidgetTop(className, topMost); }

    /// 调整视图大小
    static void ResizeView(const QString& className, int w, int h)   { SqzIn.SetWidgetSize(className, w, h); }

    /// 移动视图位置
    static void MoveView(const QString& className, int x, int y)     { SqzIn.SetWidgetPos(className, x, y); }

    // ---------- 自身快捷操作 ----------
    // callerName 由派生类的 className() 纯虚函数提供

    /// 打开自身
    static void OpenThis(const QString& callerName)   { OpenView(callerName); }

    /// 关闭自身
    static void CloseThis(const QString& callerName) { CloseView(callerName); }

    /// 隐藏自身
    static void HideThis(const QString& callerName)  { HideView(callerName); }

    /// 显示自身
    static void ShowThis(const QString& callerName)  { ShowView(callerName); }
};

} // namespace Sqz
#endif // SQZWIDGETOPS_H
