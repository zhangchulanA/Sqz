#include "SqzHub.h"
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
//        UnReg(obj);
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
        SafeDelete(deleteList[i], metaList[i].isQObject, true);
    }
//    m_objects.clear(); // SqzProp 的跟踪容器清理
}

ClassMeta SqzHub::getMetaForClass(const QString &fullname)
{
    if (m_noArgCreator.contains(fullname))
        return m_noArgCreator[fullname];
    if (m_qmlCreators.contains(fullname))
        return m_qmlCreators[fullname];
    return ClassMeta{};
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
//                Reg(static_cast<QObject*>(existing)); // 确保已注册（内部有去重）
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
        if (meta.deleter) meta.deleter(raw);
        else SafeDelete(raw, meta.isQObject, false);
        logwarn << "[SqzHub] 类型转换失败：" << fullname;
        return nullptr;
    }

    // 6. 第二次检查：可能其他线程已经创建，防止重复插入
    {
        QWriteLocker locker(&GetFactoryLock());
        if (m_singlePool.contains(fullname)) {
            // 丢弃刚创建的对象，返回已有对象
            if (meta.deleter) meta.deleter(raw);
            else SafeDelete(raw, meta.isQObject, false);
            void* existing = m_singlePool[fullname];
            if (isWidget) {
                QWidget* w = static_cast<QWidget*>(existing);
                w->show(); w->raise(); w->activateWindow();
//                Reg(static_cast<QObject*>(existing));
            }
            return existing;
        }
        // 存入池
        m_singlePool[fullname] = raw;
    }


    if (meta.isQObject) {
        // 7. 对于 QObject，连接 destroyed 信号自动从池移除
        QObject* obj = static_cast<QObject*>(raw);
        connect(obj, &QObject::destroyed, this, [=]() {
            QWriteLocker locker(&GetFactoryLock());
            m_singlePool.remove(fullname);
        });

        // 8. 注册到 SqzProp 的对象跟踪
//        Reg(static_cast<QObject*>(raw));
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
    if (!m_noArgCreator.contains(fullname)) {
        ClassMeta meta;
        meta.creator = std::move(Creator);
        meta.isQObject = isQObject;
        if (Deleter) {
            meta.deleter = std::move(Deleter);
        } else {
            if (isQObject) {
                meta.deleter = [](void* p) { static_cast<QObject*>(p)->deleteLater(); };
            } else {
                meta.deleter = [](void* p) { delete static_cast<char*>(p); };
            }
        }
        m_noArgCreator[fullname] = std::move(meta);
    } else {
        logwarn << "[SqzHub] 重复注册类：" << fullname;
    }
}

// 注册带参类
void SqzHub::RegisterWithArg(const QString& ClassName, CreatorWithArg Func)
{
    QString fullname = maybeAddThreadPrefix(ClassName);
    QWriteLocker locker(&GetFactoryLock());
    if (!m_argCreator.contains(fullname))
        m_argCreator[fullname] = std::move(Func);
    else logwarn << "[SqzHub] 重复注册带参类：" << fullname;
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
            meta.deleter = std::move(Deleter);
        } else {
            meta.deleter = [](void* p) { delete static_cast<QObject*>(p); };
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
//            Reg(obj);
            return obj;
        }
    }

    // 获取 QML 类的元数据
    ClassMeta meta;
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_qmlCreators.contains(fullname)) {
            logwarn << "[SqzHub] 未注册 QML 类：" << fullname;
            return nullptr;
        }
        meta = m_qmlCreators[fullname];
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
        meta.deleter(raw);
        logwarn << "[SqzHub] 类型转换失败（需要 SqzQuick）：" << fullname;
        return nullptr;
    }
    view->initializeView(qmlpath);

    // 存入池
    QWriteLocker locker(&GetFactoryLock());
    if (m_singlePool.contains(fullname)) {
        // 其他线程已创建，丢弃本对象
        meta.deleter(raw);
        if (view->window()) {
            view->window()->show();
            view->window()->raise();
            view->window()->requestActivate();
        }
//        Reg(qmlObj);
        return qmlObj;
    }
    m_singlePool[fullname] = raw;

    // 连接销毁信号
    connect(qmlObj, &QObject::destroyed, this, [=]() {
        QWriteLocker locker(&GetFactoryLock());
        m_singlePool.remove(fullname);
    });

//    Reg(qmlObj);

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
//            Reg(w);
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
//            Reg(existing);
            return existing;
        }
        m_singlePool[fullname] = widget;
    }

    // 连接销毁信号
    connect(widget, &QWidget::destroyed, this, [=]() {
        QWriteLocker locker(&GetFactoryLock());
        m_singlePool.remove(fullname);
    });

//    Reg(widget);
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
    {
        QReadLocker locker(&GetFactoryLock());
        if (!m_argCreator.contains(fullname)) {
            logwarn << "[SqzHub] 未注册带参类：" << fullname;
            return nullptr;
        }
        creator = m_argCreator[fullname];
    }

    void* raw = creator(args);
    if (!raw) return nullptr;

    QObject* obj = qobject_cast<QObject*>(static_cast<QObject*>(raw));
    if (!obj) {
        // 如果不是 QObject，释放并返回
        delete static_cast<char*>(raw);
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

    connect(obj, &QObject::destroyed, this, [=]() {
        QWriteLocker locker(&GetFactoryLock());
        m_singlePool.remove(fullname);
    });

//    Reg(obj);
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
//    UnReg(obj);
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
    if (meta.deleter) meta.deleter(ptr);
    else SafeDelete(ptr, meta.isQObject,true);
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
        if (meta.deleter) meta.deleter(ptr);
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
    QReadLocker locker(&GetFactoryLock());
    if (!m_noArgCreator.contains(fullname)) return nullptr;
    return m_noArgCreator[fullname].creator();
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
//        UnReg(obj);   // 先注销，避免后续访问
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
        // 强制立即删除
        SafeDelete(deleteList[i], metaList[i].isQObject, true);
    }
//    m_objects.clear(); // 清理 SqzProp 的跟踪容器（已无有效对象）
}

// 清空注册表
void SqzHub::ClearReg()
{
    QWriteLocker locker(&GetFactoryLock());
    m_noArgCreator.clear();
    m_argCreator.clear();
}

// 带参创建临时QObject
QObject* SqzHub::CreateObjectByArg(const QString& ClassName, const QVariantList& Args)
{
    QString fullname = maybeAddThreadPrefix(ClassName);

    QReadLocker locker(&GetFactoryLock());
    if (!m_argCreator.contains(fullname)) return nullptr;
    void* raw = m_argCreator[fullname](Args);
    return qobject_cast<QObject*>(static_cast<QObject*>(raw));
}

QQmlApplicationEngine *SqzHub::qmlEngine()
{
    if (!m_qmlEngine) {
        if (!qApp) {
            logwarn << "No QApplication instance! Cannot create QML engine.";
            return nullptr;
        }
        m_qmlEngine.reset(new QQmlApplicationEngine());
    }
    connect(m_qmlEngine.get(), &QQmlApplicationEngine::objectCreated,
            this, [](QObject* obj, const QUrl& url) {
        // 可在此做全局错误处理
    });
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
