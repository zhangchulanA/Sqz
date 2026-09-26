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
SqzApplication::SqzApplication(int& argc, char** argv)
    :m_app(new QApplication(argc, argv))
{
    m_s_instance = this;
    m_ConfigValid = LoadConfig();

    Init();
}

SqzApplication::~SqzApplication()
{
    QuitApp();
    // m_hub 作为成员，随后自动 ~SqzHub → CloseAll
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
        logwarn << " 配置文件未找到，已尝试以下路径均失败：";
        for (const QString& p : candidates) logwarn << " - " << p;
        return false;
    }
    loginfo << "加载配置文件:" << cfgPath;

    // QJsonDocument::fromJson 对带 BOM 的数据解析失败。读取后剥离 BOM。
    if (rawData.size() >= 3 &&
            (unsigned char)rawData[0] == 0xEF &&
            (unsigned char)rawData[1] == 0xBB &&
            (unsigned char)rawData[2] == 0xBF)
    {
        rawData = rawData.mid(3);
        loginfo << "检测到 UTF-8 BOM，已自动剥离";
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
        logwarn << " JSON 解析错误:" << parseErr.errorString()<< " | 行号:" << lineNo<< " | 字节偏移:" << offset<< " | 上下文:..." << context << "...";
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
            logwarn <<  section << "." << key << " 为 null，使用默认值";
            return false;
        }
        if (val.type() != expected) {
            logwarn << section << "." << key << " 类型错误，期望:" << typeName(expected)<< " 实际:" << typeName(val.type()) << "，使用默认值";
            return false;
        }
        return true;
    };

    // 解析App基础配置（Version + ThreadPrefix 唯一前缀来源）
    QJsonObject metaObj = root["AppMeta"].toObject();
    checkType("AppMeta", "AppName",      metaObj["AppName"],      QJsonValue::String);
    checkType("AppMeta", "Version",      metaObj["Version"],      QJsonValue::String);

    m_Cfg.AppName = metaObj["AppName"].toString("");
    m_Cfg.Version = metaObj["Version"].toString("1.0.0");

    // 全局设置线程局部前缀，移除pro宏依赖
    SqzHub::SetThreadPrefix(m_Cfg.AppName);

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
        s.AutoStart = obj["Auto"].toBool();
        s.StartOrder = obj["Order"].toInt(99);
        s.Async = obj["Async"].toBool(false);
        s.Props = obj["Props"].toObject().toVariantMap();
        //Service 字段类型校验
        checkType("Services", "ClassName",  obj["ClassName"],  QJsonValue::String);
        checkType("Services", "Auto",   obj["Auto"],  QJsonValue::Bool);
        checkType("Services", "Order",  obj["Order"], QJsonValue::Double);
        checkType("Services", "Async", obj["Async"], QJsonValue::Bool);
        checkType("Services", "Props",       obj["Props"],      QJsonValue::Object);

        if (s.ClassName.isEmpty()) {
            logwarn << " Services[" << idx << "] 缺少 ClassName，跳过";
            continue;
        }
        // A6：重复 ClassName 检测
        if (svcNameSet.contains(s.ClassName)) {
            logwarn << " Services 内 ClassName 重复:" << s.ClassName<< " | 索引:" << idx << " | 后者会覆盖前者配置";
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

    // 跟踪 IsMain 出现次数，ParseJson 完成后做唯一性/存在性校验
    int mainViewCount = 0;
    // Views 内 ClassName 重复检测
    QSet<QString> viewNameSet;

    for (int idx = 0; idx < viewArr.size(); ++idx)
    {
        QJsonObject obj = viewArr[idx].toObject();
        AppConfig::ViewItem v;
        v.ViewType = obj["ViewType"].toString();
        v.ClassName = obj["ClassName"].toString();
        v.IsMain = obj["Main"].toBool(false);
        v.AutoStart = obj["Auto"].toBool(true);
        v.Props = obj["Props"].toObject().toVariantMap();
        // View 字段类型校验
        checkType("Views", "ViewType", obj["ViewType"], QJsonValue::String);
        checkType("Views", "ClassName", obj["ClassName"], QJsonValue::String);
        checkType("Views", "Main", obj["Main"], QJsonValue::Bool);
        checkType("Views", "Auto", obj["Auto"], QJsonValue::Bool);
        checkType("Views", "Props", obj["Props"], QJsonValue::Object);

        // ClassName 必填校验
        if (v.ClassName.isEmpty()) {
            logwarn << " Views[" << idx << "] 缺少 ClassName，跳过";
            continue;
        }
        // ViewType 必填校验
        if (v.ViewType.isEmpty()) {
            logwarn << " Views[" << idx << "] 缺少 ViewType:" << v.ClassName;
        }

        //重复 ClassName 检测
        if (viewNameSet.contains(v.ClassName)) {
            logwarn << " Views 内 ClassName 重复:" << v.ClassName
                    << " | 索引:" << idx;
        }
        viewNameSet.insert(v.ClassName);

        //统计 IsMain=true 的视图数
        if (v.IsMain)
        {
            ++mainViewCount;
        }

        m_Cfg.ViewList.append(v);
    }

    if (mainViewCount > 1)
    {
        logwarn << " 检测到 " << mainViewCount << " 个 IsMain:true 的视图，"<< "只有最后一个会被设为主窗口，其余的关闭事件无法触发退出流程";
    }
    //IsMain 存在性提示（0 个主窗口：启动后无窗口可关闭，进程无法正常退出）
    else if (mainViewCount == 0)
    {
        loginfo << " Views 中没有 IsMain:true 的视图";
    }

    //解析结果 dump（便于启动排错，线上问题直接贴日志对比源 JSON）
    loginfo << " 配置解析完成 - "
            << "App:" << m_Cfg.AppName << "/" << m_Cfg.Version
            << " | Services:" << m_Cfg.ServiceList.size()
            << " | Views:" << m_Cfg.ViewList.size();
    for (const auto& s : m_Cfg.ServiceList)
        loginfo << "   Service:" << s.ClassName
                << " | Mode:" << (s.Async ? "async" : "sync")
                << " | AutoStart:" << (s.AutoStart ? "on" : "off")
                << " | Order:" << s.StartOrder;
    for (const auto& v : m_Cfg.ViewList)
        loginfo << "   View:" << v.ClassName
                << " | Type:" << v.ViewType
                << " | IsMain:" << (v.IsMain ? "on" : "off")
                << " | AutoStart:" << (v.AutoStart ? "on" : "off");
    return true;
}

bool SqzApplication::Init()
{
    if (!m_ConfigValid) { logerror << " 配置加载失败，终止初始化"; return false; }

    if (!BatchRegisterClass()) {logerror << " 类注册阶段失败，终止初始化";return false;}

    CreateServices();
    if (m_InitFailed) {
        logerror << " 服务创建失败，终止初始化";
        m_hub.CloseAll();
        SqzBus::ClearAll();
        return false;
    }

    CreateViews();
    if (m_InitFailed) {
        logerror << " 视图创建失败，终止初始化";
        m_hub.CloseAll();
        SqzBus::ClearAll();
        return false;
    }

    qApp->setApplicationName(m_Cfg.AppName);
    qApp->setApplicationDisplayName(m_Cfg.AppName);
    qApp->setApplicationVersion(m_Cfg.Version);
    return true;
}

int SqzApplication::Exec()
{
    return m_app->exec();
}

void SqzApplication::QuitApp()
{
    if (!qApp || m_quitting) return;
    m_quitting = true;
    QMetaObject::invokeMethod(qApp, [this]() {
        m_hub.CloseAll();
        SqzBus::ClearAll();
        qApp->quit();
    }, Qt::QueuedConnection);
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
                m_hub.CreateWidget(className, v.Props);
            } else if (v.ViewType == "SqzQuick") {
                m_hub.CreateQuick(className, v.Props);   // 去掉了 v.QmlSource
            } else {
                logwarn << "未知 ViewType:" << v.ViewType;
            }
            return;
        }
    }
    logwarn << "未找到视图配置:" << className;
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

// ---------- Service专属操作 ----------
void SqzApplication::OpenService(const QString& className) {
    QVariantMap props;
    QVariantList args;
    bool async = false;
    bool found = false;

    for (const auto& v : m_Cfg.ServiceList)
    {
        if (v.ClassName == className)
        {
            props = v.Props;
            async = v.Async;
            found = true;
            break;
        }
    }

    if (!found) {
        logwarn << "未找到服务配置:" << className;
        return;
    }

    if (async) {
        m_hub.CreateObjectAsync(className, props);
    } else {
        m_hub.CreateObject(className, props);
    }
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

void SqzApplication::OnMainWindowClose()
{
    QuitApp();
}

void SqzApplication::CreateServices()
{
    auto& hub = m_hub;
    for (const auto& s : m_Cfg.ServiceList)
    {
        if (!s.AutoStart) continue;

        QObject* svc = nullptr;
        if (s.Async)
        {
            svc = hub.CreateObjectAsync(s.ClassName, s.Props);

        }
        else
        {
            svc = hub.CreateObject(s.ClassName, s.Props);
        }

        if (!svc)
        {
            logerror << " 关键服务创建失败，中止初始化:" << s.ClassName
                     << " | Mode:" << (s.Async ? "async" : "sync");
            m_InitFailed = true;
            return;
        }
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
            if (!v.AutoStart){continue;}
            
            viewObj = hub.CreateWidget(v.ClassName, v.Props);
            if (!viewObj)
            {
                // D1：主窗口创建失败必须中止 Init
                if (v.IsMain)
                {
                    logerror << " 主窗口创建失败，中止初始化:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
                logwarn << " 创建 Widget 失败:" << v.ClassName;
                continue;
            }

            // 主窗口处理（安装事件过滤器）
            if (v.IsMain)
            {
                QWidget* win = qobject_cast<QWidget*>(viewObj);
                if (win)
                {
                    win->setAttribute(Qt::WA_DeleteOnClose);
                    connect(win,&QObject::destroyed,this,&SqzApplication::QuitApp);
                }
                else
                {
                    logerror << " IsMain 的 SqzWidget 不是 QWidget，中止:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
            }
        }

        // ========== SqzQuick 类型 ==========
        else if (v.ViewType == "SqzQuick")
        {
            if (!v.AutoStart){continue;}

            // 创建 Quick 视图（内部已处理 QML 路径缓存及初始化失败回滚）
            viewObj = hub.CreateQuick(v.ClassName,v.Props);

            if (!viewObj)
            {
                // 主窗口创建失败必须中止 Init
                if (v.IsMain)
                {
                    logerror << " 主 Quick 窗口创建失败，中止初始化:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
                logwarn << " 创建 Quick 视图失败:" << v.ClassName;
                continue;
            }

            // 主窗口处理（连接 closing 信号）
            if (v.IsMain)
            {
                SqzQuick* quick = qobject_cast<SqzQuick*>(viewObj);
                if (quick && quick->window())
                {
                    QQuickWindow* win = quick->window();
                    connect(win, &QQuickWindow::destroyed, this, &SqzApplication::QuitApp);

                }
                else
                {
                    logerror << " Main 的 SqzQuick 窗口无效，中止:" << v.ClassName;
                    m_InitFailed = true;
                    return;
                }
            }
        }

        // ========== 未知类型 ==========
        else
        {
            logwarn << " 未知 ViewType:" << v.ViewType
                    << " | ClassName:" << v.ClassName
                    << " | 跳过该视图（合法值: SqzWidget / SqzQuick）";
            continue;
        }
    }
}

bool SqzApplication::BatchRegisterClass()
{
    auto& table = GlobalClassTable();
    auto& hub   = m_hub;

    QSet<QString> usedCls;
    for (auto& s : m_Cfg.ServiceList) usedCls.insert(s.ClassName);
    for (auto& v : m_Cfg.ViewList)    usedCls.insert(v.ClassName);

    bool allOk = true;
    for (const QString& clsName : usedCls) {
        if (!table.contains(clsName)) {
            logerror << " 配置引用的类未注册:" << clsName
                     << " | 请检查是否使用 SQZ_REG 宏注册该类";
            allOk = false;
        }
    }
    if (!allOk) return false;

    for (auto iter = table.begin(); iter != table.end(); ++iter)
    {
        const QString clsName = iter.key();
        if (!usedCls.contains(clsName)) continue;
        const auto& factory = iter.value();

        if (factory.NoArgCreator) {
            if (factory.IsQuick)
                hub.RegisterQuickClass(clsName, factory.NoArgCreator, nullptr);
            else
                hub.RegisterNoArg(clsName, factory.NoArgCreator, nullptr, factory.IsQObject);
        }
        else if (factory.ArgCreator) {
            hub.RegisterWithArg(clsName, factory.ArgCreator, factory.IsQObject);
        }
    }
    return true;
}
}
