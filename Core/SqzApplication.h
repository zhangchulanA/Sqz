#ifndef SQZAPPLICATION_H
#define SQZAPPLICATION_H

#include <QObject>
#include <QEventLoop>
#include <QJsonDocument>
#include <QFile>
#include <QTimer>
#include <memory>
#include <QApplication>
#include <QSet>
#include "SqzGlobal.h"
#include "SqzHub.h"
#include "SqzBus.h"
#include "SqzState.h"
#include "Logger.h"

class QEvent;

//APP_PRO_VERSION

namespace Sqz
{
struct SQZ_FRAMEWORK_API AppConfig
{
    // 程序基础元信息
    QString AppName;
    QString Version;

    // 后台服务条目
    struct ServiceItem
    {
        QString ClassName;
        bool AutoStart;
        int StartOrder;
        bool Async = false;
        QVariantMap Props;
    };
    QList<ServiceItem> ServiceList;

    // 统一视图条目（Widget / Quick）
    struct ViewItem
    {
        QString ViewType;  // "Widget" / "Quick"
        QString ClassName;
        QString QmlSource;
        bool IsMain;
        bool AutoStart;
        QVariantMap Props;
    };
    QList<ViewItem> ViewList;
};

class SQZ_FRAMEWORK_API SqzApplication : public QObject
{
    Q_OBJECT
public:
    // 构造接管程序入口参数，自动创建Core/Gui底层App
    explicit SqzApplication(int& argc, char** argv);
    ~SqzApplication() override;

    static SqzApplication* instance();
    // 完整初始化流程：加载配置→注册类→启动组件
    bool Init();

    int Exec();

    // 触发延迟安全退出
    void QuitApp();

    //打印全部已注册类
    void LogRegClass();
    // 辅助函数：获取视图类型
    QString getViewType(const QString& className) const;
    // 提供访问 Hub 的接口
    SqzHub& hub() { return m_hub; }
public:
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
private:
    // 读取本地 AppConfig.json
    bool LoadConfig();

    // 解析Json文档到配置结构体
    bool ParseJson(const QJsonDocument &doc);

    // 批量把全局注册类灌入SqzHub工厂
    bool BatchRegisterClass();

    // 创建全部后台服务
    void CreateServices();

    // 统一创建Widget/Quick所有视图
    void CreateViews();

private slots:
    // 主窗口关闭触发退出流程
    void OnMainWindowClose();

private:
    std::unique_ptr<QApplication> m_app;

    AppConfig m_Cfg;

    bool m_ConfigValid = false;

    bool m_InitFailed = false;   // D1：主窗口创建失败标志，Init 中检查后返回 false 中止启动

    static SqzApplication* m_s_instance;

    SqzHub m_hub;

    bool m_quitting = false;
};
#define SqzApp SqzApplication::instance()

}
#endif
