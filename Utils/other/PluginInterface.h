// PluginInterface.h
#ifndef PluginInterface_H
#define PluginInterface_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "SqzGlobal.h"
/**
 * @brief 插件统一顶层抽象接口类
 * @author 自定义
 * @date 2026
 * @note 1. 所有业务插件必须继承此类，实现纯虚函数完成自身业务逻辑
 * @note 2. 本接口继承QObject，自带Qt元对象、信号槽、对象树管理能力
 * @note 3. 全局唯一固定IID标识，用于Qt插件加载识别与类型安全转换
 * @note 4. 定义插件通用生命周期与通用业务接口，实现主程序统一调度管理
 * @note 5. 接口文件放置系统公共目录，所有插件工程与主程序共享同一份契约
 * @note 6. 新增通用功能直接在此类扩展纯虚函数，所有插件统一适配
 */
namespace Sqz::Utils {
class SQZ_FRAMEWORK_API PluginInterface :public QObject
{
public:
    virtual ~PluginInterface() = default;

    // 这个返回值：就是你【自己随便定义的插件业务名】
    virtual QString getPluginName() const = 0;
    virtual QString getPluginVersion() const = 0;

    // ------------------- 生命周期 -------------------
    virtual bool init() = 0;          // 初始化
    virtual void start() = 0;         // 启动功能
    virtual void stop() = 0;          // 停止
    virtual void release() = 0;       // 释放资源

    // ------------------- 业务功能（你随便加） -------------------
    // 执行一次主功能
    virtual void doWork() = 0;

    // 设置参数（json格式超级好用）
    virtual void setConfig(const QJsonObject& config) = 0;

    // 获取状态
    virtual QJsonObject getStatus() = 0;

    // 错误信息
    virtual QString getLastError() = 0;
};

#define PluginInterface_IID "com.my.plugin.v1" //"com.my.plugin.v1"字符串自定义 每个插件唯一

}
Q_DECLARE_INTERFACE(Sqz::Utils::PluginInterface, PluginInterface_IID)
#endif

// 导出插件标识，固定写法
//此类的子类内部需要写这俩
//Q_PLUGIN_METADATA(IID PluginInterface_IID)
//Q_INTERFACES(PluginInterface)

//class MyPluginEntry : public QObject, public PluginInterface
//{
//    Q_OBJECT
//    // 导出插件标识，固定写法
//    Q_PLUGIN_METADATA(IID PluginInterface_IID)
//    Q_INTERFACES(PluginInterface)

//public:
//    // 实现接口
//    QString getPluginName() const override
//    {
//        // 这里就是你自定义的插件名，主程序靠这个加载
//        return "我的业务功能插件";
//    }

//    void doWork() override
//    {
//        // ========= 在这里调用你所有写好的功能 =========
//        m_biz.runAllTask();
//        m_biz.func1();
//        m_biz.func2();
//    }

//private:
//    // 实例化你自己的业务功能类
//    MyBizFunc m_biz;
//};
