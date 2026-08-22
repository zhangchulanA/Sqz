// ============================================================
// KeyManager.h
// 全局按键管理器 - 统一管理物理按键和触摸屏按键
// 支持动态绑定/重绑定、场景切换、启用/禁用、批量操作
// ============================================================

#ifndef KEYMANAGER_H
#define KEYMANAGER_H

#include <QObject>
#include <QMap>
#include <qhotkey.h>
#include <QKeySequence>
#include <QReadWriteLock>
#include <functional>
#include <QVariant>

// ============================================================
// 按键码枚举
// 定义所有可用的按键，方便统一管理
// ============================================================
enum class KeyCode {
    // 数字键 0-9（主键盘数字，非小键盘）
    Key0, Key1, Key2, Key3, Key4,
    Key5, Key6, Key7, Key8, Key9,

    // 功能键 F1-F9
    F1, F2, F3, F4, F5,
    F6, F7, F8, F9,

    // 方向键
    Up, Down, Left, Right,

    // 控制键
    Enter, Escape, Backspace,
    Tab, Space,

    // 自定义扩展（可根据硬件需求增加）
    Custom1, Custom2, Custom3, Custom4, Custom5,

    // 空键，用于初始化
    None
};

// 使 KeyCode 可以在 QVariant 中使用
Q_DECLARE_METATYPE(KeyCode)


// ============================================================
// KeyAction 类
// 封装一个按键对应的回调动作
// 支持三种回调类型：无参、带参、带返回值
// ============================================================
class KeyAction {
public:
    // 回调类型定义
    using Callback  = std::function<void()>;                      // 无参无返回值

    KeyAction() = default;

    // 构造：无参回调
    KeyAction(Callback callback) : m_callback(callback) {}

    // 执行回调
       void execute() {
           if (m_callback) {
               m_callback();
           }
       }

    /** 检查是否有有效的回调 */
    bool isValid() const {
        return m_callback != nullptr;
    }

private:
    Callback  m_callback;

};


// ============================================================
// KeyManager 类
// 全局按键管理器（单例）
// 统一管理所有按键的绑定、触发、场景切换
// ============================================================
class KeyManager : public QObject
{
    Q_OBJECT

public:
    /** 获取单例实例 */
    static KeyManager* instance();

    // ============================================================
    // 核心接口：bind - 绑定/重绑定按键
    // 自动处理首次绑定和重绑定，调用者无需关心是否已绑定
    // ============================================================

    //绑定无参回调
    void bind(KeyCode key, KeyAction::Callback  callback);
    //解绑单个按键
    void unbind(KeyCode key);
    // 解绑所有按键
    void unbindAll();

    // ============================================================
    // 触发接口（触摸屏/软件模拟按键用）
    // ============================================================

    /**
     * @brief 手动触发一个按键动作
     * @param key 按键码
     * @param param 可选参数
     * @return 回调的返回值
     *
     * 使用场景：触摸屏按钮点击时，模拟对应的物理按键
     */
    void trigger(KeyCode key);

    // ============================================================
    // 启用/禁用按键（不删除绑定，仅开关热键）
    // ============================================================

    /**
     * @brief 启用或禁用某个按键
     * @param key 按键码
     * @param enabled true=启用，false=禁用
     *
     * 使用场景：进入设置界面时暂时禁用某些功能键
     */
    void setEnabled(KeyCode key, bool enabled);

    /** 检查某个按键是否启用 */
    bool isEnabled(KeyCode key) const;

    // ============================================================
    // 场景管理
    // 不同界面下，同一按键可以有不同的功能
    // ============================================================

    /** 保存当前所有绑定为指定场景 */
    void saveContext(const QString& name);

    /** 加载指定场景（替换当前所有绑定） */
    void loadContext(const QString& name);

    /**
     * @brief 增量合并场景
     * @param name 场景名
     *
     * 将指定场景的绑定合并到当前，同名按键被覆盖
     * 不同于 loadContext，mergeContext 只覆盖同名按键，不删除其他按键
     */
    void mergeContext(const QString& name);

    /** 删除指定场景 */
    void clearContext(const QString& name);

    /** 获取当前场景名 */
    QString currentContext() const { return m_currentContext; }

    // ============================================================
    // 批量操作
    // ============================================================

    /** 批量绑定无参回调 */
    void bindBatch(const QMap<KeyCode, KeyAction::Callback >& bindings);
    // ============================================================
    // 状态查询
    // ============================================================

    /** 检查某个按键是否已绑定 */
    bool isBound(KeyCode key) const;

    /** 获取按键的显示名称（用于 UI） */
    QString getKeyDisplayName(KeyCode key) const;

    /** 获取所有已绑定的按键列表 */
    QList<KeyCode> getBoundKeys() const;

    /** 获取已绑定按键的数量 */
    int boundCount() const { return m_keyActions.size(); }

signals:
    /** 按键被按下信号（无参） */
    void keyPressed(KeyCode key);

    /** 按键被按下信号（带参） */
    void keyPressedWithParam(KeyCode key, const QVariant& param);

    /** 场景切换完成信号 */
    void contextSwitched(const QString& oldContext, const QString& newContext);

    /** 按键绑定状态变化信号 */
    void bindingChanged(KeyCode key, bool bound);

    /** 按键启用状态变化信号 */
    void keyEnabledChanged(KeyCode key, bool enabled);

private:
    /** 私有构造函数（单例） */
    explicit KeyManager(QObject* parent = nullptr);

    /** 析构函数（清理所有热键） */
    ~KeyManager();

    /** 将 KeyCode 转换为 QKeySequence 字符串 */
    QString keyCodeToSequence(KeyCode key) const;

    /** 注册热键到系统 */
    void registerHotkey(KeyCode key);

    /** 注销热键 */
    void unregisterHotkey(KeyCode key);

    /** 更新热键的启用状态（注册/注销） */
    void updateHotkeyState(KeyCode key);

private:
    static KeyManager* m_instance;                           // 单例实例

    QMap<KeyCode, KeyAction> m_keyActions;                   // 按键 → 动作映射
    QMap<KeyCode, QHotkey*> m_hotkeys;                       // 按键 → 热键对象
    QMap<KeyCode, bool> m_enabledStates;                     // 按键 → 启用状态
    QMap<QString, QMap<KeyCode, KeyAction>> m_contexts;      // 场景名 → 按键映射
    QString m_currentContext;                                // 当前场景名

    mutable QReadWriteLock m_lock;                           // 读写锁，提高并发性能
};

#endif // KEYMANAGER_H
