// SqzWidget.h
#ifndef SqzWidget_H
#define SqzWidget_H

/**
 * @class SqzWidget
 * @brief 所有窗口/视图界面的基类，继承自 QWidget,提供与 SqzService 同名的通用单例操作接）
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

protected:

    //对象首次创建后回调
    virtual void onInit() {}

    //对象即将销毁前回调
    virtual void onClose() {}


};
#define RETURNNAME  QString className() const override{return this->metaObject()->className();}

}
#endif // SqzWidget_H
