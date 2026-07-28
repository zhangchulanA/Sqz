#include "SqzApplication.h"
#include "SqzClassReg.h"
#include "SqzQuick.h"
#include "SqzWidget.h"
#include "SqzService.h"
#include <QGuiApplication>
#include <QJsonArray>
#include <algorithm>

namespace Sqz
{

SqzApplication* SqzApplication::m_s_instance = nullptr;
SqzApplication::SqzApplication(QObject *parent)
    : QObject(parent)
{
    m_s_instance = this;
    m_ConfigValid = LoadConfig();
}

SqzApplication::~SqzApplication()
{
    // 程序销毁前释放全部托管单例
    QuitApp();
}

SqzApplication *SqzApplication::instance()
{
    return m_s_instance;
}

bool SqzApplication::LoadConfig()
{
    const QString cfgPath = "./SqzAppConfig.json";
    QFile file(cfgPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        logwarn << "[SqzApp] 配置文件打开失败:" << cfgPath << file.errorString();
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();

    if (parseErr.error != QJsonParseError::NoError)
    {
        logwarn << "[SqzApp] Json解析错误:" << parseErr.errorString();
        return false;
    }
    return ParseJson(doc);
}

bool SqzApplication::ParseJson(const QJsonDocument &doc)
{
    QJsonObject root = doc.object();

    // 解析App基础配置（Version + ThreadPrefix 唯一前缀来源）
    QJsonObject metaObj = root["AppMeta"].toObject();
    m_Cfg.AppName = metaObj["AppName"].toString("");
    m_Cfg.DisplayName = metaObj["DisplayName"].toString("");
    m_Cfg.Version = metaObj["Version"].toString("1.0.0");
    m_Cfg.ThreadPrefix = metaObj["ThreadPrefix"].toString();
    m_Cfg.ExitDelayMs = metaObj["ExitDelayMs"].toInt(500);

    // 全局设置线程局部前缀，移除pro宏依赖
    SqzHub::SetThreadPrefix(m_Cfg.ThreadPrefix);

    // 解析后台服务
    QJsonArray serviceArr = root["Services"].toArray();
    m_Cfg.ServiceList.clear();
    for (auto item : serviceArr)
    {
        QJsonObject obj = item.toObject();
        AppConfig::ServiceItem s;
        s.ClassName = obj["ClassName"].toString();
        s.AutoStart = obj["AutoStart"].toBool();
        s.StartOrder = obj["StartOrder"].toInt(99);
        s.Props = obj["Props"].toObject().toVariantMap();
        QJsonArray argArr = obj["Args"].toArray();
        for (auto arg : argArr) s.Args.append(arg.toVariant());
        m_Cfg.ServiceList.append(s);
    }
    // 按启动序号升序排序
    std::sort(m_Cfg.ServiceList.begin(), m_Cfg.ServiceList.end(),
              [](const AppConfig::ServiceItem& a, const AppConfig::ServiceItem& b) {
        return a.StartOrder < b.StartOrder;
    });

    // 统一解析所有Widget/Quick视图
    QJsonArray viewArr = root["Views"].toArray();
    m_Cfg.ViewList.clear();
    for (auto item : viewArr)
    {
        QJsonObject obj = item.toObject();
        AppConfig::ViewItem v;
        v.ViewType = obj["ViewType"].toString();
        v.ClassName = obj["ClassName"].toString();
        v.QmlSource = obj["QmlSource"].toString();
        v.IsMain = obj["IsMain"].toBool(false);
        v.AutoStart = obj["AutoStart"].toBool(true);
        v.Props = obj["Props"].toObject().toVariantMap();
        m_Cfg.ViewList.append(v);
    }

    return true;
}

bool SqzApplication::Init()
{
    if (!m_ConfigValid)
    {
        logerror << "[SqzApp] 配置加载失败，终止初始化";
        return false;
    }

    // 版本匹配校验（pro中定义APP_PRO_VERSION宏与json对比）
#ifdef APP_PRO_VERSION
    const QString compileVer = APP_PRO_VERSION;
    const QString configVer = m_Cfg.Version;
    if (compileVer != configVer)
    {
        logwarn << "[SqzApp] 版本不匹配 编译版本:" << compileVer << " 配置版本:" << configVer;
    }
#endif

    BatchRegisterClass();

    CreateServices();
    CreateViews();

    m_InitComplete = true;

    qApp->setApplicationName(m_Cfg.AppName);
    qApp->setApplicationDisplayName(m_Cfg.DisplayName);
    qApp->setApplicationVersion(m_Cfg.Version);
    return true;
}

void SqzApplication::QuitApp()
{
    QTimer::singleShot(m_Cfg.ExitDelayMs,this,[=](){
        ReleaseAllResources();
        qApp->quit();
    });
}

void SqzApplication::LogRegClass()
{
    SqzIn.PrintRegClass();
}

// ---------- 通用单例操作 ----------
void SqzApplication::OpenView(const QString& className) {
    SqzHub::Instance().CreateWidget(className);
}

void SqzApplication::CloseView(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}

void SqzApplication::CloseViewLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}

void SqzApplication::RestartView(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}

bool SqzApplication::HasView(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}

// ---------- 界面专属操作 ----------
void SqzApplication::HideView(const QString& className) {
    SqzHub::Instance().HideWidget(className);
}

void SqzApplication::ShowView(const QString &className)
{
    SqzHub::Instance().ShowWidget(className);
}

void SqzApplication::ToggleView(const QString& className) {
    SqzHub::Instance().ToggleWidget(className);
}

bool SqzApplication::IsViewVisible(const QString& className) const {
    return SqzHub::Instance().IsWidgetVisible(className);
}

void SqzApplication::SetViewTopMost(const QString& className, bool topMost) {
    SqzHub::Instance().SetWidgetTop(className, topMost);
}

void SqzApplication::ResizeView(const QString& className, int w, int h) {
    SqzHub::Instance().SetWidgetSize(className, w, h);
}

void SqzApplication::MoveView(const QString& className, int x, int y) {
    SqzHub::Instance().SetWidgetPos(className, x, y);
}

void SqzApplication::OpenService(const QString& className) {
    SqzHub::Instance().CreateObject(className);
}

void SqzApplication::CloseService(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}

void SqzApplication::CloseServiceLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}

void SqzApplication::RestartService(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}

bool SqzApplication::HasService(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}


void SqzApplication::OnMainWindowClose()
{
    QuitApp();
}

void SqzApplication::ReleaseAllResources()
{
    if(m_resourceReleased) return;
    m_resourceReleased = true;
    loginfo << "[SqzApp] 收到退出信号，延迟释放资源";
    auto& hub = SqzHub::Instance();
    hub.CloseAll();
}


void SqzApplication::CreateServices()
{
    auto& hub = SqzHub::Instance();
    for (const auto& s : m_Cfg.ServiceList)
    {
        if (!s.AutoStart) continue;
        QObject* svc = s.Args.isEmpty()
                ? hub.CreateObject(s.ClassName)
                : hub.CreateObjectWithArg(s.ClassName, s.Args);
        if (!svc)
        {
            logwarn << "[SqzApp] 创建服务失败:" << s.ClassName;
            continue;
        }
        ApplyProps(svc, s.Props);
        loginfo << "[SqzApp] 自动启动服务:" << s.ClassName;
    }
}

void SqzApplication::CreateViews()
{
    auto& hub = SqzHub::Instance();
    for (const auto& v : m_Cfg.ViewList)
    {
        QObject* viewObj = nullptr;
        if (v.ViewType == "SqzWidget")
        {
            if (v.AutoStart){
                viewObj = hub.CreateWidget(v.ClassName);

                QWidget* win = qobject_cast<QWidget*>(viewObj);
                if (!win)
                {
                    logwarn << "[SqzApp] 创建Widget失败:" << v.ClassName;
                    continue;
                }
                // 绑定主窗口关闭退出
                if (v.IsMain)
                {
                    m_MainWindow = win;
                    connect(win, &QWidget::close, this, &SqzApplication::OnMainWindowClose);
                }
            }

        }
        else if (v.ViewType == "SqzQuick")
        {
            if (v.AutoStart){
                viewObj = hub.CreateQuick(v.ClassName,v.QmlSource);

                SqzQuick* quick = qobject_cast<SqzQuick*>(viewObj);
                if (!quick)
                {
                    logwarn << "[SqzApp] 创建Quick视图失败:" << v.ClassName;
                    continue;
                }
                // 把JSON配置的QmlSource设置到对象属性
                viewObj->setProperty("m_qmlSourcePath", v.QmlSource);
            }
        }
        ApplyProps(viewObj, v.Props);
    }
}

void SqzApplication::ApplyProps(QObject *obj, const QVariantMap &props)
{
    if (!obj) return;
    for (auto it = props.begin(); it != props.end(); ++it)
    {
        obj->setProperty(it.key().toUtf8(), it.value());
    }
}

void SqzApplication::BatchRegisterClass()
{
    auto& table = GlobalClassTable();
    auto& hub = SqzHub::Instance();

    QSet<QString> usedCls;
    for (auto& s : m_Cfg.ServiceList) usedCls.insert(s.ClassName);
    for (auto& v : m_Cfg.ViewList) usedCls.insert(v.ClassName);

    for (auto iter = table.begin(); iter != table.end(); ++iter)
    {
        const QString clsName = iter.key();
        if (!usedCls.contains(clsName)) continue;
        const auto& factory = iter.value();

        if (factory.NoArgCreator)
        {
            // 直接读编译期标记，彻底不用new临时对象
            if (factory.IsQuick)
                hub.RegisterQuickClass(clsName, factory.NoArgCreator, nullptr);
            else
                hub.RegisterNoArg(clsName, factory.NoArgCreator, nullptr, true);
        }
        else if (factory.ArgCreator)
        {
            hub.RegisterWithArg(clsName, factory.ArgCreator);
        }
    }
    loginfo << "[SqzApp] 批量注册配置内所有业务类完成";
}
}
