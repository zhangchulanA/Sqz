#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QObject>
#include <QUndoStack>
#include <QUndoView>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QKeySequence>
#include <QPointer>
#include <functional>
#include <QMap>
#include <QDebug>

/**
 * @brief 通用撤销管理器 - 增强版
 *
 * 支持两种使用方式：
 * 1. 传统方式：继承 QUndoCommand 写专用类
 * 2. 快速方式：使用 lambda 直接创建命令（推荐）
 *
 * 示例（快速方式）：
 *   UndoManager::instance()->execCommand(
 *       "修改颜色",
 *       [this]() { m_label->setColor(Qt::red); },   // 执行
 *       [this]() { m_label->setColor(m_oldColor); } // 撤销
 *   );
 */


// ==================== 通用命令实现 ====================

class GenericCommand : public QUndoCommand {
public:
    GenericCommand(std::function<void()> redoFunc,
                   std::function<void()> undoFunc,
                   const QString& text,
                   QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_redoFunc(redoFunc)
        , m_undoFunc(undoFunc) {
    }

    void undo() override {
        if (m_undoFunc) m_undoFunc();
    }

    void redo() override {
        if (m_redoFunc) m_redoFunc();
    }

private:
    std::function<void()> m_redoFunc;
    std::function<void()> m_undoFunc;
};

// ==================== 状态快照命令 ====================

template<typename StateType>
class StateCommand : public QUndoCommand {
public:
    StateCommand(std::function<void(const StateType&)> restoreFunc,
                 const StateType& oldState,
                 const StateType& newState,
                 const QString& text,
                 QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_restoreFunc(restoreFunc)
        , m_oldState(oldState)
        , m_newState(newState) {
    }

    void undo() override {
        if (m_restoreFunc) m_restoreFunc(m_oldState);
    }

    void redo() override {
        if (m_restoreFunc) m_restoreFunc(m_newState);
    }

private:
    std::function<void(const StateType&)> m_restoreFunc;
    StateType m_oldState;
    StateType m_newState;
};

class UndoManager : public QObject {
    Q_OBJECT

public:
    // ==================== 单例 ====================
    static UndoManager* instance();

    // ==================== 基础 API ====================

    /**
     * @brief 执行一个命令（推荐使用）
     * @param text 命令显示名称
     * @param redoFunc 执行/重做函数
     * @param undoFunc 撤销函数
     *
     * 这是最常用的方式，一行代码搞定一个可撤销操作
     */
    void execCommand(const QString& text,
                     std::function<void()> redoFunc,
                     std::function<void()> undoFunc) {
        auto* cmd = new GenericCommand(redoFunc, undoFunc, text);
        push(cmd);
    }

    /**
     * @brief 执行一个"状态快照"命令（自动保存撤销状态）
     * @param text 命令显示名称
     * @param saveFunc 保存当前状态的函数（返回 QVariant）
     * @param restoreFunc 恢复状态的函数（接收 QVariant）
     * @param execFunc 执行操作的函数
     *
     * 适用于状态保存比较复杂的场景，自动帮你保存和恢复
     */
    template<typename StateType>
    void execStateCommand(const QString& text,
                          std::function<StateType()> saveFunc,
                          std::function<void(const StateType&)> restoreFunc,
                          std::function<void()> execFunc) {
        // 执行前保存状态
        StateType oldState = saveFunc();

        // 执行操作
        execFunc();

        // 获取新状态（用于显示）
        StateType newState = saveFunc();

        // 创建命令
        auto* cmd = new StateCommand<StateType>(
            restoreFunc, oldState, newState, text
        );
        push(cmd);
    }

    /**
     * @brief 推入一个自定义命令
     * @param cmd 命令对象（由 new 创建，栈自动管理）
     */
    void push(QUndoCommand* cmd);

    /**
     * @brief 撤销
     */
    void undo();

    /**
     * @brief 重做
     */
    void redo();

    /**
     * @brief 清空历史
     */
    void clear();

    /**
     * @brief 标记为已保存
     */
    void setClean();

    // ==================== 宏命令 ====================

    /**
     * @brief 开始宏（批量操作）
     */
    void beginMacro(const QString& text);

    /**
     * @brief 结束宏
     */
    void endMacro();

    // ==================== 批量操作便捷方法 ====================

    /**
     * @brief 批量执行多个命令（自动组成宏）
     * @param text 宏名称
     * @param commands 命令列表，每个元素是 {执行函数, 撤销函数, 显示文本}
     */
    void execBatch(const QString& text,
                   const QList<std::tuple<QString, std::function<void()>, std::function<void()>>>& commands) {
        beginMacro(text);
        for (const auto& [cmdText, redoFunc, undoFunc] : commands) {
            auto* cmd = new GenericCommand(redoFunc, undoFunc, cmdText);
            push(cmd);
        }
        endMacro();
    }

    // ==================== 状态查询 ====================

    QUndoStack* stack() { return &m_stack; }
    bool canUndo() const { return m_stack.canUndo(); }
    bool canRedo() const { return m_stack.canRedo(); }
    bool isClean() const { return m_stack.isClean(); }
    int count() const { return m_stack.count(); }
    QString undoText() const { return m_stack.undoText(); }
    QString redoText() const { return m_stack.redoText(); }

    // ==================== UI 设置 ====================

    void setupMenu(QMenu* menu);
    void setupToolBar(QToolBar* toolbar);
    void setUndoLimit(int limit);
    QUndoView* createUndoView(QWidget* parent = nullptr);

signals:
    void cleanChanged(bool clean);
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void countChanged(int count);
    void indexChanged(int index);
    void commandExecuted(const QString& text);

private slots:
    void onStackChanged();

private:
    UndoManager() = default;
    ~UndoManager() = default;
    UndoManager(const UndoManager&) = delete;
    UndoManager& operator=(const UndoManager&) = delete;

    void updateActions();

    QUndoStack m_stack;
    QPointer<QAction> m_undoAction;
    QPointer<QAction> m_redoAction;
    QPointer<QAction> m_clearAction;
    bool m_macroActive = false;
};

#endif // UNDOMANAGER_H
