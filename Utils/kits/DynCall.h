#ifndef DYNCALL_H
#define DYNCALL_H

#include <QObject>
#include <QMetaObject>
#include <QTimer>
#include <QPointer>
#include <functional>
#include <type_traits>
#include <QThread>

class DynCall {
public:
    // ========== 同步调用（当前线程） ==========

    // 有返回值
    template<typename T, typename R, typename... Args>
    static typename std::enable_if<!std::is_void<R>::value, R>::type
    call(QObject* obj, R (T::*method)(Args...), Args... args) {
        static_assert(std::is_base_of<QObject, T>::value,
                      "T must inherit QObject");
        return (static_cast<T*>(obj)->*method)(args...);
    }

    // 无返回值
    template<typename T, typename R, typename... Args>
    static typename std::enable_if<std::is_void<R>::value>::type
    call(QObject* obj, R (T::*method)(Args...), Args... args) {
        static_assert(std::is_base_of<QObject, T>::value,
                      "T must inherit QObject");
        (static_cast<T*>(obj)->*method)(args...);
    }

    // ========== 异步调用（跨线程） ==========

    // 基础异步
    template<typename T, typename... Args>
    static void async(QObject* obj, void (T::*method)(Args...),
                      Args... args) {
        QMetaObject::invokeMethod(obj, [=]() {
            (static_cast<T*>(obj)->*method)(args...);
        }, Qt::QueuedConnection);
    }

    // 安全异步（对象删除时自动取消）
    template<typename T, typename... Args>
    static void asyncSafe(QObject* obj, void (T::*method)(Args...),
                          Args... args) {
        QPointer<QObject> guard(obj);
        QMetaObject::invokeMethod(obj, [=]() {
            if (!guard.isNull()) {
                (static_cast<T*>(guard.data())->*method)(args...);
            }
        }, Qt::QueuedConnection);
    }

    // 指定连接类型
    template<typename T, typename... Args>
    static void async(QObject* obj, void (T::*method)(Args...),
                      Qt::ConnectionType type, Args... args) {
        QMetaObject::invokeMethod(obj, [=]() {
            (static_cast<T*>(obj)->*method)(args...);
        }, type);
    }

    // ========== 延迟调用 ==========

    template<typename T, typename... Args>
    static void delay(QObject* obj, void (T::*method)(Args...),
                      int ms, Args... args) {
        QPointer<QObject> guard(obj);
        QTimer::singleShot(ms, [=]() {
            if (!guard.isNull()) {
                (static_cast<T*>(guard.data())->*method)(args...);
            }
        });
    }

    // ========== 在对象线程中执行 ==========

    template<typename T, typename... Args>
    static void inThread(QObject* obj, void (T::*method)(Args...),
                         Args... args) {
        if (obj->thread() == QThread::currentThread()) {
            // 已在目标线程，直接执行
            (static_cast<T*>(obj)->*method)(args...);
        } else {
            // 切换到目标线程
            async(obj, method, args...);
        }
    }

    // ========== 带返回值的异步（通过回调） ==========

    template<typename T, typename R, typename... Args>
    static void asyncWithCallback(QObject* obj, R (T::*method)(Args...),
                                  std::function<void(R)> callback,
                                  Args... args) {
        QPointer<QObject> guard(obj);
        QMetaObject::invokeMethod(obj, [=]() {
            R result = R();
            if (!guard.isNull()) {
                result = (static_cast<T*>(guard.data())->*method)(args...);
            }
            if (callback) callback(result);
        }, Qt::QueuedConnection);
    }
};

#endif // DYNCALL_H
