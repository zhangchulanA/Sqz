/*****************************************************************************
**  StateMachine.h
**  通用有限状态机（Qt 版本）
**  无命名空间，无日志，使用 Qt 容器
**
**  使用方法：
**      1. 创建 StateMachine 对象
**      2. 注册状态：registerState("状态名", 进入回调, 退出回调)
**      3. 添加转换：addTransition("源状态", "事件", "目标状态", 守卫Lambda, 动作Lambda)
**      4. 设置初始状态：setInitialState("状态名")
**      5. 触发事件：processEvent("事件名")
**      6. 可选：update() 处理无条件转换；forceTransition() 强制切换；reset() 重置
**
**  注意：守卫和动作中捕获的外部数据生命周期必须长于状态机。
*****************************************************************************/

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <QString>
#include <QVector>
#include <QHash>
#include <functional>
#include <stdexcept>
#include "SqzGlobal.h"
// 状态回调类型：无参数，无返回值
using StateCallback = std::function<void()>;

// 守卫条件：返回 bool
using GuardCondition = std::function<bool()>;

// 动作：无返回值
using Action = std::function<void()>;

// 状态机异常类
class SQZ_FRAMEWORK_API StateMachineError : public std::runtime_error {
public:
    explicit StateMachineError(const QString& msg)
        : std::runtime_error(msg.toStdString()), m_msg(msg) {}
    const QString& message() const { return m_msg; }
private:
    QString m_msg;
};

// 转换规则结构体
struct SQZ_FRAMEWORK_API Transition {
    QString event;          // 触发事件名
    QString fromState;      // 源状态
    QString toState;        // 目标状态
    GuardCondition guard;   // 守卫条件（默认总是 true）
    Action action;          // 转换动作（可选）
};

// 有限状态机主类
class SQZ_FRAMEWORK_API StateMachine {
public:
    StateMachine() {}
    ~StateMachine() {}

    // 禁止拷贝和赋值
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    // 允许移动
    StateMachine(StateMachine&&) = default;
    StateMachine& operator=(StateMachine&&) = default;

    // ---------- 状态管理 ----------
    /**
     * 注册一个状态
     * @param stateName  状态名称（唯一）
     * @param onEnter    进入状态时调用（可为空）
     * @param onExit     退出状态时调用（可为空）
     */
    void registerState(const QString& stateName,
                       StateCallback onEnter = nullptr,
                       StateCallback onExit = nullptr) {
        if (stateName.isEmpty())
            throw StateMachineError("State name cannot be empty");
        if (m_states.contains(stateName))
            throw StateMachineError("State already registered: " + stateName);
        StateInfo info{onEnter, onExit};
        m_states.insert(stateName, info);
    }

    /**
     * 设置初始状态（必须已在 registerState 中注册）
     * @param stateName     初始状态名
     * @param enterCallback 是否自动调用 onEnter（默认 true）
     */
    void setInitialState(const QString& stateName, bool enterCallback = true) {
        if (!m_states.contains(stateName))
            throw StateMachineError("Unknown initial state: " + stateName);
        m_initialState = stateName;
        m_currentState = stateName;
        if (enterCallback) {
            callOnEnter(m_currentState);
        }
    }

    // ---------- 转换规则管理 ----------
    /**
     * 添加一个事件驱动的转换规则
     * @param from   源状态
     * @param event  事件名
     * @param to     目标状态
     * @param guard  守卫条件（默认永远为 true）
     * @param action 转换动作（默认空）
     */
    void addTransition(const QString& from, const QString& event,
                       const QString& to,
                       GuardCondition guard = []() { return true; },
                       Action action = nullptr) {
        if (from.isEmpty() || event.isEmpty() || to.isEmpty())
            throw StateMachineError("From, event, to cannot be empty");
        if (!m_states.contains(from))
            throw StateMachineError("Unknown from state: " + from);
        if (!m_states.contains(to))
            throw StateMachineError("Unknown to state: " + to);
        m_transitions.push_back({event, from, to, guard, action});
    }

    /**
     * 添加一个无条件事件的转换（用于轮询检测）
     * 等价于 addTransition(from, "", to, guard, action)
     */
    void addConditionalTransition(const QString& from, const QString& to,
                                  GuardCondition guard,
                                  Action action = nullptr) {
        addTransition(from, "", to, guard, action);
    }

    // ---------- 事件驱动 ----------
    /**
     * 触发一个事件，状态机尝试执行匹配的转换
     * @param event 事件名
     * @return 是否成功执行了转换（有匹配且守卫通过）
     */
    bool processEvent(const QString& event) {
        if (m_currentState.isEmpty())
            throw StateMachineError("StateMachine not initialized (call setInitialState)");

        // 收集所有从当前状态出发且事件匹配的候选规则
        QVector<const Transition*> candidates;
        for (const auto& t : m_transitions) {
            if (t.fromState == m_currentState && t.event == event) {
                candidates.append(&t);
            }
        }

        // 按添加顺序依次检查守卫，第一个通过的执行转换
        for (const auto* t : candidates) {
            if (t->guard()) {
                if (t->action)
                    t->action();
                doTransition(t->toState);
                return true;
            }
        }
        return false;  // 没有可用的转换
    }

    /**
     * 轮询所有无条件事件（事件名为空）的转换
     * @return 是否执行了任意转换
     */
    bool update() {
        return processEvent("");
    }

    // ---------- 强制控制 ----------
    /**
     * 强制切换到某个状态（不检查守卫和动作，但仍会调用 onExit/onEnter）
     * @param newState      目标状态名
     * @param skipCallbacks 是否跳过所有回调（默认 false）
     */
    void forceTransition(const QString& newState, bool skipCallbacks = false) {
        if (!m_states.contains(newState))
            throw StateMachineError("Unknown state: " + newState);
        if (m_currentState == newState)
            return;

        if (!skipCallbacks) {
            callOnExit(m_currentState);
        }
        m_currentState = newState;
        if (!skipCallbacks) {
            callOnEnter(m_currentState);
        }
    }

    /**
     * 重置状态机到初始状态（保留所有注册的规则）
     * @param skipCallbacks 是否跳过回调
     */
    void reset(bool skipCallbacks = false) {
        if (m_initialState.isEmpty())
            throw StateMachineError("Initial state not set");
        if (m_currentState != m_initialState) {
            if (!skipCallbacks)
                callOnExit(m_currentState);
            m_currentState = m_initialState;
            if (!skipCallbacks)
                callOnEnter(m_currentState);
        }
    }

    // ---------- 查询接口 ----------
    const QString& getCurrentState() const { return m_currentState; }
    bool isInState(const QString& stateName) const { return m_currentState == stateName; }

    /**
     * 获取从当前状态可触发的事件列表（不检查守卫，只返回候选事件名）
     */
    QVector<QString> getAvailableEvents() const {
        QVector<QString> events;
        for (const auto& t : m_transitions) {
            if (t.fromState == m_currentState && !t.event.isEmpty()) {
                if (!events.contains(t.event))
                    events.append(t.event);
            }
        }
        return events;
    }

private:
    struct StateInfo {
        StateCallback onEnter;
        StateCallback onExit;
    };

    void callOnEnter(const QString& state) {
        auto it = m_states.find(state);
        if (it != m_states.end() && it->onEnter) {
            it->onEnter();
        }
    }

    void callOnExit(const QString& state) {
        auto it = m_states.find(state);
        if (it != m_states.end() && it->onExit) {
            it->onExit();
        }
    }

    void doTransition(const QString& newState) {
        if (m_currentState == newState)
            return;
        callOnExit(m_currentState);
        QString oldState = m_currentState;
        m_currentState = newState;
        callOnEnter(m_currentState);
    }

private:
    QHash<QString, StateInfo> m_states;      // 状态名 -> 回调信息
    QVector<Transition>       m_transitions; // 转换规则列表（顺序即优先级）
    QString                   m_initialState;
    QString                   m_currentState;
};

#endif // STATE_MACHINE_H
