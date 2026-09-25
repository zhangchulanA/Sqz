#ifndef SqzService_H
#define SqzService_H

/**
 * @class SqzService
 * @brief 所有业务对象（服务、管理器等）的基类，继承自 QObject。
 *        提供与 SqzWidget 同名的通用单例操作接口（Open/Close/Reset/IsExist
 *
 */
#include <QObject>
#include <QThread>
#include <QMetaObject>
#include <QCoreApplication>
#include "SqzClassReg.h"

namespace Sqz {
class SQZ_FRAMEWORK_API SqzService : public QObject
{
    Q_OBJECT
     Q_PROPERTY(bool async READ isAsync)
    friend class SqzHub;
public:
    explicit SqzService(QObject* parent = nullptr);
    virtual ~SqzService();

    // ========== 通用单例操作（与 SqzView 同名但后缀为 Service） ==========
    //打开服务（不存在则创建，存在则激活）
    void OpenService(const QString& className);

    //关闭服务（立即销毁）
    void CloseService(const QString& className);

    //延迟关闭服务（下一事件循环安全销毁，推荐使用）
    void CloseServiceLater(const QString& className);

    //重启服务（关闭后重新打开）
    void RestartService(const QString& className);

    //检查服务是否存在
    bool HasService(const QString& className) const;

protected slots:

    //对象首次创建后回调
    virtual void onInit() {}

    //对象即将销毁前回调
    virtual void onClose() {}

protected:
    // ========== 异步工具（供子类业务代码使用） ==========

    // 在主线程执行 fn（同步服务或已在主线程时直接执行）
    template <typename F>
    void OnMainThread(F&& fn) const
    {
        QCoreApplication* app = QCoreApplication::instance();
        if (!m_isAsync || !app || QThread::currentThread() == app->thread()) {
            fn();
        } else {
            QMetaObject::invokeMethod(app,std::forward<F>(fn),Qt::QueuedConnection);
        }
    }

private:

    // ========== 异步标记（由 SqzHub 在创建时设置） ==========
    bool isAsync() const { return m_isAsync; }
    void setAsync(bool a) { m_isAsync = a; }

    bool m_isAsync = false;
};

}
#endif // SqzService_H
