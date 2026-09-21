#pragma once

#include <cassert>
#include <stdexcept>
#include <utility>
#include <functional>
#include <atomic>
#include <chrono>
#include <memory>
/**
 * @file Toolbox.h
 * @brief Sqz框架通用工具集
 * 包含：类型安全包装、RAII守卫、Qt辅助、生命周期工具、性能统计
 * C++17，头文件模板，无需cpp实现
 */

//==================== TypeSafety 类型安全工具 ====================

/**
 * @brief NonNull 非空指针包装器
 * 契约：被包装指针一定不为nullptr；不管理所有权；无法防止野/悬垂指针
 */
template<typename T>
class NonNull
{
public:
    explicit NonNull(T* p)
        : mPtr(p)
    {
        if(mPtr == nullptr)
        {
            throw std::invalid_argument("NonNull: constructed with nullptr pointer");
        }
    }

    NonNull() = delete;
    NonNull(std::nullptr_t) = delete;

    NonNull(const NonNull&) = default;
    NonNull& operator=(const NonNull&) = default;
    NonNull(NonNull&&) noexcept = default;
    NonNull& operator=(NonNull&&) noexcept = default;

    [[nodiscard]] T* get() const noexcept
    {
        assert(mPtr != nullptr);
        return mPtr;
    }

    T* operator->() const noexcept
    {
        assert(mPtr != nullptr);
        return mPtr;
    }

    T& operator*() const noexcept
    {
        assert(mPtr != nullptr);
        return *mPtr;
    }

    operator T*() const = delete;

private:
    T* mPtr{nullptr};
};

template<typename T>
using NonNullConst = NonNull<const T>;

/**
 * @brief NonNullUniquePtr 独占所有权且非空的unique_ptr包装
 */
template<typename T>
class NonNullUniquePtr
{
public:
    explicit NonNullUniquePtr(std::unique_ptr<T> ptr)
        : mPtr(std::move(ptr))
    {
        if(!mPtr)
        {
            throw std::invalid_argument("NonNullUniquePtr: empty unique_ptr");
        }
    }

    NonNullUniquePtr() = delete;
    NonNullUniquePtr(std::nullptr_t) = delete;

    NonNullUniquePtr(const NonNullUniquePtr&) = delete;
    NonNullUniquePtr& operator=(const NonNullUniquePtr&) = delete;

    NonNullUniquePtr(NonNullUniquePtr&&) noexcept = default;
    NonNullUniquePtr& operator=(NonNullUniquePtr&&) noexcept = default;

    [[nodiscard]] T* get() const noexcept
    {
        assert(mPtr != nullptr);
        return mPtr.get();
    }

    T* operator->() const noexcept
    {
        assert(mPtr != nullptr);
        return mPtr.get();
    }

    T& operator*() const noexcept
    {
        assert(mPtr != nullptr);
        return *mPtr;
    }

    [[nodiscard]] std::unique_ptr<T> release() noexcept
    {
        return std::move(mPtr);
    }

private:
    std::unique_ptr<T> mPtr;
};

/**
 * @brief TaggedValue 强类型标签包装，避免同底层类型参数混淆
 * @tparam Tag 空标签结构体，编译期区分类型
 * @tparam T 底层存储类型
 */
template<typename Tag, typename T>
struct TaggedValue
{
    T value{};

    constexpr explicit TaggedValue(T v) noexcept
        : value(v)
    {}

    [[nodiscard]] constexpr T get() const noexcept
    {
        return value;
    }

    constexpr bool operator==(const TaggedValue& other) const noexcept
    {
        return value == other.value;
    }

    constexpr bool operator!=(const TaggedValue& other) const noexcept
    {
        return value != other.value;
    }
};

/**
 * @brief ClampedValue 自动限幅数值，赋值永远被限制在Min~Max区间
 */
template<typename T, T Min, T Max>
struct ClampedValue
{
    T mVal{Min};

    constexpr void set(T v)
    {
        if(v < Min)
            mVal = Min;
        else if(v > Max)
            mVal = Max;
        else
            mVal = v;
    }

    [[nodiscard]] constexpr T get() const noexcept
    {
        return mVal;
    }
};

//==================== RaiiTools RAII守卫工具 ====================

/**
 * @brief ScopeGuard 作用域守卫，离开作用域自动执行回调
 */
class ScopeGuard
{
public:
    explicit ScopeGuard(std::function<void()> callback)
        : mCallback(std::move(callback))
    {}

    ~ScopeGuard()
    {
        if(mActive)
        {
            mCallback();
        }
    }

    void dismiss() noexcept
    {
        mActive = false;
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : mCallback(std::move(other.mCallback)), mActive(other.mActive)
    {
        other.mActive = false;
    }

private:
    std::function<void()> mCallback;
    bool mActive{true};
};

/**
 * @brief ValueGuard 变量值守卫，离开作用域自动恢复旧值
 */
template<typename T>
class ValueGuard
{
public:
    explicit ValueGuard(T& ref)
        : mRef(ref), mOld(ref)
    {}

    ~ValueGuard()
    {
        mRef = std::move(mOld);
    }

    ValueGuard(const ValueGuard&) = delete;
    ValueGuard& operator=(const ValueGuard&) = delete;

private:
    T& mRef;
    T mOld;
};

/**
 * @brief TimeCost RAII耗时统计，构造开始计时，调用ms/us获取耗时
 */
struct TimeCost
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point mStart;

    TimeCost()
        : mStart(Clock::now())
    {}

    [[nodiscard]] size_t ms() const
    {
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - mStart).count());
    }

    [[nodiscard]] size_t us() const
    {
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - mStart).count());
    }
};

//==================== QtAux Qt辅助工具 ====================
#ifdef QT_CORE_LIB
#include <QObject>

/**
 * @brief QSignalBlockGuard RAII局部屏蔽QObject信号，出域自动恢复原有状态
 */
struct QSignalBlockGuard
{
    QObject* mObj{nullptr};
    bool mOldState{false};

    explicit QSignalBlockGuard(QObject* obj)
        : mObj(obj), mOldState(obj->blockSignals(true))
    {}

    ~QSignalBlockGuard()
    {
        if(mObj)
        {
            mObj->blockSignals(mOldState);
        }
    }

    QSignalBlockGuard(const QSignalBlockGuard&) = delete;
    QSignalBlockGuard& operator=(const QSignalBlockGuard&) = delete;
};

#endif

//==================== Lifecycle 生命周期工具 ====================

/**
 * @brief StaticSingleton Meyers线程安全静态单例基类
 * 继承此类，友元声明，私有构造，外部不可new
 */
template<typename T>
class StaticSingleton
{
public:
    static T& instance()
    {
        static T obj;
        return obj;
    }

protected:
    StaticSingleton() = default;
    ~StaticSingleton() = default;

public:
    StaticSingleton(const StaticSingleton&) = delete;
    StaticSingleton& operator=(const StaticSingleton&) = delete;
};

/**
 * @brief OnceFlag 线程安全一次性执行标记，替代static bool初始化标记
 */
struct OnceFlag
{
    std::atomic<bool> done{false};

    /**
     * @brief once 返回true代表本次是第一次执行
     */
    bool once()
    {
        bool expect = false;
        return done.compare_exchange_strong(expect, true);
    }
};
