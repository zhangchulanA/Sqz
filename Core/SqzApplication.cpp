#include "SqzApplication.h"
#include "SqzClassReg.h"
#include "SqzQuick.h"
#include "SqzWidget.h"
#include "SqzService.h"
#include <QGuiApplication>
#include <QEvent>
#include <QJsonArray>
#include <QMetaProperty>   // B1/B2：ApplyProps 用 QMetaProperty 检查属性类型
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
    // （QuitApp 用 singleShot 依赖事件循环，析构时不可靠；修复 Bug #19）
    ReleaseAllResources();
}

SqzApplication *SqzApplication::instance()
{
    return m_s_instance;
}

bool SqzApplication::LoadConfig()
{
    // 修复 C2：路径回退搜索（applicationDirPath → currentPath → qrc 资源）
    // 解决不同启动方式（双击/服务/systemd/IDE 工作目录）找不到配置的问题
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/SqzAppConfig.json",
        QDir::currentPath() + "/SqzAppConfig.json",
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

    // 修复 C4：Windows 记事本保存 UTF-8 会加 BOM（EF BB BF），
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
        // 修复 C1：解析错误时打印字节偏移对应的行号 + 前后上下文，便于大 JSON 定位
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

    // 修复 A1：类型校验辅助，字段存在但类型不符时 warn（字段缺失走默认值，不 warn）
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

        // A6：重复 ClassName 检测
        if (viewNameSet.contains(v.ClassName)) {
            logwarn << "[SqzApp] Views 内 ClassName 重复:" << v.ClassName
                    << " | 索引:" << idx;
        }
        viewNameSet.insert(v.ClassName);

        // 修复 A5：SqzQuick 视图的 QmlSource 必填（空路径会让 CreateQuick 走缓存空值→初始化失败）
        if (v.ViewType == "SqzQuick" && v.QmlSource.isEmpty())
        {
            logwarn << "[SqzApp] Views[" << idx << "] 类型 SqzQuick 缺少 QmlSource:" << v.ClassName;
        }

        // 修复 A3：统计 IsMain=true 的视图数
        if (v.IsMain)
        {
            ++mainViewCount;
        }

        m_Cfg.ViewList.append(v);
    }

    // 修复 A3：IsMain 唯一性校验（多个 IsMain:true 会互相覆盖 m_MainWindow + eventFilter 绑错对象）
    if (mainViewCount > 1)
    {
        logwarn << "[SqzApp] 检测到 " << mainViewCount << " 个 IsMain:true 的视图，"
                << "只有最后一个会被设为主窗口，其余的关闭事件无法触发退出流程";
    }
    // 修复 A3：IsMain 存在性提示（0 个主窗口：启动后无窗口可关闭，进程无法正常退出）
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


// 事件过滤器：拦截主窗口 Close 事件触发退出流程
// 修复 Bug #5：QWidget::close() 是 Q_INVOKABLE slot 而非 signal，原 connect(&QWidget::close,...)
// 运行时打印 "Not a signal" 警告且连接无效，主窗口关闭无法触发退出。改用事件过滤器拦截。
bool SqzApplication::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_MainWindow && event->type() == QEvent::Close) {
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
            // D2：Critical=true 的关键服务创建失败时中止 Init（如 DbService 创建失败，后续业务全崩，不如直接退出）
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
        ApplyProps(svc, s.Props);
        loginfo << "[SqzApp] 自动启动服务:" << s.ClassName
                << (s.Critical ? " (Critical)" : "");
    }
}

// 创建视图的失败标志（用于 D1：主窗口创建失败需中止 Init）
// 用成员变量之外的方式传递，避免改接口；此处用 std::optional<bool> 风格的局部变量 + 返回值
// 因 CreateViews 返回 void，改用 m_InitFailed 标志由 Init 检查

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
                    // 修复 D1：主窗口创建失败必须中止 Init，否则进程"活但无 UI"
                    if (v.IsMain)
                    {
                        logerror << "[SqzApp] 主窗口创建失败，中止初始化:" << v.ClassName;
                        m_InitFailed = true;
                        return;
                    }
                    logwarn << "[SqzApp] 创建Widget失败:" << v.ClassName;
                    continue;
                }
                // 绑定主窗口关闭退出（QWidget::close() 是 slot 非 signal，PMF connect 失效；
                // 改用事件过滤器拦截 QEvent::Close，确保主窗口关闭触发退出流程）
                if (v.IsMain)
                {
                    m_MainWindow = win;
                    win->installEventFilter(this);
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
                // QmlSource 已通过 CreateQuick 参数传入并由 Hub 缓存（修复 Bug #18：原 setProperty 在 QML 加载后无效）
            }
        }
        // 修复 A2：未知 ViewType 原本静默丢弃（无 else 分支），现报 warn
        else
        {
            logwarn << "[SqzApp] 未知 ViewType:" << v.ViewType
                    << " | ClassName:" << v.ClassName
                    << " | 跳过该视图（合法值: SqzWidget / SqzQuick）";
            continue;
        }
        ApplyProps(viewObj, v.Props);
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
            logwarn << "[SqzApp] 属性不存在:" << propName
                    << " | 对象类:" << meta->className()
                    << " | 跳过设置";
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
        if (!value.canConvert(metaProp.type()) &&
                metaProp.type() != QVariant::UserType)
        {
            logwarn << "[SqzApp] 属性类型不兼容:" << propName
                    << " | 期望:" << QVariant::typeToName(metaProp.type())
                    << " | 实际:" << QVariant::typeToName(value.type())
                    << " | 对象类:" << meta->className();
            continue;
        }

        bool ok = obj->setProperty(propName.toUtf8().constData(), value);
        if (!ok)
        {
            logwarn << "[SqzApp] setProperty 失败:" << propName
                    << " | 对象类:" << meta->className();
        }
    }
}

void SqzApplication::BatchRegisterClass()
{
    auto& table = GlobalClassTable();
    auto& hub = SqzHub::Instance();

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
