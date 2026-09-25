#include "SqzHub.h"
#include <QWriteLocker>   // 显式包含
#include <QThread>
#include <QCoreApplication>
#include <QTimer>
#include "SqzService.h"
#include "SqzQuick.h"
#include "SqzWidget.h"
#include "SqzMainWindow.h"
namespace Sqz {
QString         SqzHub::s_prefix;
QReadWriteLock  SqzHub::s_prefixLock;

SqzHub::SqzHub(QObject *parent) : QObject(parent)
{

}


SqzHub::~SqzHub()
{
    // 析构阶段事件循环已停止，统一调用 destroyAllObjects() 立即同步销毁所有单例
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

void SqzHub::destroyAllObjects()
{
    QHash<QString, void*>    pool;
    QHash<QString, QThread*> threads;
    {
        QWriteLocker locker(&GetFactoryLock());
        pool.swap(m_singlePool);
        threads.swap(m_serviceThreads);
    }

    // 1) 先给每个异步对象在其线程里同步执行 onClose，再停线程
    for (auto it = threads.begin(); it != threads.end(); ++it) {
        const QString fullname = it.key();
        QThread* t = it.value();
        void* p = pool.value(fullname, nullptr);
        stopServiceThread(fullname, t, static_cast<QObject*>(p));
    }

    // 2) 逐个 onClose + 删除（同步对象走这里；异步对象的 onClose 已在上面执行，
    //    为避免重复调用，这里仅对同步对象调 onClose）
    for (auto it = pool.begin(); it != pool.end(); ++it) {
        const QString fullname = it.key();
        void* p = it.value();
        ClassMeta meta = getMetaForClass(fullname);
        QObject* obj = static_cast<QObject*>(p);

        const bool isAsync = threads.contains(fullname);
        if (!isAsync && obj) {
            if (auto* v = qobject_cast<SqzWidget*>(obj))         v->onClose();
            else if (auto* s = qobject_cast<SqzService*>(obj))   s->onClose();
            else if (auto* q = qobject_cast<SqzQuick*>(obj))     q->onClose();
            else if (auto* m = qobject_cast<SqzMainWindow*>(obj)) m->onClose();
        }

        if (meta.immediateDeleter) meta.immediateDeleter(p);
        else SafeDelete(p, meta.isQObject, true);
    }

    // 线程对象已在 stopServiceThread 内 delete}
}

void SqzHub::stopServiceThread(const QString &fullname, QThread *thread, QObject *obj)
{
    if (!thread) return;

    // 先让服务线程同步执行 onClose（若线程仍在跑）
    if (thread->isRunning() && obj) {
        if (auto* svc = qobject_cast<SqzService*>(obj)) {
            QMetaObject::invokeMethod(svc, "onClose",
                                      Qt::BlockingQueuedConnection);
        } else if (auto* view = qobject_cast<SqzWidget*>(obj)) {
            QMetaObject::invokeMethod(view, "onClose",
                                      Qt::BlockingQueuedConnection);
        } else if (auto* qmlView = qobject_cast<SqzQuick*>(obj)) {
            QMetaObject::invokeMethod(qmlView, "onClose",
                                      Qt::BlockingQueuedConnection);
        } else if (auto* mainWin = qobject_cast<SqzMainWindow*>(obj)) {
            QMetaObject::invokeMethod(mainWin, "onClose",
                                      Qt::BlockingQueuedConnection);
        }
    }

    // 停线程
    if (thread->isRunning()) {
        thread->quit();
        if (!thread->wait(5000)) {
            logwarn << "线程退出超时，强制终止：" << fullname
                    << " | Thread:" << thread->objectName();
            thread->terminate();
            thread->wait(1000);
        }
    }
    delete thread;}

bool SqzHub::IsAsyncService(const QString &ClassName) const
{
    const QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    return m_serviceThreads.contains(fullname);
}

void *SqzHub::createInternal(const QString &ClassName, std::function<bool (void *)> validator, bool isWidget, const QVariantMap &props)
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
                //                ApplyPropsToObject(w,props);
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
                //                ApplyPropsToObject(w,props);
            }
            return existing;
        }
        // 存入池
        m_singlePool[fullname] = raw;
    }

    ApplyPropsToObject(static_cast<QWidget*>(raw),props);
    if (meta.isQObject) {
        // 7. 对于 QObject，连接 destroyed 信号自动从池移除
        QObject* obj = static_cast<QObject*>(raw);
        connect(obj, &QObject::destroyed, this, [this, fullname, raw]() {
            QWriteLocker locker(&GetFactoryLock());
            // 仅当池中仍是同一对象时移除（防止 ResetObj 后旧对象销毁误删新对象）
            if (m_singlePool.value(fullname) == raw)
                m_singlePool.remove(fullname);
        });

        // 8.新增：调用 onInit
        if (auto* view = qobject_cast<SqzWidget*>(obj))
            view->onInit();
        else if (auto* svc = qobject_cast<SqzService*>(obj))
            svc->onInit();
        //        else if (auto* mainWin = qobject_cast<SqzMainWindow*>(obj))
        //            mainWin->onInit();

    }
    // 9. 若是窗口，显示并置前
    if (isWidget) {
        QWidget* w = static_cast<QWidget*>(raw);
        w->show(); w->raise(); w->activateWindow();
    }

    return raw;
}

void SqzHub::ApplyPropsToObject(QObject *obj, const QVariantMap &props)
{
    if(!obj || props.isEmpty()) return;
    //    const QMetaObject* meta = obj->metaObject();
    for(auto it = props.begin();it != props.end();it++){
        const QString& propName = it.key();
        const QVariant& value = it.value();

        obj->setProperty(propName.toUtf8().constData(), value);

        //        int propIdx = meta->indexOfProperty(propName.toUtf8().constData());
        //        if (propIdx < 0)
        //        {
        //            logwarn << "[SqzApp] 属性不存在,作为动态属性设置" << propName
        //                    << " | 对象类:" << meta->className();
        //            obj->setProperty(propName.toUtf8().constData(), value);
        //            continue;
        //        }
    }
}

void SqzHub::SetThreadPrefix(const QString &prefix)
{
    QWriteLocker lk(&s_prefixLock);
    s_prefix = prefix;
}

QString SqzHub::ThreadPrefix()
{
    QReadLocker lk(&s_prefixLock);
    return s_prefix;    // 读锁内拷贝，返回后锁自动释放
}

QString SqzHub::maybeAddThreadPrefix(const QString &className)
{
    // className 已包含 "::" 视为已带前缀，直接返回（避免重复加前缀）
    if (className.contains("::")) return className;

    QReadLocker lk(&s_prefixLock);
    if (s_prefix.isEmpty()) return className;
    return s_prefix + "::" + className;
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
QWidget* SqzHub::CreateWidget(const QString& ClassName, const QVariantMap &props)
{
    return static_cast<QWidget*>(createInternal(ClassName,
                                                [](void* p) { return qobject_cast<QWidget*>(static_cast<QObject*>(p)) != nullptr;},
    true,props));
}

// 创建QObject单例
QObject* SqzHub::CreateObject(const QString& ClassName, const QVariantMap &props)
{
    return static_cast<QObject*>(createInternal(ClassName,
                                                [](void* p) { return qobject_cast<QObject*>(static_cast<QObject*>(p)) != nullptr; },
    false,props));
}


QObject *SqzHub::CreateQuick(const QString &ClassName, const QString& qmlpath, const QVariantMap &props)
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
    ApplyPropsToObject(view,props);
    view->setQmlSourcePath(actualQmlPath);
    view->init();

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

QObject *SqzHub::GetQuickObject(const QString &ClassName)const
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    if (m_singlePool.contains(fullname)) {
        return static_cast<QObject*>(m_singlePool[fullname]);
    }
    return nullptr;
}


QObject *SqzHub::CreateObjectAsync(const QString &ClassName, const QVariantMap &props)
{
    const QString fullname = maybeAddThreadPrefix(ClassName);

    // 主线程校验（异步创建虽然内部移线程，但对象仍在主线程构造）
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        logwarn << "[SqzHub] 异步服务必须在主线程创建：" << fullname;
        return nullptr;
    }

    // 1. 已存在则直接返回
    {
        QReadLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            return static_cast<QObject*>(m_singlePool[fullname]);
        }
    }

    // 2. 取元数据
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_noArgCreator.contains(fullname)) {
            logwarn << "[SqzHub] 未注册类（异步）：" << fullname;
            return nullptr;
        }
        meta = m_noArgCreator[fullname];
    }

    // 3. 主线程构造（构造期间对象仍属于主线程，可以在 setter 里访问主线程资源）
    void* raw = meta.creator();
    if (!raw) {
        logwarn << "[SqzHub] 异步对象创建失败：" << fullname;
        return nullptr;
    }

    QObject* obj = qobject_cast<QObject*>(static_cast<QObject*>(raw));
    if (!obj) {
        if (meta.immediateDeleter) meta.immediateDeleter(raw);
        else SafeDelete(raw, meta.isQObject, true);
        logwarn << "[SqzHub] 异步服务必须是 QObject：" << fullname;
        return nullptr;
    }

    // 4. 主线程应用属性（避免 setter 跨线程访问其它主线程资源）
    ApplyPropsToObject(obj, props);

    // 5. 创建专属线程并移入
    QThread* thread = new QThread(this);
    thread->setObjectName(QStringLiteral("SqzSvc-") + fullname);

    obj->moveToThread(thread);
    // 注意：moveToThread 后 obj 已无 parent（外部若设过 parent 会失败，需保证无 parent）

    // 6. 启动线程
    thread->start();

    // 7. 入池 + 记录线程
    {
        QWriteLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            // 竞态兜底：极端情况下其它线程抢先创建
            thread->quit();
            thread->wait(3000);
            delete thread;
            if (meta.immediateDeleter) meta.immediateDeleter(raw);
            else SafeDelete(raw, meta.isQObject, true);
            return static_cast<QObject*>(m_singlePool[fullname]);
        }
        m_singlePool[fullname]     = obj;
        m_serviceThreads[fullname] = thread;
    }

    // 8. 通知基类自己已异步 + 派发 onInit 到服务线程
    if (auto* svc = qobject_cast<SqzService*>(obj)) {
        svc->setAsync(true);
        QMetaObject::invokeMethod(svc, "onInit", Qt::QueuedConnection);
    }

    // 9. 对象被外部 delete 时，联动清理线程（destroyAllObjects 已显式 quit，会走到这里再 take 为空）
    connect(obj, &QObject::destroyed, this, [this, fullname, obj]() {
        QThread* t = nullptr;
        {
            QWriteLocker locker(&GetFactoryLock());
            if (m_singlePool.value(fullname) == obj)
                m_singlePool.remove(fullname);
            t = m_serviceThreads.take(fullname);
        }
        if (t) {
            if (t->isRunning()) {
                t->quit();
                t->wait(3000);
            }
            t->deleteLater();
        }
    });

    loginfo << "[SqzHub] 异步服务已启动：" << fullname
            << " | Thread:" << thread->objectName();
    return obj;
}

// 判断对象是否存在
bool SqzHub::IsExist(const QString& ClassName) const
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QReadLocker locker(&GetFactoryLock());
    return m_singlePool.contains(fullname);
}


// 立即销毁对象
void SqzHub::CloseObj(const QString& ClassName)
{
    const QString fullname = maybeAddThreadPrefix(ClassName);

    ClassMeta meta;
    void* ptr = nullptr;
    QThread* thread = nullptr;
    {
        QWriteLocker locker(&GetFactoryLock());
        if (!m_singlePool.contains(fullname)) return;
        ptr    = m_singlePool.take(fullname);
        meta   = getMetaForClass(fullname);
        thread = m_serviceThreads.take(fullname);  // 同步服务为 nullptr
    }

    QObject* obj = static_cast<QObject*>(ptr);

    if (thread) {
        // 异步：先在线程里同步执行 onClose，再停线程（stopServiceThread 内部已做）
        stopServiceThread(fullname, thread, obj);
    } else if (obj) {
        // 同步：直接调用 onClose
        if (auto* v = qobject_cast<SqzWidget*>(obj))         v->onClose();
        else if (auto* s = qobject_cast<SqzService*>(obj))   s->onClose();
        else if (auto* q = qobject_cast<SqzQuick*>(obj))     q->onClose();
        else if (auto* m = qobject_cast<SqzMainWindow*>(obj)) m->onClose();
    }

    // 此时线程已退出（或本就是同步对象），直接 delete 安全
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
}

// 创建临时对象（不入池）
void* SqzHub::CreateTemp(const QString& ClassName)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    //锁内仅拷贝 functor，锁外执行 creator()。
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
        // 正常销毁应通过 ClassMeta.deleter 完成；注册时已强制非 QObject 类提供 deleter
        delete static_cast<char*>(Ptr);
    }
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
    // 退出阶段事件循环可能即将停止，立即同步销毁（deleteLater 不可靠）
    destroyAllObjects();
}

// 带参创建临时QObject
QObject* SqzHub::CreateObjectByArg(const QString& ClassName, const QVariantList& Args)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    //锁内仅拷贝 functor + 元数据，锁外执行创建。
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

QQmlEngine *SqzHub::qmlEngine()
{
    if (!m_qmlEngine) {
        if (!qApp) {
            logwarn << "No QApplication instance! Cannot create QML engine.";
            return nullptr;
        }
        m_qmlEngine.reset(new QQmlEngine());

        // 引擎级错误处理：QML 加载/运行期错误统一落日志
        // 注意：QQmlEngine 没有 objectCreated 信号，需要改用 QQmlComponent 的 status 检查
        connect(m_qmlEngine.get(), &QQmlEngine::warnings,
                this, [](const QList<QQmlError>& warnings) {
            for (const auto& w : warnings)
                logwarn << "[QML]" << w.toString();
        });
    }
    return m_qmlEngine.get();
}


SqzHub::PrefixScope::PrefixScope(const QString &prefix)
{
    QWriteLocker lk(&s_prefixLock);
    m_oldPrefix = s_prefix;
    s_prefix    = prefix;
}

SqzHub::PrefixScope::~PrefixScope()
{
    QWriteLocker lk(&s_prefixLock);
    s_prefix = m_oldPrefix;
}
}
