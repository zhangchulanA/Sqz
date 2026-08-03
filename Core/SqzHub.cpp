#include "SqzHub.h"
#include <QReadLocker>    // 显式包含（修复：原依赖间接包含，跨平台/版本不健壮）
#include <QWriteLocker>   // 显式包含
#include <QThread>
#include <QTimer>
#include "SqzWidget.h"
#include "SqzService.h"
#include "SqzQuick.h"
#include "SqzMainWindow.h"
namespace Sqz {
thread_local QString SqzHub::t_prefix;

SqzHub::SqzHub(QObject *parent) : QObject(parent)
{

}


SqzHub::~SqzHub()
{
    // 析构阶段事件循环已停止，统一调用 destroyAllObjects() 立即同步销毁所有单例
    // （修复 Bug #17：原与 CloseAll 逻辑重复，现共用同一销毁路径）
    destroyAllObjects();
}

ClassMeta SqzHub::getMetaForClass(const QString &fullname)
{
    if (m_noArgCreator.contains(fullname))
        return m_noArgCreator[fullname];
    if (m_argMeta.contains(fullname))
        return m_argMeta[fullname];
    if (m_qmlCreators.contains(fullname))
        return m_qmlCreators[fullname];
    return ClassMeta{};
}

/**
 * @brief 批量销毁池中所有对象的公共实现。
 *
 * 修复 Bug #17：原 ~SqzHub 与 CloseAll 包含完全相同的「取快照→回调 onClose→销毁」逻辑，
 * 维护时极易遗漏一处导致行为不一致。提取为公共方法后，两处入口共享同一销毁路径。
 *
 * 实现要点（亦顺带消除潜在死锁）：
 *   1. 持写锁仅做「快照指针+元数据 并清空池」，随后立即释放锁；
 *   2. onClose() 回调与 immediateDeleter 均在锁外执行——回调可能再次进入 SqzHub
 *      （如服务析构触发 CloseObj），持锁回调会与默认非递归的 QReadWriteLock 形成死锁。
 */
void SqzHub::destroyAllObjects()
{
    QList<void*> deleteList;
    QList<ClassMeta> metaList;
    {
        QWriteLocker locker(&GetFactoryLock());
        for (auto it = m_singlePool.begin(); it != m_singlePool.end(); ++it) {
            deleteList.append(it.value());
            metaList.append(getMetaForClass(it.key()));
        }
        m_singlePool.clear();
    }
    for (int i = 0; i < deleteList.size(); ++i) {
        QObject* obj = static_cast<QObject*>(deleteList[i]);
        // 回调 onClose：通知对象即将销毁，便于释放自身资源
        if (obj) {
            if (auto* view = qobject_cast<SqzWidget*>(obj))
                view->onClose();
            else if (auto* svc = qobject_cast<SqzService*>(obj))
                svc->onClose();
            else if (auto* qmlView = qobject_cast<SqzQuick*>(obj))
                qmlView->onClose();
            else if (auto* mainWin = qobject_cast<SqzMainWindow*>(obj))
                mainWin->onClose();
        }
        // 立即同步销毁（析构/退出阶段事件循环已停或即将停，deleteLater 不可靠）
        if (metaList[i].immediateDeleter) metaList[i].immediateDeleter(deleteList[i]);
        else SafeDelete(deleteList[i], metaList[i].isQObject, true);
    }
}

void *SqzHub::createInternal(const QString &ClassName, std::function<bool (void *)> validator, bool isWidget)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    // 1. UI 对象必须主线程
    if (isWidget && QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 禁止子线程操作UI：" << fullname;
        return nullptr;
    }

    // 2. 第一次检查：池中是否已有对象
    {
        QReadLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            void* existing = m_singlePool[fullname];
            if (isWidget) {
                QWidget* w = static_cast<QWidget*>(existing);
                w->show(); w->raise(); w->activateWindow();
            }
            return existing;
        }
    }

    // 3. 获取元数据
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_noArgCreator.contains(fullname)) {
            logwarn << "[SqzHub] 未注册类：" << fullname;
            return nullptr;
        }
        meta = m_noArgCreator[fullname];
    }

    // 4. 创建对象
    void* raw = meta.creator();
    if (!raw) {
        logwarn << "[SqzHub] 创建对象失败：" << fullname;
        return nullptr;
    }

    // 5. 验证类型（若 validator 返回 false 则删除并返回）
    if (!validator(raw)) {
        if (meta.immediateDeleter) meta.immediateDeleter(raw);
        else SafeDelete(raw, meta.isQObject, false);
        logwarn << "[SqzHub] 类型转换失败：" << fullname;
        return nullptr;
    }

    // 6. 第二次检查：可能其他线程已经创建，防止重复插入
    {
        QWriteLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            // 丢弃刚创建的对象，返回已有对象
            if (meta.immediateDeleter) meta.immediateDeleter(raw);
            else SafeDelete(raw, meta.isQObject, false);
            void* existing = m_singlePool[fullname];
            if (isWidget) {
                QWidget* w = static_cast<QWidget*>(existing);
                w->show(); w->raise(); w->activateWindow();
            }
            return existing;
        }
        // 存入池
        m_singlePool[fullname] = raw;
    }


    if (meta.isQObject) {
        // 7. 对于 QObject，连接 destroyed 信号自动从池移除
        QObject* obj = static_cast<QObject*>(raw);
        connect(obj, &QObject::destroyed, this, [this, fullname, raw]() {
            QWriteLocker locker(&GetFactoryLock());
            // 仅当池中仍是同一对象时移除（防止 ResetObj 后旧对象销毁误删新对象）
            if (m_singlePool.value(fullname) == raw)
                m_singlePool.remove(fullname);
        });

        // 9.新增：调用 onInit
        if (auto* view = qobject_cast<SqzWidget*>(obj))
            view->onInit();
        else if (auto* svc = qobject_cast<SqzService*>(obj))
            svc->onInit();
        else if (auto* mainWin = qobject_cast<SqzMainWindow*>(obj))
            mainWin->onInit();

    }
    // 9. 若是窗口，显示并置前
    if (isWidget) {
        QWidget* w = static_cast<QWidget*>(raw);
        w->show(); w->raise(); w->activateWindow();
    }

    return raw;
}


void SqzHub::SetThreadPrefix(const QString &prefix)
{
    t_prefix = prefix;
}

QString SqzHub::ThreadPrefix()
{
    return t_prefix;
}

QString SqzHub::maybeAddThreadPrefix(const QString &className)
{
    QString prefix = SqzHub::ThreadPrefix();
        if (prefix.isEmpty() || className.contains("::"))
            return className;
        return prefix + "::" + className;
}

// 注册无参类
void SqzHub::RegisterNoArg(const QString& ClassName,
                           std::function<void*()> Creator,
                           std::function<void(void*)> Deleter,
                           bool isQObject)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QWriteLocker locker(&GetFactoryLock());
    if (m_noArgCreator.contains(fullname)) {
        logwarn << "[SqzHub] 重复注册类：" << fullname;
        return;
    }
    // 非 QObject 且未提供 Deleter 时拒绝注册：
    // void* 无法还原真实类型，delete static_cast<char*>(p) 不调用析构函数（UB），且无法销毁资源
    if (!isQObject && !Deleter) {
        logerror << "[SqzHub] 非 QObject 类必须提供 Deleter，拒绝注册：" << fullname;
        return;
    }
    ClassMeta meta;
    meta.creator = std::move(Creator);
    meta.isQObject = isQObject;
    if (Deleter) {
        // 用户提供自定义 Deleter：同时作为延迟与立即销毁器
        meta.deleter = Deleter;
        meta.immediateDeleter = std::move(Deleter);
    } else {
        // QObject 默认：延迟用 deleteLater，立即用 delete
        meta.deleter = [](void* p) { static_cast<QObject*>(p)->deleteLater(); };
        meta.immediateDeleter = [](void* p) { delete static_cast<QObject*>(p); };
    }
    m_noArgCreator[fullname] = std::move(meta);
}

// 注册带参类
void SqzHub::RegisterWithArg(const QString& ClassName, CreatorWithArg Func,
                             bool isQObject,
                             std::function<void(void*)> Deleter)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QWriteLocker locker(&GetFactoryLock());
    if (m_argCreator.contains(fullname)) {
        logwarn << "[SqzHub] 重复注册带参类：" << fullname;
        return;
    }
    // 非 QObject 带参类未提供 Deleter：无法安全销毁，拒绝注册
    if (!isQObject && !Deleter) {
        logerror << "[SqzHub] 非 QObject 带参类必须提供 Deleter，拒绝注册：" << fullname;
        return;
    }
    m_argCreator[fullname] = std::move(Func);
    // 同步存入元数据表，供 CloseObj/CloseAll 销毁时查询 deleter/isQObject
    ClassMeta meta;
    meta.isQObject = isQObject;
    if (Deleter) {
        meta.deleter = Deleter;
        meta.immediateDeleter = std::move(Deleter);
    } else {
        meta.deleter = [](void* p) { static_cast<QObject*>(p)->deleteLater(); };
        meta.immediateDeleter = [](void* p) { delete static_cast<QObject*>(p); };
    }
    m_argMeta[fullname] = std::move(meta);
}

void SqzHub::RegisterQuickClass(const QString &ClassName, std::function<void *()> Creator, std::function<void (void *)> Deleter)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QWriteLocker locker(&GetFactoryLock());
    if (!m_qmlCreators.contains(fullname)) {
        ClassMeta meta;
        meta.creator = std::move(Creator);
        meta.isQObject = true;  // QML 窗口肯定是 QObject
        if (Deleter) {
            meta.deleter = Deleter;
            meta.immediateDeleter = std::move(Deleter);
        } else {
            // Quick 默认用 delete（QQuickWindow 需立即释放避免窗口残留）
            meta.deleter = [](void* p) { delete static_cast<QObject*>(p); };
            meta.immediateDeleter = [](void* p) { delete static_cast<QObject*>(p); };
        }
        m_qmlCreators[fullname] = std::move(meta);
    } else {
        logwarn << "[SqzHub] 重复注册 QML 类：" << fullname;
    }
}

// 创建窗口单例（主线程专用）
QWidget* SqzHub::CreateWidget(const QString& ClassName)
{
    return static_cast<QWidget*>(createInternal(ClassName,
                                                [](void* p) { return qobject_cast<QWidget*>(static_cast<QObject*>(p)) != nullptr; },
    true));
}

// 创建QObject单例
QObject* SqzHub::CreateObject(const QString& ClassName)
{
    return static_cast<QObject*>(createInternal(ClassName,
                                                [](void* p) { return qobject_cast<QObject*>(static_cast<QObject*>(p)) != nullptr; },
    false));
}

// 创建普通类单例
void* SqzHub::CreateRawObj(const QString& ClassName)
{
    return createInternal(ClassName,
                          [](void* p) { return true; }, // 任何指针都接受
    false);
}

QObject *SqzHub::CreateQuick(const QString &ClassName,const QString& qmlpath)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 禁止子线程操作QML UI：" << fullname;
        return nullptr;
    }
    // 第一次检查：池中是否已有对象
    {
        QReadLocker locker(&GetFactoryLock());;
        if (m_singlePool.contains(fullname)) {
            QObject* obj = static_cast<QObject*>(m_singlePool[fullname]);
            // 激活窗口
            QMetaObject::invokeMethod(obj, "show");
            QMetaObject::invokeMethod(obj, "raise");
            QMetaObject::invokeMethod(obj, "requestActivate");
            return obj;
        }
    }

    // 获取 QML 类的元数据
    ClassMeta meta;
    QString actualQmlPath = qmlpath;
    {
        QWriteLocker locker(&GetFactoryLock());
        if (!m_qmlCreators.contains(fullname)) {
            logwarn << "[SqzHub] 未注册 QML 类：" << fullname;
            return nullptr;
        }
        meta = m_qmlCreators[fullname];
        // 存储或读取 qmlpath（供 ResetObj 重建使用，修复 Bug #18：ResetObj 用空 qmlpath 重建失败）
        if (!actualQmlPath.isEmpty())
            m_quickQmlPath[fullname] = actualQmlPath;
        else
            actualQmlPath = m_quickQmlPath.value(fullname);
    }

    // 创建 QML 逻辑对象（子类实例）
    void* raw = meta.creator();
    if (!raw) {
        logwarn << "[SqzHub] 创建 QML 对象失败：" << fullname;
        return nullptr;
    }

    // 类型转换
    QObject* qmlObj = static_cast<QObject*>(raw);
    SqzQuick* view = qobject_cast<SqzQuick*>(qmlObj);
    if (!view) {
        meta.immediateDeleter(raw);
        logwarn << "[SqzHub] 类型转换失败（需要 SqzQuick）：" << fullname;
        return nullptr;
    }
    view->initializeView(actualQmlPath);

    // 存入池
    QWriteLocker locker(&GetFactoryLock());
    if (m_singlePool.contains(fullname)) {
        // 其他线程已创建，丢弃本对象
        meta.immediateDeleter(raw);
        if (view->window()) {
            view->window()->show();
            view->window()->raise();
            view->window()->requestActivate();
        }
        return qmlObj;
    }
    m_singlePool[fullname] = raw;

    // 连接销毁信号
    connect(qmlObj, &QObject::destroyed, this, [this, fullname, raw]() {
        QWriteLocker locker(&GetFactoryLock());
        // 仅当池中仍是同一对象时移除（防止 ResetObj 后旧对象销毁误删新对象）
        if (m_singlePool.value(fullname) == raw)
            m_singlePool.remove(fullname);
    });


    // 显示窗口
    if (view->window()) {
        view->window()->show();
        view->window()->raise();
        view->window()->requestActivate();
    }

    return qmlObj;
}

QObject *SqzHub::GetQuickObject(const QString &ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    if (m_singlePool.contains(fullname)) {
        return static_cast<QObject*>(m_singlePool[fullname]);
    }
    return nullptr;
}

QWidget *SqzHub::CreateWidgetWithArg(const QString &ClassName, const QVariantList &args)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 禁止子线程操作UI：" << fullname;
        return nullptr;
    }

    // 检查是否已存在
    {
        QReadLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            QWidget* w = static_cast<QWidget*>(m_singlePool[fullname]);
            w->show(); w->raise(); w->activateWindow();
            return w;
        }
    }

    // 获取带参构造器
    CreatorWithArg creator;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_argCreator.contains(fullname)) {
            logwarn << "[SqzHub] 未注册带参类：" << fullname;
            return nullptr;
        }
        creator = m_argCreator[fullname];
    }

    // 创建对象
    void* raw = creator(args);
    if (!raw) {
        logwarn << "[SqzHub] 带参创建对象失败：" << fullname;
        return nullptr;
    }

    QWidget* widget = qobject_cast<QWidget*>(static_cast<QObject*>(raw));
    if (!widget) {
        // 若创建的不是 QWidget，需释放（假设是 QObject，用 deleteLater）
        QObject* obj = static_cast<QObject*>(raw);
        obj->deleteLater();
        logwarn << "[SqzHub] 带参创建不是 QWidget：" << fullname;
        return nullptr;
    }

    // 存入池
    {
        QWriteLocker locker(&GetFactoryLock());
        // 再次检查，防止竞态
        if (m_singlePool.contains(fullname)) {
            widget->deleteLater(); // 丢弃新对象
            QWidget* existing = static_cast<QWidget*>(m_singlePool[fullname]);
            existing->show(); existing->raise(); existing->activateWindow();
            return existing;
        }
        m_singlePool[fullname] = widget;
    }

    // 连接销毁信号
    connect(widget, &QWidget::destroyed, this, [this, fullname, widget]() {
        QWriteLocker locker(&GetFactoryLock());
        // 仅当池中仍是同一对象时移除（防止 ResetObj 后旧对象销毁误删新对象）
        if (m_singlePool.value(fullname) == widget)
            m_singlePool.remove(fullname);
    });

    // 触发生命周期回调（与无参路径 createInternal 保持一致）
    if (auto* view = qobject_cast<SqzWidget*>(widget))
        view->onInit();
    else if (auto* mainWin = qobject_cast<SqzMainWindow*>(widget))
        mainWin->onInit();

    widget->show(); widget->raise(); widget->activateWindow();
    return widget;
}

QObject *SqzHub::CreateObjectWithArg(const QString &ClassName, const QVariantList &args)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    // 检查是否已存在
    {
        QReadLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname))
            return static_cast<QObject*>(m_singlePool[fullname]);
    }

    CreatorWithArg creator;
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_argCreator.contains(fullname)) {
            logwarn << "[SqzHub] 未注册带参类：" << fullname;
            return nullptr;
        }
        creator = m_argCreator[fullname];
        meta = m_argMeta.value(fullname);
    }

    void* raw = creator(args);
    if (!raw) return nullptr;

    QObject* obj = qobject_cast<QObject*>(static_cast<QObject*>(raw));
    if (!obj) {
        // 非 QObject：用注册的 deleter 安全释放（避免 delete char* 的 UB）
        if (meta.immediateDeleter) meta.immediateDeleter(raw);
        else delete static_cast<char*>(raw);
        logwarn << "[SqzHub] 带参创建不是 QObject：" << fullname;
        return nullptr;
    }

    {
        QWriteLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            obj->deleteLater();
            return static_cast<QObject*>(m_singlePool[fullname]);
        }
        m_singlePool[fullname] = obj;
    }

    connect(obj, &QObject::destroyed, this, [this, fullname, obj]() {
        QWriteLocker locker(&GetFactoryLock());
        // 仅当池中仍是同一对象时移除（防止 ResetObj 后旧对象销毁误删新对象）
        if (m_singlePool.value(fullname) == obj)
            m_singlePool.remove(fullname);
    });

    // 触发生命周期回调（与无参路径 createInternal 保持一致）
    if (auto* svc = qobject_cast<SqzService*>(obj))
        svc->onInit();
    else if (auto* qmlView = qobject_cast<SqzQuick*>(obj))
        qmlView->onInit();

    return obj;
}

// 判断对象是否存在
bool SqzHub::IsExist(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    return m_singlePool.contains(fullname);
}


// 立即销毁对象
void SqzHub::CloseObj(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    ClassMeta meta;
    void* ptr = nullptr;
    {
        QWriteLocker locker(&GetFactoryLock());
        if (!m_singlePool.contains(fullname)) return;
        ptr = m_singlePool.take(fullname);
        meta = getMetaForClass(fullname);
    }

    QObject* obj = static_cast<QObject*>(ptr);
    // ---------- 调用 onBeforeClose ----------
    if (obj) {
        if (auto* view = qobject_cast<SqzWidget*>(obj))
            view->onClose();
        else if (auto* svc = qobject_cast<SqzService*>(obj))
            svc->onClose();
        else if (auto* qmlView = qobject_cast<SqzQuick*>(obj))
            qmlView->onClose();
        else if (auto* mainWin = qobject_cast<SqzMainWindow*>(obj))
            mainWin->onClose();
    }
    // 立即销毁（兑现"CloseObj 立即"语义，且避免 ResetObj 后旧对象 destroyed 误删新对象）
    if (meta.immediateDeleter) meta.immediateDeleter(ptr);
    else SafeDelete(ptr, meta.isQObject, true);
}

// 延迟销毁对象
void SqzHub::CloseObjLater(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QTimer::singleShot(0, this, [=](){ CloseObj(fullname); });
}

void SqzHub::DeleteTemp(const QString &ClassName, void *ptr)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    if (m_noArgCreator.contains(fullname)) {
        auto& meta = m_noArgCreator[fullname];
        if (meta.immediateDeleter) meta.immediateDeleter(ptr);
        else SafeDelete(ptr, meta.isQObject);
    } else {
        logwarn << "[SqzHub] DeleteTemp：未注册类" << fullname;
    }
}

// 重置对象（销毁+重建）
void SqzHub::ResetObj(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    bool isWidget = false;
    bool isQml = false;
    bool isQObj = false;
    {
        QReadLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname))
        {
            void* ptr = m_singlePool[fullname];
            QObject* obj = static_cast<QObject*>(ptr);
            if (qobject_cast<QWidget*>(obj)) {
                isWidget = true;
            } else if (qobject_cast<SqzQuick*>(obj)) {
                isQml = true;   // 新增 QML 判断
            } else if (qobject_cast<QObject*>(obj)) {
                isQObj = true;
            }
        }
    }
    CloseObj(fullname);
    if (isWidget) CreateWidget(fullname);
    else if (isQml) CreateQuick(fullname);  // 新增
    else if (isQObj) CreateObject(fullname);
    else CreateRawObj(fullname);
}

// 创建临时对象（不入池）
void* SqzHub::CreateTemp(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    // 修复 Bug #20：原实现持读锁调用 creator()，若回调再次进入 SqzHub
    // （如构造期间触发 Register/CreateObject），与默认非递归 QReadWriteLock 形成死锁。
    // 改为锁内仅拷贝 functor，锁外执行 creator()。
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_noArgCreator.contains(fullname)) return nullptr;
        meta = m_noArgCreator[fullname];
    }
    return meta.creator();
}

// 安全释放裸指针
void SqzHub::SafeDelete(void* Ptr, bool isQObject, bool immediate)
{
    if (!Ptr) return;
    if (isQObject) {
        QObject* obj = static_cast<QObject*>(Ptr);
        if (immediate)
            delete obj;
        else
            obj->deleteLater();
    } else {
        // 兜底路径：void* 无法还原真实类型，delete char* 不调用实际析构函数（对含资源成员的类型为 UB）
        // 正常销毁应通过 ClassMeta.deleter 完成；注册时已强制非 QObject 类提供 deleter
        delete static_cast<char*>(Ptr);
    }
}

// 隐藏窗口
void SqzHub::HideWidget(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (w) w->hide();
}

void SqzHub::ShowWidget(const QString &ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (w) w->show();
}

// 切换窗口显示状态
void SqzHub::ToggleWidget(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (!w) return;
    if (w->isVisible()) w->hide();
    else { w->show(); w->raise(); }
}

// 判断窗口是否可见
bool SqzHub::IsWidgetVisible(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QWidget* w = GetWidgetPtr(fullname);
    return w ? w->isVisible() : false;
}

// 设置窗口置顶
void SqzHub::SetWidgetTop(const QString& ClassName, bool TopMost)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (w) { w->setWindowFlag(Qt::WindowStaysOnTopHint, TopMost); w->show(); }
}

// 设置窗口大小
void SqzHub::SetWidgetSize(const QString& ClassName, int W, int H)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (w) w->resize(W, H);
}

// 设置窗口位置
void SqzHub::SetWidgetPos(const QString& ClassName, int X, int Y)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI：" << fullname; return; }
    QWidget* w = GetWidgetPtr(fullname);
    if (w) w->move(X, Y);
}

// 获取窗口指针
QWidget* SqzHub::GetWidgetPtr(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QReadLocker locker(&GetFactoryLock());
    if (!m_singlePool.contains(fullname)) return nullptr;
    return static_cast<QWidget*>(m_singlePool[fullname]);
}

// 隐藏所有窗口
void SqzHub::HideAllWidget()
{
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    { logwarn << "[SqzHub] 子线程不可操作UI"; return; }
    QReadLocker locker(&GetFactoryLock());
    for (auto ptr : m_singlePool)
    {
        QWidget* w = qobject_cast<QWidget*>(static_cast<QObject*>(ptr));
        if (w) w->hide();
    }
}


// ==================== Quick 窗口专属操作实现 ====================

/// @brief 隐藏指定 Quick 窗口（不销毁对象）
void SqzHub::HideQuick(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (view && view->window()) {
        view->window()->hide();
    }
}

/// @brief 显示指定 Quick 窗口
void SqzHub::ShowQuick(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (view && view->window()) {
        view->window()->show();
        view->window()->raise();
        view->window()->requestActivate();
    }
}

/// @brief 切换 Quick 窗口的显示/隐藏状态
void SqzHub::ToggleQuick(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (!view || !view->window()) return;

    if (view->window()->isVisible()) {
        view->window()->hide();
    } else {
        view->window()->show();
        view->window()->raise();
        view->window()->requestActivate();
    }
}

/// @brief 判断 Quick 窗口是否当前可见
bool SqzHub::IsQuickVisible(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return false;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    return view && view->window() ? view->window()->isVisible() : false;
}

/// @brief 设置 Quick 窗口置顶或取消置顶
void SqzHub::SetQuickTop(const QString& ClassName, bool TopMost)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (view && view->window()) {
        view->window()->setFlag(Qt::WindowStaysOnTopHint, TopMost);
    }
}

/// @brief 设置 Quick 窗口大小
void SqzHub::SetQuickSize(const QString& ClassName, int W, int H)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (view && view->window()) {
        view->window()->resize(W, H);
    }
}

/// @brief 设置 Quick 窗口在屏幕上的位置
void SqzHub::SetQuickPos(const QString& ClassName, int X, int Y)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 子线程不可操作Quick UI：" << fullname;
        return;
    }

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    if (view && view->window()) {
        view->window()->setX(X);
        view->window()->setY(Y);
    }
}

/// @brief 获取 Quick 窗口的原生 QQuickWindow 指针
QQuickWindow* SqzHub::GetQuickPtr(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QObject* obj = GetQuickObject(fullname);
    if (!obj) return nullptr;

    SqzQuick* view = qobject_cast<SqzQuick*>(obj);
    return view ? view->window() : nullptr;
}



// 判断类是否已注册
bool SqzHub::IsClassReg(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QReadLocker locker(&GetFactoryLock());
    return m_noArgCreator.contains(fullname) || m_argCreator.contains(fullname);
}

// 判断实例是否为窗口
bool SqzHub::IsWidgetObj(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QReadLocker locker(&GetFactoryLock());
    if (!m_singlePool.contains(fullname)) return false;
    return qobject_cast<QWidget*>(static_cast<QObject*>(m_singlePool[fullname])) != nullptr;
}

// 判断实例是否为QObject
bool SqzHub::IsQObject(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QReadLocker locker(&GetFactoryLock());
    if (!m_singlePool.contains(fullname)) return false;
    return qobject_cast<QObject*>(static_cast<QObject*>(m_singlePool[fullname])) != nullptr;
}

// 获取所有实例类名列表
QStringList SqzHub::GetExistClassList()
{
    QReadLocker locker(&GetFactoryLock());
    return m_singlePool.keys();
}

// 获取实例总数
int SqzHub::GetInstanceCount()
{
    QReadLocker locker(&GetFactoryLock());
    return m_singlePool.size();
}

// 打印已注册类名
void SqzHub::PrintRegClass()
{
    QReadLocker locker(&GetFactoryLock());
    logdebug << "===== [SqzHub] 已注册类列表 =====";
    for (auto& key : m_noArgCreator.keys())
        logdebug << key;
    for (auto& key : m_qmlCreators.keys())
        logdebug << key;
}

// 销毁所有单例
void SqzHub::CloseAll()
{
    // 统一调用 destroyAllObjects()（修复 Bug #17：原与 ~SqzHub 逻辑重复，现共用同一销毁路径）
    // 退出阶段事件循环可能即将停止，立即同步销毁（deleteLater 不可靠）
    destroyAllObjects();
}

// 清空注册表
void SqzHub::ClearReg()
{
    QWriteLocker locker(&GetFactoryLock());
    m_noArgCreator.clear();
    m_argCreator.clear();
    m_argMeta.clear();
    m_qmlCreators.clear();
    m_quickQmlPath.clear();   // 同步清理 QML 路径缓存，避免清表后残留陈旧路径
}

// 带参创建临时QObject
QObject* SqzHub::CreateObjectByArg(const QString& ClassName, const QVariantList& Args)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    // 修复 Bug #20：原实现持读锁调用 m_argCreator[fullname](Args)，
    // 若回调再次进入 SqzHub 会与默认非递归 QReadWriteLock 死锁。
    // 改为锁内仅拷贝 functor + 元数据，锁外执行创建。
    CreatorWithArg creator;
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_argCreator.contains(fullname)) return nullptr;
        creator = m_argCreator[fullname];
        meta = m_argMeta.value(fullname);
    }

    void* raw = creator(Args);
    QObject* obj = qobject_cast<QObject*>(static_cast<QObject*>(raw));
    if (!obj) {
        // 非 QObject 临时对象：用注册的 deleter 安全释放（避免 delete char* 的 UB）
        if (meta.immediateDeleter) meta.immediateDeleter(raw);
        else delete static_cast<char*>(raw);
    }
    return obj;
}

QQmlApplicationEngine *SqzHub::qmlEngine()
{
    if (!m_qmlEngine) {
        if (!qApp) {
            logwarn << "No QApplication instance! Cannot create QML engine.";
            return nullptr;
        }
        m_qmlEngine.reset(new QQmlApplicationEngine());
        // 仅在首次创建时连接一次，避免每次调用累积重复连接
        connect(m_qmlEngine.get(), &QQmlApplicationEngine::objectCreated,
                this, [](QObject* obj, const QUrl& url) {
            // 全局 QML 加载错误处理（可扩展）
        });
    }
    return m_qmlEngine.get();
}


SqzHub::PrefixScope::PrefixScope(const QString &prefix) : m_oldPrefix(t_prefix)
{
    t_prefix = prefix;
}

SqzHub::PrefixScope::~PrefixScope()
{
    t_prefix = m_oldPrefix;
}
}
