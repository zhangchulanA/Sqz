#ifndef OBJECTPOOL_H
#define OBJECTPOOL_H

#include <QVector>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>
#include <functional>
#include "SqzGlobal.h"
/* ============================================================
   ObjectPool<T>：对象复用池（Header-Only，线程安全）

   功能：
   - 预分配对象，重复使用，避免频繁 new/delete
   - 归还时自动重置对象状态（调用 Reset() 或自定义函数）
   - 池满时 Acquire() 返回 nullptr，防止内存爆炸
   - 纯头文件，无需编译，直接 #include 即可使用

   使用示例：
   ------------------------------------------------------------
   ObjectPool<MyClass> pool(100);
   MyClass* obj = pool.Acquire();
   if (obj) {
       obj->doSomething();
       pool.Release(obj);
   }
   ------------------------------------------------------------
   ============================================================ */
namespace Sqz::Utils {
template<typename T>
class  ObjectPool {
public:
    /*!
     * \brief 构造函数
     * \param initialSize 预分配对象数量，默认 0（不预分配）
     */
    explicit ObjectPool(int initialSize = 0) {
        PreAllocate(initialSize);
    }

    // 禁止拷贝和赋值（池子不能被复制）
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /*!
     * \brief 析构函数：释放所有对象
     */
    ~ObjectPool() {
        QMutexLocker locker(&m_mutex);
        qDeleteAll(m_allObjects);
        m_allObjects.clear();
        m_freeQueue.clear();
    }

    /* ============================================================
       核心操作
       ============================================================ */

    /*!
     * \brief 从池中借一个对象
     * \return 对象指针，如果池子空了返回 nullptr
     * \note 获取到的对象可能残留上次使用后的数据，使用前请自行初始化
     *       或确保 Reset()/自定义重置函数已正确执行
     */
    T* Acquire() {
        QMutexLocker locker(&m_mutex);
        if (m_freeQueue.isEmpty()) {
            return nullptr;
        }
        T* obj = m_freeQueue.dequeue();
        m_usedCount++;
        return obj;
    }

    /*!
     * \brief 归还对象到池中
     * \param obj 要归还的对象指针
     * \note 归还时会自动调用重置函数（优先级：自定义 > Reset() > 无操作）
     *       归还后请勿再使用该指针
     */
    void Release(T* obj) {
        if (!obj) return;

        QMutexLocker locker(&m_mutex);
        // 执行重置
        if (m_resetFunction) {
            m_resetFunction(obj);
        } else {
            CallReset(obj);
        }
        m_freeQueue.enqueue(obj);
        m_usedCount--;
    }

    /* ============================================================
       管理操作
       ============================================================ */

    /*!
     * \brief 预分配额外对象（扩容）
     * \param count 要新增的对象数量
     * \note 可在运行时动态扩容，不会影响已有对象
     */
    void PreAllocate(int count) {
        if (count <= 0) return;

        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < count; ++i) {
            T* obj = new T();
            m_allObjects.append(obj);
            m_freeQueue.enqueue(obj);
        }
    }

    /*!
     * \brief 设置自定义重置函数
     * \param fn 函数签名为 void(T*)
     * \note 设置后，每次 Release 都会调用此函数
     *       如果不设置，默认尝试调用对象的 Reset() 方法
     */
    void SetResetFunction(std::function<void(T*)> fn) {
        QMutexLocker locker(&m_mutex);
        m_resetFunction = fn;
    }

    /*!
     * \brief 清空池子，释放所有对象
     * \note 调用后所有已借出的对象将变成野指针，请确保全部归还后再调用
     */
    void Clear() {
        QMutexLocker locker(&m_mutex);
        qDeleteAll(m_allObjects);
        m_allObjects.clear();
        m_freeQueue.clear();
        m_usedCount = 0;
    }

    /* ============================================================
       状态查询
       ============================================================ */

    /*!
     * \brief 当前空闲对象数量
     */
    int Available() const {
        QMutexLocker locker(&m_mutex);
        return m_freeQueue.size();
    }

    /*!
     * \brief 当前正在使用中的对象数量
     */
    int Used() const {
        QMutexLocker locker(&m_mutex);
        return m_usedCount;
    }

    /*!
     * \brief 池子总容量（全部对象数量）
     */
    int Total() const {
        QMutexLocker locker(&m_mutex);
        return m_allObjects.size();
    }

    /*!
     * \brief 是否为空（没有任何对象）
     */
    bool IsEmpty() const {
        QMutexLocker locker(&m_mutex);
        return m_allObjects.isEmpty();
    }

private:
    /* ============================================================
       内部重置检测（SFINAE 技术）
       检测 T 是否有 Reset() 方法，有则调用，无则忽略
       ============================================================ */

    // 检测 T 是否有 Reset() 方法（优先级高）
    template<typename U>
    auto CallResetImpl(U* obj, int) -> decltype(obj->Reset(), void()) {
        obj->Reset();
    }

    // 没有 Reset() 方法时的后备（优先级低）
    template<typename U>
    void CallResetImpl(U* obj, long) {
        (void)obj;  // 什么都不做
    }

    // 对外入口
    void CallReset(T* obj) {
        CallResetImpl(obj, 0);
    }

    mutable QMutex m_mutex;
    QVector<T*> m_allObjects;    // 所有对象（管理生命周期）
    QQueue<T*> m_freeQueue;      // 空闲对象队列
    int m_usedCount = 0;         // 正在使用中的对象数量

    std::function<void(T*)> m_resetFunction;  // 自定义重置函数
};
}
#endif // OBJECTPOOL_H
