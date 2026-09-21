// ============================================================
// KeyManager.cpp
// 全局按键管理器实现
// ============================================================

#include "KeyManager.h"
#include <QDebug>

// ============================================================
// 单例实现
// ============================================================

KeyManager* KeyManager::m_instance = nullptr;

KeyManager* KeyManager::instance()
{
    if (!m_instance) {
        m_instance = new KeyManager();
    }
    return m_instance;
}

KeyManager::KeyManager(QObject* parent)
    : QObject(parent)
    , m_currentContext("default")
{
}

KeyManager::~KeyManager()
{
    // 清理所有热键
    for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
        if (it.value()) {
            it.value()->setRegistered(false);
            delete it.value();
        }
    }
    m_hotkeys.clear();
}


// ============================================================
// bind - 绑定/重绑定按键
// 自动判断首次绑定还是重绑定，调用者无需关心
// ============================================================

void KeyManager::bind(KeyCode key, KeyAction::Callback callback)
{
    QWriteLocker locker(&m_lock);

    bool wasBound = m_keyActions.contains(key);

    if (wasBound) {
        // 已绑定：直接替换回调，复用热键对象
        m_keyActions[key] = KeyAction(callback);
        qDebug() << "[KeyManager] Rebound key:" << (int)key;
    } else {
        // 首次绑定：创建回调并注册热键
        m_keyActions[key] = KeyAction(callback);
        registerHotkey(key);
        emit bindingChanged(key, true);
        qDebug() << "[KeyManager] Bound key:" << (int)key;
    }
}


// ============================================================
// unbind - 解绑按键
// ============================================================

void KeyManager::unbind(KeyCode key)
{
    QWriteLocker locker(&m_lock);

    if (m_keyActions.contains(key)) {
        unregisterHotkey(key);
        m_keyActions.remove(key);
        m_enabledStates.remove(key);
        emit bindingChanged(key, false);
        qDebug() << "[KeyManager] Unbound key:" << (int)key;
    }
}

void KeyManager::unbindAll()
{
    QWriteLocker locker(&m_lock);

    QList<KeyCode> keys = m_keyActions.keys();
    for (KeyCode key : keys) {
        unregisterHotkey(key);
        m_enabledStates.remove(key);
        emit bindingChanged(key, false);
    }
    m_keyActions.clear();
    qDebug() << "[KeyManager] Unbound all keys";
}


// ============================================================
// trigger - 手动触发按键（触摸屏用）
// ============================================================

void KeyManager::trigger(KeyCode key)
{
    QReadLocker locker(&m_lock);

    // 检查是否已绑定
    if (!m_keyActions.contains(key)) {
        qWarning() << "[KeyManager] Trigger failed: key not bound" << static_cast<int>(key);
        return;
    }

    // 检查是否被禁用
    if (m_enabledStates.value(key, true) == false) {
        qDebug() << "[KeyManager] Trigger blocked: key disabled" << static_cast<int>(key);
        return;
    }

    // 执行回调
    m_keyActions[key].execute();
    emit keyPressed(key);
}


// ============================================================
// setEnabled / isEnabled - 启用/禁用按键
// ============================================================

void KeyManager::setEnabled(KeyCode key, bool enabled)
{
    QWriteLocker locker(&m_lock);

    if (!m_keyActions.contains(key)) {
        return;
    }

    m_enabledStates[key] = enabled;
    updateHotkeyState(key);
    emit keyEnabledChanged(key, enabled);
    qDebug() << "[KeyManager] Key" << (int)key << "enabled:" << enabled;
}

bool KeyManager::isEnabled(KeyCode key) const
{
    QReadLocker locker(&m_lock);
    return m_enabledStates.value(key, true);
}


// ============================================================
// saveContext / loadContext / mergeContext - 场景管理
// ============================================================

void KeyManager::saveContext(const QString& name)
{
    QReadLocker locker(&m_lock);
    m_contexts[name] = m_keyActions;
    qDebug() << "[KeyManager] Saved context:" << name
             << "with" << m_keyActions.size() << "bindings";
}

void KeyManager::loadContext(const QString& name)
{
    QWriteLocker locker(&m_lock);

    if (!m_contexts.contains(name)) {
        qWarning() << "[KeyManager] Context not found:" << name;
        return;
    }

    QString oldContext = m_currentContext;

    // 1. 注销当前所有热键
    for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
        if (it.value()) {
            it.value()->setRegistered(false);
        }
    }
    m_hotkeys.clear();
    m_enabledStates.clear();

    // 2. 加载新场景
    m_keyActions = m_contexts[name];
    m_currentContext = name;

    // 3. 重新注册所有热键
    for (auto it = m_keyActions.begin(); it != m_keyActions.end(); ++it) {
        registerHotkey(it.key());
    }

    emit contextSwitched(oldContext, name);
    qDebug() << "[KeyManager] Loaded context:" << name
             << "with" << m_keyActions.size() << "bindings";
}

void KeyManager::mergeContext(const QString& name)
{
    QWriteLocker locker(&m_lock);

    if (!m_contexts.contains(name)) {
        qWarning() << "[KeyManager] Context not found:" << name;
        return;
    }

    const auto& contextBindings = m_contexts[name];
    int mergeCount = 0;

    for (auto it = contextBindings.begin(); it != contextBindings.end(); ++it) {
        KeyCode key = it.key();

        // 如果该按键已绑定，先注销旧热键
        if (m_hotkeys.contains(key)) {
            unregisterHotkey(key);
        }

        // 覆盖或新增
        m_keyActions[key] = it.value();
        registerHotkey(key);
        mergeCount++;
        emit bindingChanged(key, true);
    }

    qDebug() << "[KeyManager] Merged context:" << name
             << "overwrote" << mergeCount << "keys";
}

void KeyManager::clearContext(const QString& name)
{
    QWriteLocker locker(&m_lock);
    m_contexts.remove(name);
    qDebug() << "[KeyManager] Cleared context:" << name;
}


// ============================================================
// bindBatch - 批量绑定
// ============================================================

void KeyManager::bindBatch(const QMap<KeyCode, KeyAction::Callback>& bindings)
{
    QWriteLocker locker(&m_lock);

    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        KeyCode key = it.key();

        if (m_keyActions.contains(key)) {
            m_keyActions[key] = KeyAction(it.value());
        } else {
            m_keyActions[key] = KeyAction(it.value());
            registerHotkey(key);
            emit bindingChanged(key, true);
        }
    }
    qDebug() << "[KeyManager] Batch bound" << bindings.size() << "keys";
}


// ============================================================
// isBound / getBoundKeys / getKeyDisplayName - 状态查询
// ============================================================

bool KeyManager::isBound(KeyCode key) const
{
    QReadLocker locker(&m_lock);
    return m_keyActions.contains(key);
}

QList<KeyCode> KeyManager::getBoundKeys() const
{
    QReadLocker locker(&m_lock);
    return m_keyActions.keys();
}

QString KeyManager::getKeyDisplayName(KeyCode key) const
{
    switch (key) {
        // 数字键
        case KeyCode::Key0: return "0";
        case KeyCode::Key1: return "1";
        case KeyCode::Key2: return "2";
        case KeyCode::Key3: return "3";
        case KeyCode::Key4: return "4";
        case KeyCode::Key5: return "5";
        case KeyCode::Key6: return "6";
        case KeyCode::Key7: return "7";
        case KeyCode::Key8: return "8";
        case KeyCode::Key9: return "9";

        // 功能键
        case KeyCode::F1:  return "F1";
        case KeyCode::F2:  return "F2";
        case KeyCode::F3:  return "F3";
        case KeyCode::F4:  return "F4";
        case KeyCode::F5:  return "F5";
        case KeyCode::F6:  return "F6";
        case KeyCode::F7:  return "F7";
        case KeyCode::F8:  return "F8";
        case KeyCode::F9:  return "F9";

        // 方向键
        case KeyCode::Up:    return "↑";
        case KeyCode::Down:  return "↓";
        case KeyCode::Left:  return "←";
        case KeyCode::Right: return "→";

        // 控制键
        case KeyCode::Enter:     return "Enter";
        case KeyCode::Escape:    return "Esc";
        case KeyCode::Backspace: return "⌫";
        case KeyCode::Tab:       return "Tab";
        case KeyCode::Space:     return "Space";

        // 自定义
        case KeyCode::Custom1: return "C1";
        case KeyCode::Custom2: return "C2";
        case KeyCode::Custom3: return "C3";
        case KeyCode::Custom4: return "C4";
        case KeyCode::Custom5: return "C5";

        default: return "?";
    }
}


// ============================================================
// 内部实现：热键注册/注销
// ============================================================

QString KeyManager::keyCodeToSequence(KeyCode key) const
{
    switch (key) {
        // 数字键
        case KeyCode::Key0: return "0";
        case KeyCode::Key1: return "1";
        case KeyCode::Key2: return "2";
        case KeyCode::Key3: return "3";
        case KeyCode::Key4: return "4";
        case KeyCode::Key5: return "5";
        case KeyCode::Key6: return "6";
        case KeyCode::Key7: return "7";
        case KeyCode::Key8: return "8";
        case KeyCode::Key9: return "9";

        // 功能键
        case KeyCode::F1:  return "F1";
        case KeyCode::F2:  return "F2";
        case KeyCode::F3:  return "F3";
        case KeyCode::F4:  return "F4";
        case KeyCode::F5:  return "F5";
        case KeyCode::F6:  return "F6";
        case KeyCode::F7:  return "F7";
        case KeyCode::F8:  return "F8";
        case KeyCode::F9:  return "F9";

        // 方向键
        case KeyCode::Up:    return "Up";
        case KeyCode::Down:  return "Down";
        case KeyCode::Left:  return "Left";
        case KeyCode::Right: return "Right";

        // 控制键
        case KeyCode::Enter:     return "Enter";
        case KeyCode::Escape:    return "Escape";
        case KeyCode::Backspace: return "Backspace";
        case KeyCode::Tab:       return "Tab";
        case KeyCode::Space:     return "Space";

        default:
            return QString();
    }
}

void KeyManager::registerHotkey(KeyCode key)
{
    // 如果已有热键，先清理
    if (m_hotkeys.contains(key) && m_hotkeys[key]) {
        m_hotkeys[key]->setRegistered(false);
        delete m_hotkeys[key];
    }

    // 将 KeyCode 转为快捷键字符串
    QString seq = keyCodeToSequence(key);
    if (seq.isEmpty()) {
        qWarning() << "[KeyManager] Cannot convert KeyCode to sequence:" << (int)key;
        return;
    }

    // 创建并注册热键
    QHotkey* hotkey = new QHotkey(QKeySequence(seq), true, this);

    if (!hotkey->isRegistered()) {
        qWarning() << "[KeyManager] Hotkey registration failed for:" << seq;
        delete hotkey;
        return;
    }

    m_hotkeys[key] = hotkey;

    // 检查是否被禁用
    bool enabled = m_enabledStates.value(key, true);
    if (!enabled) {
        hotkey->setRegistered(false);
    }

    // 连接热键信号
    connect(hotkey, &QHotkey::activated, this, [this, key]() {
        // 检查是否已绑定且启用
        if (m_keyActions.contains(key) && m_enabledStates.value(key, true)) {
            m_keyActions[key].execute();
            emit keyPressed(key);
        }
    });

    qDebug() << "[KeyManager] Registered hotkey:" << seq
             << "for key:" << (int)key << "enabled:" << enabled;
}

void KeyManager::unregisterHotkey(KeyCode key)
{
    if (m_hotkeys.contains(key)) {
        if (m_hotkeys[key]) {
            m_hotkeys[key]->setRegistered(false);
            delete m_hotkeys[key];
        }
        m_hotkeys.remove(key);
    }
}

void KeyManager::updateHotkeyState(KeyCode key)
{
    if (!m_hotkeys.contains(key)) {
        return;
    }

    bool enabled = m_enabledStates.value(key, true);
    m_hotkeys[key]->setRegistered(enabled);
    qDebug() << "[KeyManager] Updated hotkey state:" << (int)key
             << "enabled:" << enabled;
}
