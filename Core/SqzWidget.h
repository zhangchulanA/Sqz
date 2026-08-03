// SqzWidget.h
#ifndef SqzWidget_H
#define SqzWidget_H

/**
 * @class SqzWidget
 * @brief 所有窗口/视图界面的基类，继承自 QWidget。
 *        提供与 SqzService 同名的通用单例操作接），
 *        并额外提供窗口专属操作。
 *        子类必须实现 className() 纯虚函数，并确保类名与注册名称一致。
 *        推荐使用 SqzHub 创建子类实例，避免直接 new。
 */
#include <QWidget>
#include <QMetaObject>
#include "SqzGlobal.h"
#include "SqzClassReg.h"
namespace  Sqz {
class SQZ_FRAMEWORK_API SqzWidget : public QWidget
{
    Q_OBJECT
    friend class SqzHub;
public:
    explicit SqzWidget(QWidget* parent = nullptr);
    virtual ~SqzWidget();


    // ========== 通用单例操作（与 SqzWidget/SqzService 同名） ==========

    //打开视图（不存在则创建，存在则激活）
    void OpenView(const QString& className);

    //关闭视图（立即销毁）
    void CloseView(const QString& className);

    //延迟关闭视图（下一事件循环安全销毁）
    void CloseViewLater(const QString& className);

    //重启视图（关闭后重新打开）
    void RestartView(const QString& className);

    //检查视图是否存在
    bool HasView(const QString& className) const;

    // ========== 视图显隐/位置操作 ==========

    //隐藏视图（不销毁）
    void HideView(const QString& className);

    //显示视图（并提升到最前）
    void ShowView(const QString& className);

    //切换视图显隐状态
    void ToggleView(const QString& className);

    //视图是否可见
    bool IsViewVisible(const QString& className) const;

    //设置视图置顶
    void SetViewTopMost(const QString& className, bool topMost);

    //调整视图大小
    void ResizeView(const QString& className, int w, int h);

    //移动视图位置
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
    //对象首次创建后回调
    virtual void onInit() {}

    //对象即将销毁前回调
    virtual void onClose() {}

    //获取子类名称（必须实现）
    virtual QString className() const = 0;

};
}
#endif // SqzWidget_H
