// SqzQuick.h
#ifndef SqzQuick_H
#define SqzQuick_H

#include <QObject>
#include <QMetaObject>
#include "SqzClassReg.h"
#include "SqzGlobal.h"
#include "SqzApplication.h"
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
    Q_PROPERTY(QString qmlSourcePath READ qmlSourcePath WRITE setQmlSourcePath)
    friend class SqzHub;
    friend class SqzApplication;

public:
    explicit SqzQuick(QObject* parent = nullptr);
    virtual ~SqzQuick();

    // ========== 通用单例操作（与 SqzWidget/SqzService 同名） ==========

    // 打开视图（不存在则创建，存在则激活）
    void OpenView(const QString& className);

    // 关闭视图（立即销毁）
    void CloseView(const QString& className);

    // 延迟关闭视图（下一事件循环安全销毁）
    void CloseViewLater(const QString& className);

    // 重启视图（关闭后重新打开）
    void RestartView(const QString& className);

    // 检查视图是否存在
    bool HasView(const QString& className) const;


protected:
    // 子类可重写以提供动态 QML 源路径（默认返回 m_qmlSourcePath）
    virtual QString qmlSource() const { return m_qmlSourcePath; }

    QString qmlSourcePath() const { return m_qmlSourcePath; }
    void setQmlSourcePath(const QString& path) { m_qmlSourcePath = path; }

protected:

    // 对象首次创建后回调
    virtual void onInit() {}

    // 对象即将销毁前回调
    virtual void onClose() {}

    // 基类提供的方法，用于初始化内部的 m_view
    bool init();

private:

    // 提供对 m_view 的只读访问（如果子类需要查询状态）
    QQuickWindow* window() const { return m_window; }

    QQuickWindow* m_window = nullptr;

    bool m_initialized = false;

    QString m_qmlSourcePath;

};
}
#endif // SqzQuick_H
