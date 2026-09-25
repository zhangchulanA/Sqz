// SqzHub.h
#ifndef SqzHub_H
#define SqzHub_H

#include <functional>
#include <QVariantList>
#include <QWidget>
#include <QWriteLocker>     // 同上

#include <QQmlApplicationEngine>
#include <memory>
#include <QQmlEngine>
#include <QtQuick/QQuickView>
#include <QThread>
#include "SqzGlobal.h"

namespace Sqz {

//全局读写锁（线程安全，避免静态初始化顺序问题
inline QReadWriteLock& GetFactoryLock()
{
    static QReadWriteLock factoryLock;
    return factoryLock;
}

//类元数据：存储创建/销毁函数及类型信息
struct SQZ_FRAMEWORK_API ClassMeta
{
    std::function<void*()> creator;                   // 创建函数
    std::function<void(void* ptr)> deleter;            // 延迟销毁（deleteLater），用于 CloseObjLater
    std::function<void(void* ptr)> immediateDeleter;  // 立即销毁（delete），用于 CloseObj/CloseAll/~SqzHub
    bool isQObject = true;                            // 是否为 QObject 派生类
};

//带参构造函数类型：接收 QVariantList 参数
using CreatorWithArg = std::function<void*(const QVariantList& args)>;

/**
 * @brief 通用单例工厂，通过类名字符串创建/管理 QWidget、QObject、QML 对象。
 *        支持线程前缀隔离，读写锁保证线程安全。
 */
class SQZ_FRAMEWORK_API SqzHub : public QObject
{
    Q_OBJECT
public:
    friend class SqzQuick;
    explicit SqzHub(QObject *parent = nullptr);
    ~SqzHub();
public:
    // RAII 临时切换线程前缀
    class PrefixScope {
    public:
        explicit PrefixScope(const QString& prefix);
        ~PrefixScope();
    private:
        QString m_oldPrefix;
    };

    static void SetThreadPrefix(const QString& prefix);
    static QString ThreadPrefix();

private:
    static QString          s_prefix;       // 全局前缀
    static QReadWriteLock   s_prefixLock;   // 保护 s_prefix
    static QString maybeAddThreadPrefix(const QString& className);

public:
    // 单例入口
//    static SqzHub& Instance() {
//        static SqzHub factoryInstance;
//        return factoryInstance;
//    }

// =============================== 注册接口 =====================================
    //注册无参构造类
    void RegisterNoArg(const QString& ClassName,
                       std::function<void*()> Creator,
                       std::function<void(void*)> Deleter = nullptr,
                       bool isQObject = false);

    //注册带参构造类（接收 QVariantList）
    //isQObject 标记是否为 QObject 派生（决定销毁方式 deleteLater/delete）
    //Deleter 自定义销毁函数，为空时按 isQObject 自动选择默认销毁器
    void RegisterWithArg(const QString& ClassName, CreatorWithArg Func,
                         bool isQObject = true,
                         std::function<void(void*)> Deleter = nullptr);

    //注册 QML/Quick 类（无参构造）
    void RegisterQuickClass(const QString& ClassName,
                            std::function<void*()> Creator,
                            std::function<void(void*)> Deleter = nullptr);

// =============================== 核心创建 ===================================

    //创建/获取 QWidget 单例（主线程）
    QWidget* CreateWidget(const QString& ClassName,const QVariantMap& props = {});

    //创建/获取 QObject 单例
    QObject* CreateObject(const QString& ClassName,const QVariantMap& props = {});

    //创建/获取 QML Quick 窗口单例（主线程）
    QObject* CreateQuick(const QString& ClassName,const QString& qmlpath = "",const QVariantMap& props = {});

    //创建异步对象
    QObject* CreateObjectAsync(const QString& ClassName, const QVariantMap& props);

    // ================================= 生命周期管理 =================================

    //判断单例是否已存在
    bool IsExist(const QString& ClassName) const;

    //立即销毁单例
    void CloseObj(const QString& ClassName);

    //延迟销毁单例（下一事件循环）
    void CloseObjLater(const QString& ClassName);

    //删除临时对象（不入池）
    void DeleteTemp(const QString& ClassName, void* ptr);

    //重置单例（销毁后重建）
    void ResetObj(const QString& ClassName);

    //创建临时对象（不入池，需手动释放）
    void* CreateTemp(const QString& ClassName);

    //安全释放裸指针（静态）
    static void SafeDelete(void* Ptr, bool isQObject = false, bool immediate = false);

// ============================== 工具 =================================

    //检查类是否已注册
    bool IsClassReg(const QString& ClassName);

    //判断实例是否为 QWidget
    bool IsWidgetObj(const QString& ClassName);

    //判断实例是否为 QObject
    bool IsQObject(const QString& ClassName);

    //获取所有已创建实例的类名列表
    QStringList GetExistClassList();

    //获取当前实例总数
    int GetInstanceCount();

    //打印所有已注册类名（调试）
    void PrintRegClass();

// ========================= 批量操作 =========================

    //销毁所有单例
    void CloseAll();

    //创建带参临时 QObject
    QObject* CreateObjectByArg(const QString& ClassName, const QVariantList& Args);

    //获取 QML 引擎指针
    QQmlEngine* qmlEngine();


private:

//    Q_DISABLE_COPY(SqzHub)

    //内部创建核心函数
    void* createInternal(const QString& ClassName,
                         std::function<bool(void*)> validator,
                         bool isWidget,const QVariantMap& props = {});

    //辅助函数 给对象应用属性
    static void ApplyPropsToObject(QObject* obj,const QVariantMap& props);

    //获取 Quick 对象指针（内部）
    QObject* GetQuickObject(const QString& ClassName) const;

    //获取类的元数据
    ClassMeta getMetaForClass(const QString& fullname);

    void destroyAllObjects();

    // 内部工具：在指定线程执行一段代码（异步创建/销毁统一复用）
       void stopServiceThread(const QString& fullname, QThread* thread, QObject* obj);

       bool IsAsyncService(const QString& ClassName) const;

private:
//    std::unique_ptr<QQmlApplicationEngine> m_qmlEngine;  //   QML 引擎
    std::unique_ptr<QQmlEngine> m_qmlEngine;
    QHash<QString, ClassMeta>      m_noArgCreator;   //   无参构造器表
    QHash<QString, CreatorWithArg> m_argCreator;     //   带参构造器表
    QHash<QString, ClassMeta>      m_argMeta;         //   带参类元数据表（销毁时查 deleter/isQObject）
    QHash<QString, void*>          m_singlePool;     //   单例对象池
    QHash<QString, ClassMeta>      m_qmlCreators;     //   Quick 类构造器表
    QHash<QString, QString>        m_quickQmlPath;   //   Quick 视图 QML 源路径缓存（供 ResetObj 重建使用）
    // 每个类的异步线程
    QHash<QString, QThread*> m_serviceThreads;
};

#if A
// ---------- 自动注册宏（支持模块前缀） ----------
#ifdef _MSC_VER
#define FORCE_LINK_THIS(x) __pragma(comment(linker, "/include:" #x))
#else
#define FORCE_LINK_THIS(x) __attribute__((used))
#endif

//注册无参 Widget 类
#define SQZOBJECT_NOARG(Cls) \
    static void _auto_reg_##Cls() \
{ \
    constexpr bool isQObj = std::is_base_of<QObject, Cls>::value; \
    SqzHub::Instance().RegisterNoArg(MAKE_FULL_NAME(Cls), \
    []()->void*{ return new Cls(); }, \
    [](void* ptr){ delete static_cast<Cls*>(ptr); }, \
    isQObj \
    ); \
    } \
    FORCE_LINK_THIS(_reg_flag_##Cls) static bool _reg_flag_##Cls = (_auto_reg_##Cls(), true);

 //注册无参 Quick 类
#define SQZQUICK_NOARG(Class) \
    static void _auto_reg_qml_##Class() { \
    SqzHub::Instance().RegisterQuickClass(MAKE_FULL_NAME(Class), \
    []()->void*{ return new Class(); }, \
    [](void* ptr){ delete static_cast<Class*>(ptr); } \
    ); \
    } \
    FORCE_LINK_THIS(_reg_qml_flag_##Class) \
    static bool _reg_qml_flag_##Class = (_auto_reg_qml_##Class(), true);

 //注册带参类（接收 QVariantList）
#define SQZOBJECT_ARG(Cls) \
    static void _auto_reg_arg_##Cls() \
{ \
    SqzHub::Instance().RegisterWithArg(MAKE_FULL_NAME(Cls), [](const QVariantList& Args)->void*{ \
    return new Cls(Args); \
    }); \
    } \
    FORCE_LINK_THIS(_reg_flag_arg_##Cls) static bool _reg_flag_arg_##Cls = (_auto_reg_arg_##Cls(), true);
#endif

}
#endif // SqzHub_H
