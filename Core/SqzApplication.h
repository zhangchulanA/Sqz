#ifndef SQZAPPLICATION_H
#define SQZAPPLICATION_H

#include <QObject>
#include <QEventLoop>
#include <QJsonDocument>
#include <QFile>
#include <QTimer>
#include <memory>
#include <QSet>
#include "SqzGlobal.h"
#include "SqzHub.h"
#include "SqzBus.h"
#include "SqzState.h"
#include "Logger.h"

//APP_PRO_VERSION

namespace Sqz
{
struct SQZ_FRAMEWORK_API AppConfig
{
    // 程序基础元信息
    QString AppName;
    QString DisplayName;
    QString Version;
    QString ThreadPrefix;
    int ExitDelayMs = 500;

    // 后台服务条目
    struct SQZ_FRAMEWORK_API ServiceItem
    {
        QString ClassName;
        bool AutoStart;
        int StartOrder;
        QVariantList Args;
        QVariantMap Props;
    };
    QList<ServiceItem> ServiceList;

    // 统一视图条目（Widget / Quick）
    struct SQZ_FRAMEWORK_API ViewItem
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
    explicit SqzApplication(QObject *parent = nullptr);
    ~SqzApplication() override;

    static SqzApplication* instance();
    // 完整初始化流程：加载配置→注册类→启动组件
    bool Init();

    // 触发延迟安全退出
    void QuitApp();

    //打印全部已注册类
    void LogRegClass();

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
    void BatchRegisterClass();

    // 给QObject批量反射赋值属性
    void ApplyProps(QObject *obj, const QVariantMap &props);

    // 创建全部后台服务
    void CreateServices();

    // 统一创建Widget/Quick所有视图
    void CreateViews();

private slots:
    // 主窗口关闭触发退出流程
    void OnMainWindowClose();
private:
    bool m_resourceReleased = false;
    void ReleaseAllResources();
private:
    AppConfig m_Cfg;
    QObject* m_MainWindow = nullptr;
    bool m_ConfigValid = false;
    bool m_InitComplete = false;

    static SqzApplication* m_s_instance;
};
#define SqzApp SqzApplication::instance()

}
#endif
