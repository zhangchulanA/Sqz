#include "SqzApplication.h"
#include "SqzClassReg.h"
#include "SqzQuick.h"
#include "SqzWidget.h"
#include "SqzService.h"
#include <QGuiApplication>
#include <QEvent>
#include <QJsonArray>
#include <QMetaProperty>
#include <QDir>
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
    // 析构阶段事件循环可能已停止，直接同步释放资源
    ReleaseAllResources();
}

SqzApplication *SqzApplication::instance()
{
    return m_s_instance;
}

bool SqzApplication::LoadConfig()
{
    // 解决不同启动方式（双击/服务/systemd/IDE 工作目录）找不到配置的问题
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/SqzAppConfig.json",
        QDir::currentPath() + "/SqzAppConfig.json",
        QCoreApplication::applicationDirPath() + "/config/SqzAppConfig.json",
        QDir::currentPath() + "/config/SqzAppConfig.json",
        QStringLiteral(":/SqzAppConfig.json")  // qrc 内置资源
    };

    QString cfgPath;
    QByteArray rawData;
    bool opened = false;
    for (const QString& path : candidates)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
        {
            cfgPath = path;
            rawData = file.readAll();
            file.close();
            opened = true;
            break;
        }
    }
    if (!opened)
    {
        logwarn << "[SqzApp] 配置文件未找到，已尝试以下路径均失败：";
        for (const QString& p : candidates) logwarn << "    - " << p;
        return false;
    }
    loginfo << "[SqzApp] 加载配置文件:" << cfgPath;

    // QJsonDocument::fromJson 对带 BOM 的数据解析失败。读取后剥离 BOM。
    if (rawData.size() >= 3 &&
            (unsigned char)rawData[0] == 0xEF &&
            (unsigned char)rawData[1] == 0xBB &&
            (unsigned char)rawData[2] == 0xBF)
    {
        rawData = rawData.mid(3);
        loginfo << "[SqzApp] 检测到 UTF-8 BOM，已自动剥离";
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseErr);

    if (parseErr.error != QJsonParseError::NoError)
    {
        // 解析错误时打印字节偏移对应的行号 + 前后上下文，便于大 JSON 定位
        const int offset = parseErr.offset;
        int lineNo = 1;
        int lineStart = 0;
        for (int i = 0; i < offset && i < rawData.size(); ++i) {
            if (rawData[i] == '\n') { ++lineNo; lineStart = i + 1; }
        }
        // 截取错误所在行前后各 80 字符作为上下文
        const int ctxStart = qMax(lineStart, offset - 80);
        const int ctxEnd   = qMin(rawData.size(), offset + 80);
        QString context = QString::fromUtf8(rawData.mid(ctxStart, ctxEnd - ctxStart));
        // 换行转义保证单行输出
        context.replace('\n', "\\n").replace('\r', "\\r");
        logwarn << "[SqzApp] JSON 解析错误:" << parseErr.errorString()
                << " | 行号:" << lineNo
                << " | 字节偏移:" << offset
                << " | 上下文:..." << context << "...";
        return false;
    }
    return ParseJson(doc);
}

bool SqzApplication::ParseJson(const QJsonDocument &doc)
{
    QJsonObject root = doc.object();

    // 修复 A1：JSON 值类型名转换（日志友好输出）
    auto typeName = [](QJsonValue::Type t) -> const char* {
        switch (t) {
        case QJsonValue::Bool:   return "Bool";
        case QJsonValue::Double: return "Number";
        case QJsonValue::String: return "String";
        case QJsonValue::Array:  return "Array";
        case QJsonValue::Object: return "Object";
        case QJsonValue::Null:    return "Null";
        default: return "Undefined";
        }
    };

    // 返回 true 表示"可安全用默认值读取"
    auto checkType = [&](const QString& section, const QString& key,
            const QJsonValue& val, QJsonValue::Type expected) -> bool {
        if (val.type() == QJsonValue::Undefined) return true;   // 缺失，由 toXxx(default) 兜底
        if (val.type() == QJsonValue::Null) {
            logwarn << "[SqzApp] " << section << "." << key << " 为 null，使用默认值";
            return false;
        }
        if (val.type() != expected) {
            logwarn << "[SqzApp] " << section << "." << key
                    << " 类型错误，期望:" << typeName(expected)
                    << " 实际:" << typeName(val.type()) << "，使用默认值";
            return false;
        }
        return true;
    };

    // 解析App基础配置（Version + ThreadPrefix 唯一前缀来源）
    QJsonObject metaObj = root["AppMeta"].toObject();
    // A1：AppMeta 字段类型校验
    checkType("AppMeta", "AppName",      metaObj["AppName"],      QJsonValue::String);
    checkType("AppMeta", "DisplayName",  metaObj["DisplayName"],  QJsonValue::String);
    checkType("AppMeta", "Version",      metaObj["Version"],      QJsonValue::String);
    checkType("AppMeta", "ThreadPrefix", metaObj["ThreadPrefix"], QJsonValue::String);
    checkType("AppMeta", "ExitDelayMs",  metaObj["ExitDelayMs"],  QJsonValue::Double);
    checkType("AppMeta", "StrictVersion", metaObj["StrictVersion"], QJsonValue::Bool);

    m_Cfg.AppName = metaObj["AppName"].toString("");
    m_Cfg.DisplayName = metaObj["DisplayName"].toString("");
    m_Cfg.Version = metaObj["Version"].toString("1.0.0");
    m_Cfg.ThreadPrefix = metaObj["ThreadPrefix"].toString();
    m_Cfg.ExitDelayMs = metaObj["ExitDelayMs"].toInt(500);
    // A7：解析 StrictVersion 开关（版本不匹配时是否 fail-fast）
    m_Cfg.StrictVersion = metaObj["StrictVersion"].toBool(false);

    // 全局设置线程局部前缀，移除pro宏依赖
    SqzHub::SetThreadPrefix(m_Cfg.ThreadPrefix);

    // 解析后台服务
    QJsonArray serviceArr = root["Services"].toArray();
    m_Cfg.ServiceList.clear();
    // A6：Services 内 ClassName 重复检测
    QSet<QString> svcNameSet;
    for (int idx = 0; idx < serviceArr.size(); ++idx)
    {
        QJsonObject obj = serviceArr[idx].toObject();
        AppConfig::ServiceItem s;
        s.ClassName = obj["ClassName"].toString();
        s.AutoStart = obj["AutoStart"].toBool();
        s.StartOrder = obj["StartOrder"].toInt(99);
        s.Critical = obj["Critical"].toBool(false);   // D2
        s.Props = obj["Props"].toObject().toVariantMap();
        QJsonArray argArr = obj["Args"].toArray();
        for (auto arg : argArr) s.Args.append(arg.toVariant());
        m_PropsCache[s.ClassName] = s.Props;
        // A1：Service 字段类型校验
        checkType("Services", "ClassName",  obj["ClassName"],  QJsonValue::String);
        checkType("Services", "AutoStart",   obj["AutoStart"],  QJsonValue::Bool);
        checkType("Services", "StartOrder",  obj["StartOrder"], QJsonValue::Double);
        checkType("Services", "Critical",    obj["Critical"],   QJsonValue::Bool);
        checkType("Services", "Args",        obj["Args"],       QJsonValue::Array);
        checkType("Services", "Props",       obj["Props"],      QJsonValue::Object);

        // A1：ClassName 必填校验（空 ClassName 会导致后续创建失败但无早期 warn）
        if (s.ClassName.isEmpty()) {
            logwarn << "[SqzApp] Services[" << idx << "] 缺少 ClassName，跳过";
            continue;
        }
        // A6：重复 ClassName 检测
        if (svcNameSet.contains(s.ClassName)) {
            logwarn << "[SqzApp] Services 内 ClassName 重复:" << s.ClassName
                    << " | 索引:" << idx << " | 后者会覆盖前者配置";
        }
        svcNameSet.insert(s.ClassName);

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

    // 修复 A3：跟踪 IsMain 出现次数，ParseJson 完成后做唯一性/存在性校验
    int mainViewCount = 0;
    // A6：Views 内 ClassName 重复检测
    QSet<QString> viewNameSet;

    for (int idx = 0; idx < viewArr.size(); ++idx)
    {
        QJsonObject obj = viewArr[idx].toObject();
        AppConfig::ViewItem v;
        v.ViewType = obj["ViewType"].toString();
        v.ClassName = obj["ClassName"].toString();
        v.QmlSource = obj["QmlSource"].toString();
        v.IsMain = obj["IsMain"].toBool(false);
        v.AutoStart = obj["AutoStart"].toBool(true);
        v.Props = obj["Props"].toObject().toVariantMap();
        QJsonArray argArr = obj["Args"].toArray();
        for (auto arg : argArr) v.Args.append(arg.toVariant());
        m_PropsCache[v.ClassName] = v.Props;
        // A1：View 字段类型校验
        checkType("Views", "ViewType", obj["ViewType"], QJsonValue::String);
        checkType("Views", "ClassName", obj["ClassName"], QJsonValue::String);
        if(obj["ViewType"].toString() == "SqzQuick")
            checkType("Views", "QmlSource", obj["QmlSource"], QJsonValue::String);
        checkType("Views", "IsMain", obj["IsMain"], QJsonValue::Bool);
        checkType("Views", "AutoStart", obj["AutoStart"], QJsonValue::Bool);
        checkType("Views", "Props", obj["Props"], QJsonValue::Object);

        // A1：ClassName 必填校验
        if (v.ClassName.isEmpty()) {
            logwarn << "[SqzApp] Views[" << idx << "] 缺少 ClassName，跳过";
            continue;
        }
        // A1：ViewType 必填校验
        if (v.ViewType.isEmpty()) {
            logwarn << "[SqzApp] Views[" << idx << "] 缺少 ViewType:" << v.ClassName;
        }

        //重复 ClassName 检测
        if (viewNameSet.contains(v.ClassName)) {
            logwarn << "[SqzApp] Views 内 ClassName 重复:" << v.ClassName
                    << " | 索引:" << idx;
        }
        viewNameSet.insert(v.ClassName);

        //SqzQuick 视图的 QmlSource 必填（空路径会让 CreateQuick 走缓存空值→初始化失败）
        if (v.ViewType == "SqzQuick" && v.QmlSource.isEmpty())
        {
            logwarn << "[SqzApp] Views[" << idx << "] 类型 SqzQuick 缺少 QmlSource:" << v.ClassName;
        }

        //统计 IsMain=true 的视图数
        if (v.IsMain)
        {
            ++mainViewCount;
        }

        m_Cfg.ViewList.append(v);
    }

    //IsMain 唯一性校验（多个 IsMain:true 会互相覆盖 m_MainWindow + eventFilter 绑错对象）
    if (mainViewCount > 1)
    {
        logwarn << "[SqzApp] 检测到 " << mainViewCount << " 个 IsMain:true 的视图，"
                << "只有最后一个会被设为主窗口，其余的关闭事件无法触发退出流程";
    }
    //IsMain 存在性提示（0 个主窗口：启动后无窗口可关闭，进程无法正常退出）
    else if (mainViewCount == 0)
    {
        logwarn << "[SqzApp] Views 中没有任何 IsMain:true 的视图，进程将无法通过关闭窗口退出";
    }

    // 修复 C3：解析结果 dump（便于启动排错，线上问题直接贴日志对比源 JSON）
    loginfo << "[SqzApp] 配置解析完成 - "
            << "App:" << m_Cfg.AppName << "/" << m_Cfg.Version
            << " | ThreadPrefix:" << m_Cfg.ThreadPrefix
            << " | StrictVersion:" << (m_Cfg.StrictVersion ? "on" : "off")
            << " | Services:" << m_Cfg.ServiceList.size()
            << " | Views:" << m_Cfg.ViewList.size();
    for (const auto& s : m_Cfg.ServiceList)
        loginfo << "[SqzApp]   Service:" << s.ClassName
                << " | AutoStart:" << (s.AutoStart ? "on" : "off")
                << " | Critical:" << (s.Critical ? "on" : "off")
                << " | Order:" << s.StartOrder;
    for (const auto& v : m_Cfg.ViewList)
        loginfo << "[SqzApp]   View:" << v.ClassName
                << " | Type:" << v.ViewType
                << " | IsMain:" << (v.IsMain ? "on" : "off")
                << " | AutoStart:" << (v.AutoStart ? "on" : "off")
                << (v.QmlSource.isEmpty() ? QString() : " | Qml:" + v.QmlSource);

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
        // A7：StrictVersion=true 时 fail-fast（版本跨度大字段增删会导致后续空指针，中止比半崩更安全）
        if (m_Cfg.StrictVersion)
        {
            logerror << "[SqzApp] 版本不匹配且 StrictVersion=true，中止启动"
                     << " | 编译版本:" << compileVer << " | 配置版本:" << configVer;
            return false;
        }
        logwarn << "[SqzApp] 版本不匹配 编译版本:" << compileVer << " 配置版本:" << configVer
                << "（StrictVersion=off，继续启动）";
    }
#endif

    BatchRegisterClass();

    CreateServices();
    CreateViews();

    // 修复 D1：主窗口创建失败时 CreateViews 设置 m_InitFailed=true 并提前 return，此处检查中止 Init
    if (m_InitFailed)
    {
        logerror << "[SqzApp] 因关键组件创建失败，初始化中止";
        return false;
    }

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
    m_hub.PrintRegClass();
}

QString SqzApplication::getViewType(const QString &className) const
{
    for (const auto& v : m_Cfg.ViewList) {
        if (v.ClassName == className) {
            return v.ViewType;
        }
    }
    return QString();
}

// ---------- 通用单例操作 ----------
void SqzApplication::OpenView(const QString& className) {   
    for (const auto& v : m_Cfg.ViewList) {
        if (v.ClassName == className) {
            if (v.ViewType == "SqzWidget") {
                if (v.Args.isEmpty())
                    m_hub.CreateWidget(className, v.Props);
                else
                    m_hub.CreateWidgetWithArg(className, v.Args, v.Props);
            } else if (v.ViewType == "SqzQuick") {
                m_hub.CreateQuick(className, v.QmlSource, v.Props);
            } else {
                logwarn << "[SqzApp] OpenView: 未知 ViewType:" << v.ViewType;
            }
            return;
        }
    }
    logwarn << "[SqzApp] OpenView: 未找到视图配置:" << className;
}

void SqzApplication::CloseView(const QString& className) {
    m_hub.CloseObj(className);
}

void SqzApplication::CloseViewLater(const QString& className) {
    m_hub.CloseObjLater(className);
}

void SqzApplication::RestartView(const QString& className) {
    CloseView(className);
    OpenView(className);
}

bool SqzApplication::HasView(const QString& className) const {
    return m_hub.IsExist(className);
}

// ---------- 界面专属操作 ----------
void SqzApplication::HideView(const QString& className) {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.HideWidget(className);
    } else if (viewType == "SqzQuick") {
        m_hub.HideQuick(className);
    } else {
        logwarn << "[SqzApp] HideView: 未知或未配置的视图:" << className;
    }
}

void SqzApplication::ShowView(const QString &className)
{
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.ShowWidget(className);
    } else if (viewType == "SqzQuick") {
        m_hub.ShowQuick(className);
    } else {
        logwarn << "[SqzApp] ShowView: 未知或未配置的视图:" << className;
    }
}

void SqzApplication::ToggleView(const QString& className) {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.ToggleWidget(className);
    } else if (viewType == "SqzQuick") {
        m_hub.ToggleQuick(className);
    } else {
        logwarn << "[SqzApp] ToggleView: 未知或未配置的视图:" << className;
    }
}

bool SqzApplication::IsViewVisible(const QString& className)const  {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        return m_hub.IsWidgetVisible(className);
    } else if (viewType == "SqzQuick") {
        return m_hub.IsQuickVisible(className);
    } else {
        logwarn << "[SqzApp] IsViewVisible: 未知或未配置的视图:" << className;
        return false;
    }
}

void SqzApplication::SetViewTopMost(const QString& className, bool topMost) {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.SetWidgetTop(className, topMost);
    } else if (viewType == "SqzQuick") {
        m_hub.SetQuickTop(className, topMost);
    } else {
        logwarn << "[SqzApp] SetViewTopMost: 未知或未配置的视图:" << className;
    }
}

void SqzApplication::ResizeView(const QString& className, int w, int h) {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.SetWidgetSize(className, w, h);
    } else if (viewType == "SqzQuick") {
        m_hub.SetQuickSize(className, w, h);
    } else {
        logwarn << "[SqzApp] ResizeView: 未知或未配置的视图:" << className;
    }
}

void SqzApplication::MoveView(const QString& className, int x, int y) {
    QString viewType = getViewType(className);
    if (viewType == "SqzWidget") {
        m_hub.SetWidgetPos(className, x, y);
    } else if (viewType == "SqzQuick") {
        m_hub.SetQuickPos(className, x, y);
    } else {
        logwarn << "[SqzApp] MoveView: 未知或未配置的视图:" << className;
    }
}

// ---------- Service专属操作 ----------
void SqzApplication::OpenService(const QString& className) {
    QVariantMap props;
    for(const auto& v : m_Cfg.ServiceList){
        if(v.ClassName == className){
            props = v.Props;
            break;
        }
    }
    m_hub.CreateObject(className,props);
}

void SqzApplication::CloseService(const QString& className) {
    m_hub.CloseObj(className);
}

void SqzApplication::CloseServiceLater(const QString& className) {
    m_hub.CloseObjLater(className);
}

void SqzApplication::RestartService(const QString& className) {
    CloseService(className);
    OpenService(className);
}

bool SqzApplication::HasService(const QString& className) const {
    return m_hub.IsExist(className);
}

// 运行时打印 "Not a signal" 警告且连接无效，主窗口关闭无法触发退出。改用事件过滤器拦截。
bool SqzApplication::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_MainObject  && event->type() == QEvent::Close) {
        OnMainWindowClose();
        // 返回 false 允许窗口继续正常关闭流程
        return false;
    }
    return QObject::eventFilter(obj, event);
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
    auto& hub = m_hub;
    hub.CloseAll();
}

void SqzApplication::CreateServices()
{
    auto& hub = m_hub;
    for (const auto& s : m_Cfg.ServiceList)
    {
        if (!s.AutoStart) continue;
        QObject* svc = s.Args.isEmpty()
                ? hub.CreateObject(s.ClassName,s.Props)
                : hub.CreateObjectWithArg(s.ClassName, s.Args,s.Props);
        if (!svc)
        {
            //Critical=true 的关键服务创建失败时中止 直接退出）
            if (s.Critical)
            {
                logerror << "[SqzApp] 关键服务创建失败，中止初始化:" << s.ClassName
                         << "（配置 Critical:true）";
                m_InitFailed = true;
                return;
            }
            logwarn << "[SqzApp] 创建服务失败:" << s.ClassName;
            continue;
        }
        //        ApplyProps(svc, s.Props);
        loginfo << "[SqzApp] 自动启动服务:" << s.ClassName
                << (s.Critical ? " (Critical)" : "");
    }
}

// 因 CreateViews 返回 void，改用 m_InitFailed 标志由 Init 检查
void SqzApplication::CreateViews()
{
    auto& hub = m_hub;
    for (const auto& v : m_Cfg.ViewList)
    {
        QObject* viewObj = nullptr;

        // ========== SqzWidget 类型 ==========
        if (v.ViewType == "SqzWidget")
        {
            if (!v.AutoStart)
                continue;

            // 根据有无参数调用不同创建接口（修复重复创建 Bug）
            if (v.Args.isEmpty())
                viewObj = hub.CreateWidget(v.ClassName, v.Props);
            else
                viewObj = hub.CreateWidgetWithArg(v.ClassName, v.Args, v.Props);

            if (!viewObj)
            {
                // D1：主窗口创建失败必须中止 Init
                if (v.IsMain)
                {
                    logerror << "[SqzApp] 主窗口创建失败，中止初始化:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
                logwarn << "[SqzApp] 创建 Widget 失败:" << v.ClassName;
                continue;
            }

            // 主窗口处理（安装事件过滤器）
            if (v.IsMain)
            {
                QWidget* win = qobject_cast<QWidget*>(viewObj);
                if (win)
                {
                    m_MainObject = win;
                    win->installEventFilter(this);
                }
                else
                {
                    logerror << "[SqzApp] IsMain 的 SqzWidget 不是 QWidget，中止:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
            }
        }

        // ========== SqzQuick 类型 ==========
        else if (v.ViewType == "SqzQuick")
        {
            if (!v.AutoStart)
                continue;

            // 创建 Quick 视图（内部已处理 QML 路径缓存及初始化失败回滚）
            viewObj = hub.CreateQuick(v.ClassName, v.QmlSource, v.Props);

            if (!viewObj)
            {
                // 主窗口创建失败必须中止 Init
                if (v.IsMain)
                {
                    logerror << "[SqzApp] 主 Quick 窗口创建失败，中止初始化:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
                logwarn << "[SqzApp] 创建 Quick 视图失败:" << v.ClassName;
                continue;
            }

            // 主窗口处理（连接 closing 信号）
            if (v.IsMain)
            {
                SqzQuick* quick = qobject_cast<SqzQuick*>(viewObj);
                if (quick && quick->window())
                {
                    m_MainObject = quick;
                    QQuickWindow* win = quick->window();
                    // Qt 5.12 支持 QQuickWindow::closing 信号
                   connect(win, &QObject::destroyed, this, &SqzApplication::QuitApp);
                }
                else
                {
                    logerror << "[SqzApp] IsMain 的 SqzQuick 窗口无效，中止:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
            }
        }

        // ========== 未知类型 ==========
        else
        {
            logwarn << "[SqzApp] 未知 ViewType:" << v.ViewType
                    << " | ClassName:" << v.ClassName
                    << " | 跳过该视图（合法值: SqzWidget / SqzQuick）";
            continue;
        }

        // 如果需要应用属性
        // ApplyProps(viewObj, v.Props);
    }
}

void SqzApplication::ApplyProps(QObject *obj, const QVariantMap &props)
{
    if (!obj) return;
    const QMetaObject* meta = obj->metaObject();

    for (auto it = props.begin(); it != props.end(); ++it)
    {
        const QString& propName = it.key();
        const QVariant& value = it.value();
        // 修复 B1：检查属性是否存在（typo 时 setProperty 返回 false 但不报错，难排查）
        int propIdx = meta->indexOfProperty(propName.toUtf8().constData());
        if (propIdx < 0)
        {
            //            logwarn << "[SqzApp] 属性不存在,作为动态属性设置" << propName
            //                    << " | 对象类:" << meta->className();
            continue;
        }
        const QMetaProperty metaProp = meta->property(propIdx);

        // 修复 B2：检查值类型是否与属性类型兼容（如把字符串写给 qreal 属性会静默失败）
        if (!value.isValid()) {
            logwarn << "[SqzApp] 属性值无效:" << propName
                    << " | 对象类:" << meta->className();
            continue;
        }
        // QVariant::canConvert 不完全可靠，但能挡住明显类型不符（字符串→数字等）
        // 对于用户自定义类型，canConvert 永远 true，所以只 warn 明显错误

        QVariant::Type expectedType = metaProp.type();
        if (!value.canConvert(metaProp.type()) &&
                expectedType != QVariant::UserType&&
                expectedType != QVariant::UserType)
        {
            logwarn << "[SqzApp] 属性类型不兼容:" << propName
                    << " | 期望:" << QVariant::typeToName(expectedType)
                    << " | 实际:" << QVariant::typeToName(value.type())
                    << " | 对象类:" << meta->className();
            continue;
        }

        bool ok = obj->setProperty(propName.toUtf8().constData(), value);
        if (!ok)
        {
            logwarn << "[SqzApp] setProperty 失败:" << propName
                    << " | 对象类:" << meta->className();
        }else{
            loginfo <<"属性配置成功" <<propName.toUtf8().constData()<<value;
        }
    }
}

void SqzApplication::BatchRegisterClass()
{
    auto& table = GlobalClassTable();
    auto& hub = m_hub;

    QSet<QString> usedCls;
    for (auto& s : m_Cfg.ServiceList) usedCls.insert(s.ClassName);
    for (auto& v : m_Cfg.ViewList) usedCls.insert(v.ClassName);

    // 修复 A4：检查配置中引用的类是否都已在 GlobalClassTable 注册
    // 未注册的类到 CreateWidget/CreateObject 时才会失败，此处提前 warn 缩短排错链路
    for (const QString& clsName : usedCls) {
        if (!table.contains(clsName)) {
            logwarn << "[SqzApp] 配置引用的类未注册:" << clsName
                    << " | 请检查是否使用 SQZ_REG_NOARG/SQZ_REG_ARG 宏注册该类";
        }
    }

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
            hub.RegisterWithArg(clsName, factory.ArgCreator, factory.IsQObject);
        }
    }
    loginfo << "[SqzApp] 批量注册配置内所有业务类完成";
}
}
