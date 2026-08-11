#ifndef SqzViewOps_H
#define SqzViewOps_H

#include <QString>
#include "SqzGlobal.h"

namespace Sqz {

/**
 * @brief 视图操作统一实现（同时支持 SqzWidget 和 SqzQuick）
 */
class SQZ_FRAMEWORK_API SqzViewOps
{
public:
    // ========== 通用单例操作 ==========
    static void OpenView(const QString& className);
    static void CloseView(const QString& className);
    static void CloseViewLater(const QString& className);
    static void RestartView(const QString& className);
    static bool HasView(const QString& className);

    // ========== 视图显隐/位置操作 ==========
    static void HideView(const QString& className);
    static void ShowView(const QString& className);
    static void ToggleView(const QString& className);
    static bool IsViewVisible(const QString& className);
    static void SetViewTopMost(const QString& className, bool topMost);
    static void ResizeView(const QString& className, int w, int h);
    static void MoveView(const QString& className, int x, int y);

    // ========== 快捷操作（操作自身） ==========
    static void OpenThis(const QString& className);
    static void CloseThis(const QString& className);
    static void HideThis(const QString& className);
    static void ShowThis(const QString& className);
};

} // namespace Sqz

#endif // SqzViewOps_H
