#ifndef CHAINBRANCH_H
#define CHAINBRANCH_H

/************************************************************************************************
 * @file       ChainBranch.h
 * @brief      增强型链式分支选择器，替代传统长串 if-else / else if
 * @author     自研通用工具类（长期项目复用）
 * @environment Qt 5.12 + C++11 + Ubuntu 18.04
 * @feature    1. 链式流式写法，结构统一，可读性强
 *             2. 支持等值匹配、任意复杂逻辑条件(&&/||/范围/嵌套)
 *             3. 原生支持结构体/报文/自定义对象多字段组合判断
 *             4. 短路机制：命中首个分支后终止后续判断，行为与原生if-else完全一致
 * @security   1. 私有构造函数，禁止外部手动创建实例，仅通过静态入口调用
 *             2. 禁用拷贝、移动、赋值，避免分支状态标记错乱
 *             3. 成员变量全部私有化，杜绝外部篡改
 * @alias      简写别名：CB（全局可用，3字母，简化书写）
 *             若与其他库冲突，可在包含本头文件前定义宏 CHAINBRANCH_NO_GLOBAL_ALIAS 以禁用全局别名。
 ************************************************************************************************/

#include <functional>
#include <QString>
#include "SqzGlobal.h"
/**
 * @class   ChainBranch
 * @brief   链式分支选择器主类
 * @note    1. 无需提前初始化、无需预注册映射表，调用处直接编写分支逻辑
 *          2. 分支顺序即判断优先级，从上到下依次匹配
 *          3. 所有分支命中后自动短路，不会重复执行
 */

namespace Sqz::Utils {
class SQZ_FRAMEWORK_API ChainBranch
{
public:
    /********************************************************************************************
     * 类型别名定义
     ********************************************************************************************/
    /// 无参执行函数类型（普通分支逻辑）
    using Func = std::function<void()>;

    /// 无参条件谓词：返回bool，判断分支条件是否成立
    using Predicate = std::function<bool()>;

    /********************************************************************************************
     * 静态入口函数：启动整型/字符串分支链
     ********************************************************************************************/
    /**
     * @brief   以【整型/枚举值】作为判断依据，启动分支链
     * @param   value 待判断的整数、枚举值
     * @return  链式调用对象
     */
    static ChainBranch On(int value)
    {
        return ChainBranch(value, true);
    }

    /**
     * @brief   以【字符串】作为判断依据，启动分支链
     * @param   value 待判断的字符串、命令、指令
     * @return  链式调用对象
     */
    static ChainBranch On(const QString& value)
    {
        return ChainBranch(value, false);
    }

    /********************************************************************************************
     * 分支接口1：等值匹配（简单判断，等价于 if(xxx == yyy)）
     ********************************************************************************************/
    /**
     * @brief   整型等值匹配分支
     * @param   caseVal 目标匹配值
     * @param   func    匹配成功后执行的逻辑函数
     * @return  链式调用对象，支持连续调用
     */
    ChainBranch& Case(int caseVal, Func func)
    {
        // 未命中任何分支 + 类型为整型 + 值相等 → 执行逻辑并标记已命中
        if (!m_isMatched && m_isInt && m_intVal == caseVal)
        {
            m_isMatched = true;
            func();
        }
        return *this;
    }

    /**
     * @brief   字符串等值匹配分支
     * @param   caseVal 目标匹配字符串
     * @param   func    匹配成功后执行的逻辑函数
     * @return  链式调用对象，支持连续调用
     */
    ChainBranch& Case(const QString& caseVal, Func func)
    {
        if (!m_isMatched && !m_isInt && m_strVal == caseVal)
        {
            m_isMatched = true;
            func();
        }
        return *this;
    }

    /********************************************************************************************
     * 分支接口2：自定义复杂条件（核心，支持&&/||/范围/嵌套逻辑）
     ********************************************************************************************/
    /**
     * @brief   自定义复杂条件分支
     * @param   cond 条件判断谓词（返回true表示条件成立）
     * @param   func 条件成立后执行的逻辑函数
     * @return  链式调用对象，支持连续调用
     */
    ChainBranch& CaseIf(Predicate cond, Func func)
    {
        if (!m_isMatched && cond())
        {
            m_isMatched = true;
            func();
        }
        return *this;
    }

    /********************************************************************************************
     * 兜底分支：默认分支（等价于原生 else）
     ********************************************************************************************/
    /**
     * @brief   默认兜底分支，所有条件均不成立时执行
     * @param   func 兜底逻辑函数
     */
    void Default(Func func)
    {
        if (!m_isMatched)
        {
            func();
        }
    }
    /**
         * @brief   显式结束分支链，不执行任何默认逻辑
         * @note    该方法用于明确表示“无匹配分支时不做任何操作”。
         *          与不调用 Default() 的行为完全一致，但提高代码可读性。
         * @example CB::On(100).Case(1, []{...}).Case(2, []{...}).End();
         */
        void End()
        {
            // 什么都不做，仅作为链式调用的终结标记
        }
    /********************************************************************************************
     * 嵌套模板类：专门处理 结构体/自定义对象/报文 多字段复杂判断
     ********************************************************************************************/
    /**
     * @class   DataBranch
     * @brief   数据对象分支处理器（泛型模板，适配任意结构体/类）
     * @tparam  T 自定义数据类型（报文、任务结构体、设备参数等）
     */
    template<typename T>
    class DataBranch
    {
    public:
        /// 带数据参数的条件谓词（基于对象字段做判断）
        using DataPredicate = std::function<bool(const T&)>;

        /// 带数据参数的执行函数（分支逻辑可使用对象数据）
        using DataFunc = std::function<void(const T&)>;

        /**
         * @brief   构造函数：绑定外部数据对象
         * @param   data 外部传入的结构体/对象（常引用，避免拷贝）
         */
        explicit DataBranch(const T& data) : m_data(data) {}

        /**
         * @brief   数据对象的复杂条件分支
         * @param   cond 多字段组合条件
         * @param   func 条件成立后执行的逻辑
         * @return  链式调用对象
         */
        DataBranch& CaseIf(DataPredicate cond, DataFunc func)
        {
            if (!m_isMatched && cond(m_data))
            {
                m_isMatched = true;
                func(m_data);
            }
            return *this;
        }

        /**
         * @brief   数据对象分支的默认兜底逻辑
         * @param   func 兜底逻辑
         */
        void Default(DataFunc func)
        {
            if (!m_isMatched)
            {
                func(m_data);
            }
        }

    private:
        const T& m_data;        ///< 绑定的外部数据对象（常引用，保证数据安全）
        bool     m_isMatched = false; ///< 分支命中标记（短路核心）
    };

    /********************************************************************************************
     * 入口函数：绑定结构体/自定义对象，启动数据分支链
     ********************************************************************************************/
    /**
     * @brief   绑定任意结构体/对象，启动多字段分支判断
     * @tparam  T 数据类型
     * @param   data 待判断的结构体/对象
     * @return  数据分支处理器实例
     */
    template<typename T>
    static DataBranch<T> OnData(const T& data)
    {
        return DataBranch<T>(data);
    }

    /********************************************************************************************
     * 安全加固：禁用拷贝、移动、赋值（防止实例状态错乱）
     ********************************************************************************************/
    // 禁用拷贝构造
    ChainBranch(const ChainBranch&) = delete;
    // 禁用移动构造
    ChainBranch(ChainBranch&&) = delete;
    // 禁用拷贝赋值
    ChainBranch& operator=(const ChainBranch&) = delete;
    // 禁用移动赋值
    ChainBranch& operator=(ChainBranch&&) = delete;

private:
    /********************************************************************************************
     * 私有构造函数：禁止外部手动创建实例，仅静态入口可调用
     ********************************************************************************************/
    /**
     * @brief   整型分支私有构造
     * @param   val 整型值
     * @param   isInt 标记当前分支类型（true=整型，false=字符串）
     */
    explicit ChainBranch(int val, bool isInt)
        : m_intVal(val), m_isInt(isInt)
    {}

    /**
     * @brief   字符串分支私有构造
     * @param   val 字符串值
     * @param   isInt 标记当前分支类型（true=整型，false=字符串）
     */
    explicit ChainBranch(const QString& val, bool isInt)
        : m_strVal(val), m_isInt(isInt)
    {}

    /********************************************************************************************
     * 私有成员变量（全部私有化，外部不可访问）
     ********************************************************************************************/
    int         m_intVal    = 0;        ///< 存储整型/枚举判断值
    QString     m_strVal;              ///< 存储字符串判断值
    bool        m_isInt     = true;    ///< 分支类型标记：true=整型，false=字符串
    bool        m_isMatched = false;    ///< 分支命中标记：true=已匹配分支，后续全部短路
};

#ifndef CHAINBRANCH_NO_GLOBAL_ALIAS
/**
 * @brief 全局简写别名 CB（ChainBranch）
 * @note  若用户项目中存在同名冲突，可在包含本头文件之前定义宏：
 *        #define CHAINBRANCH_NO_GLOBAL_ALIAS
 *        此时全局别名 CB 将不会被定义，避免冲突。
 *        推荐使用完整类名 ChainBranch 或自行定义别名。
 */
using CBH = ChainBranch;
#endif // CHAINBRANCH_NO_GLOBAL_ALIAS
}
#endif // CHAINBRANCH_H
