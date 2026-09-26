// SqzMainWindow.h
#ifndef SqzMainWindow_H
#define SqzMainWindow_H

/**
 * @class SqzMainWindow
 * @brief 所有窗口/视图界面的基类，继承自 QMainWindow。
 *        提供与 SqzService 同名的通用单例操作接口），
 *        并额外提供窗口专属操作。
 */
#include <QMainWindow>
#include <QMetaObject>
#include "SqzClassReg.h"
#include "SqzGlobal.h"
namespace Sqz {
class SQZ_FRAMEWORK_API SqzMainWindow : public QMainWindow
{
    Q_OBJECT
    friend class SqzHub;
public:
    explicit SqzMainWindow(QWidget* parent = nullptr);
    virtual ~SqzMainWindow();


    // ========== 通用单例操作（与 SqzMainWindow/SqzService 同名） ==========

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

protected:

    //    对象首次创建后回调
    virtual void onInit() {}

    //    对象即将销毁前回调
    virtual void onClose() {}

};
}
#endif // SqzMainWindow_H
