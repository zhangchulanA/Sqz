// SqzService.h
#ifndef SqzService_H
#define SqzService_H

/**
 * @class SqzService
 * @brief 所有业务对象（服务、管理器等）的基类，继承自 QObject。
 *        提供与 SqzWidget 同名的通用单例操作接口（Open/Close/Reset/IsExist），
 *        但不包含界面相关方法。子类必须实现 className() 纯虚函数，
 *        并确保类名与注册名称一致。推荐使用 SqzHub 创建子类实例。
 */
#include <QObject>
#include "SqzHub.h"
#include "SqzGlobal.h"

namespace Sqz {
class SQZ_FRAMEWORK_API SqzService : public QObject
{
    Q_OBJECT
    friend class SqzHub;
public:
    explicit SqzService(QObject* parent = nullptr);
    virtual ~SqzService();

    // ========== 通用单例操作（与 SqzView 同名但后缀为 Service） ==========

    /// @brief 打开服务（不存在则创建，存在则激活）
    void OpenService(const QString& className);

    /// @brief 关闭服务（立即销毁）
    void CloseService(const QString& className);

    /// @brief 延迟关闭服务（下一事件循环安全销毁，推荐使用）
    void CloseServiceLater(const QString& className);

    /// @brief 重启服务（关闭后重新打开）
    void RestartService(const QString& className);

    /// @brief 检查服务是否存在
    bool HasService(const QString& className) const;

    // ========== 快捷操作（操作自身） ==========

    /// @brief 呼叫自身服务
    void OpenThis();

    /// @brief 杀掉自身服务
    void CloseThis();
protected:
    /**
     * 生命周期回调（由 SqzHub 调用）
     * 不要在构造函数或 onInit() 中调用 OpenSelf() 或 CloseSelf() 等依赖虚函数的方法。
     **/
    /// @brief 对象首次创建后回调
    virtual void onInit() {}

    /// @brief 对象即将销毁前回调
    ///
    virtual void onClose() {}

    /// @brief 获取子类名称（必须实现）
    virtual QString className() const = 0;

    /// @brief 获取qml路径（必须实现）
    virtual QString qmlSource() const = 0;
};
}
#endif // SqzService_H
