// ShortcutManager.cpp
#include "ShortcutManager.h"
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QJsonArray>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Sqz {

// ============================================================
// 单例
// ============================================================

ShortcutManager* ShortcutManager::s_instance = nullptr;

ShortcutManager* ShortcutManager::Instance()
{
    if (!s_instance) {
        s_instance = new ShortcutManager();
    }
    return s_instance;
}

ShortcutManager::ShortcutManager(QObject* parent)
    : QObject(parent)
    , m_globalImpl(new GlobalShortcutImpl(this))  // ⭐ 使用 new 创建
{
}

ShortcutManager::~ShortcutManager()
{
    // ⭐ 清理全局快捷键实现
    if (m_globalImpl) {
        delete m_globalImpl;
        m_globalImpl = nullptr;
    }

    for (auto& info : m_shortcuts) {
        if (info.shortcut) {
            delete info.shortcut;
        }
    }
}

// ============================================================
// 批量注册
// ============================================================

ShortcutManager& ShortcutManager::RegisterBatch(
    const QMap<QString, QString>& shortcuts,
    std::function<void(const QString&)> handler)
{
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        QString id = it.key();
        QString key = it.value();
        Register(id, key, [handler, id]() {
            if (handler) handler(id);
        });
    }
    return *this;
}

ShortcutManager& ShortcutManager::RegisterBatch(
    const QMap<QString, QPair<QString, QString>>& shortcuts,
    std::function<void(const QString&)> handler)
{
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        QString id = it.key();
        QString key = it.value().first;
        QString displayName = it.value().second;
        Register(id, key, [handler, id]() {
            if (handler) handler(id);
        }, displayName);
    }
    return *this;
}

bool ShortcutManager::LoadFromJson(
    const QString& jsonPath,
    std::function<void(const QString&)> handler)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开快捷键配置文件:" << jsonPath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "JSON 格式错误:" << jsonPath;
        return false;
    }

    QJsonObject obj = doc.object();
    QMap<QString, QString> shortcuts;

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString id = it.key();
        QJsonValue value = it.value();

        if (value.isObject()) {
            QJsonObject item = value.toObject();
            if (item.contains("key")) {
                shortcuts[id] = item["key"].toString();
            }
        } else if (value.isString()) {
            shortcuts[id] = value.toString();
        }
    }

    RegisterBatch(shortcuts, handler);
    return true;
}

// ============================================================
// 单个注册
// ============================================================

ShortcutManager& ShortcutManager::Register(
    const QString& id,
    const QString& keySequence,
    std::function<void()> callback,
    const QString& displayName)
{
    if (m_shortcuts.contains(id)) {
        qWarning() << "快捷键 ID 已存在:" << id;
        return *this;
    }

    ShortcutInfo info;
    info.id = id;
    info.displayName = displayName.isEmpty() ? id : displayName;
    info.callback = callback;

    if (!parseKeySequence(keySequence, info)) {
        qWarning() << "无效的快捷键格式:" << keySequence;
        return *this;
    }

    if (detectConflict(info.keySequence, id)) {
        qWarning() << "快捷键冲突:" << keySequence << "已被使用";
        return *this;
    }

    registerShortcutInternal(info);
    m_shortcuts[id] = info;

    emit shortcutChanged(id, "", keySequence);
    return *this;
}

ShortcutManager& ShortcutManager::Register(
    const QString& id,
    const QString& keySequence,
    QObject* parent,
    std::function<void()> callback,
    const QString& displayName)
{
    Register(id, keySequence, callback, displayName);
    if (parent) {
        QObject::connect(parent, &QObject::destroyed, [this, id]() {
            unregisterShortcut(id);
        });
    }
    return *this;
}

ShortcutManager& ShortcutManager::RegisterContext(
    const QString& id,
    const QString& keySequence,
    const QString& context,
    std::function<void()> callback,
    const QString& displayName)
{
    Register(id, keySequence, callback, displayName);
    if (m_shortcuts.contains(id)) {
        m_shortcuts[id].level = ShortcutLevel::Context;
        m_shortcuts[id].context = context;
        m_contextShortcuts[context].append(id);
    }
    return *this;
}

ShortcutManager& ShortcutManager::RegisterGlobal(
    const QString& id,
    const QString& keySequence,
    std::function<void()> callback,
    const QString& displayName)
{
    Register(id, keySequence, callback, displayName);

    if (m_shortcuts.contains(id)) {
        m_shortcuts[id].level = ShortcutLevel::Global;
        m_shortcuts[id].isGlobal = true;

        if (m_globalImpl && m_globalImpl->isSupported()) {
            m_globalImpl->registerShortcut(id, m_shortcuts[id].keySequence, callback);
        } else {
            qWarning() << "当前平台不支持全局快捷键";
        }
    }

    return *this;
}

ShortcutManager& ShortcutManager::TriggerOnRelease(bool release)
{
    if (m_shortcuts.isEmpty()) {
        return *this;
    }

    auto it = m_shortcuts.end();
    --it;
    it.value().trigger = release ? KeyTrigger::Release : KeyTrigger::Press;
    return *this;
}

// ============================================================
// 查询 API
// ============================================================

QString ShortcutManager::GetShortcut(const QString& id) const
{
    if (!m_shortcuts.contains(id)) {
        return QString();
    }
    return m_shortcuts[id].keySequence.toString(QKeySequence::NativeText);
}

QKeySequence ShortcutManager::GetKeySequence(const QString& id) const
{
    if (!m_shortcuts.contains(id)) {
        return QKeySequence();
    }
    return m_shortcuts[id].keySequence;
}

bool ShortcutManager::HasShortcut(const QString& id) const
{
    return m_shortcuts.contains(id);
}

bool ShortcutManager::IsSingleKey(const QString& id) const
{
    if (!m_shortcuts.contains(id)) {
        return false;
    }
    return m_shortcuts[id].isSingleKey;
}

QList<ShortcutManager::ShortcutInfo> ShortcutManager::GetAllShortcuts() const
{
    return m_shortcuts.values();
}

QString ShortcutManager::MenuText(const QString& id) const
{
    if (!m_shortcuts.contains(id)) {
        return id;
    }
    const auto& info = m_shortcuts[id];
    QString keyStr = info.keySequence.toString(QKeySequence::NativeText);
    if (keyStr.isEmpty()) {
        return info.displayName;
    }
    return QString("%1 (%2)").arg(info.displayName, keyStr);
}

// ============================================================
// 管理 API
// ============================================================

ShortcutManager& ShortcutManager::SetParentWindow(QWidget* parent)
{
    m_parentWindow = parent;
    if (parent) {
        setupEventFilter();
    }
    return *this;
}

void ShortcutManager::SetEnabled(const QString& id, bool enabled)
{
    if (!m_shortcuts.contains(id)) {
        return;
    }

    if (enabled) {
        m_disabledShortcuts.remove(id);
    } else {
        m_disabledShortcuts.insert(id);
    }

    auto& info = m_shortcuts[id];
    info.enabled = enabled;
    if (info.shortcut) {
        info.shortcut->setEnabled(enabled);
    }

    if (info.isGlobal && m_globalImpl) {
        if (!enabled) {
            m_globalImpl->unregisterShortcut(id);
        } else {
            m_globalImpl->registerShortcut(id, info.keySequence, info.callback);
        }
    }
}

void ShortcutManager::SetAllEnabled(bool enabled)
{
    m_allEnabled = enabled;
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        SetEnabled(it.key(), enabled);
    }
}

void ShortcutManager::ActivateContext(const QString& context)
{
    m_activeContext = context;
}

QString ShortcutManager::KeyFriendlyName(Qt::Key key)
{
    static QMap<Qt::Key, QString> map = {
        {Qt::Key_Space, "空格键"},
        {Qt::Key_Escape, "Esc键"},
        {Qt::Key_Tab, "Tab键"},
        {Qt::Key_Backspace, "退格键"},
        {Qt::Key_Enter, "回车键"},
        {Qt::Key_Return, "回车键"},
        {Qt::Key_Delete, "Delete键"},
        {Qt::Key_Insert, "Insert键"},
        {Qt::Key_Home, "Home键"},
        {Qt::Key_End, "End键"},
        {Qt::Key_PageUp, "PageUp键"},
        {Qt::Key_PageDown, "PageDown键"},
        {Qt::Key_Up, "上箭头"},
        {Qt::Key_Down, "下箭头"},
        {Qt::Key_Left, "左箭头"},
        {Qt::Key_Right, "右箭头"},
        {Qt::Key_F1, "F1键"},
        {Qt::Key_F2, "F2键"},
        {Qt::Key_F3, "F3键"},
        {Qt::Key_F4, "F4键"},
        {Qt::Key_F5, "F5键"},
        {Qt::Key_F6, "F6键"},
        {Qt::Key_F7, "F7键"},
        {Qt::Key_F8, "F8键"},
        {Qt::Key_F9, "F9键"},
        {Qt::Key_F10, "F10键"},
        {Qt::Key_F11, "F11键"},
        {Qt::Key_F12, "F12键"},
    };

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QString(QChar(key));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QString(QChar(key));
    }

    return map.value(key, QKeySequence(key).toString());
}

// ============================================================
// 内部实现
// ============================================================

bool ShortcutManager::registerShortcutInternal(const ShortcutInfo& info)
{
    if (info.isSingleKey) {
        registerSingleKeyShortcut(info);
        return true;
    } else {
        if (!m_parentWindow) {
            qWarning() << "注册组合键需要先设置父窗口: SetParentWindow()";
            return false;
        }

        auto* shortcut = new QShortcut(info.keySequence, m_parentWindow);
        shortcut->setContext(Qt::ApplicationShortcut);

        QObject::connect(shortcut, &QShortcut::activated, [this, info]() {
            if (m_allEnabled && info.enabled) {
                if (info.callback) info.callback();
                emit shortcutTriggered(info.id);
            }
        });

        m_shortcuts[info.id].shortcut = shortcut;
        return true;
    }
}

void ShortcutManager::registerSingleKeyShortcut(const ShortcutInfo& info)
{
    m_singleKeyMap[info.nativeKey].append(info.id);
}

void ShortcutManager::unregisterShortcut(const QString& id)
{
    if (!m_shortcuts.contains(id)) {
        return;
    }

    auto& info = m_shortcuts[id];

    if (info.shortcut) {
        delete info.shortcut;
        info.shortcut = nullptr;
    }

    if (info.isSingleKey) {
        for (auto it = m_singleKeyMap.begin(); it != m_singleKeyMap.end(); ++it) {
            it.value().removeAll(id);
        }
    }

    if (info.isGlobal && m_globalImpl) {
        m_globalImpl->unregisterShortcut(id);
    }

    m_shortcuts.remove(id);
    m_disabledShortcuts.remove(id);
}

bool ShortcutManager::parseKeySequence(const QString& keyStr, ShortcutInfo& info) const
{
    QString normalized = keyStr.trimmed();
    info.isSingleKey = isSingleKey(normalized);

    if (info.isSingleKey) {
        QKeySequence seq(normalized);

        if (!seq.isEmpty()) {
            info.qtKey = static_cast<Qt::Key>(seq[0]);
            info.nativeKey = seq[0];
            info.keySequence = QKeySequence(info.qtKey);
            info.modifiers = Qt::NoModifier;
            return true;
        }

        static QMap<QString, Qt::Key> keyMap = {
            {"Space", Qt::Key_Space},
            {"Esc", Qt::Key_Escape},
            {"Escape", Qt::Key_Escape},
            {"Tab", Qt::Key_Tab},
            {"Backspace", Qt::Key_Backspace},
            {"Enter", Qt::Key_Enter},
            {"Return", Qt::Key_Return},
            {"Delete", Qt::Key_Delete},
            {"Insert", Qt::Key_Insert},
            {"Home", Qt::Key_Home},
            {"End", Qt::Key_End},
            {"PageUp", Qt::Key_PageUp},
            {"PageDown", Qt::Key_PageDown},
            {"Up", Qt::Key_Up},
            {"Down", Qt::Key_Down},
            {"Left", Qt::Key_Left},
            {"Right", Qt::Key_Right},
            {"F1", Qt::Key_F1}, {"F2", Qt::Key_F2}, {"F3", Qt::Key_F3},
            {"F4", Qt::Key_F4}, {"F5", Qt::Key_F5}, {"F6", Qt::Key_F6},
            {"F7", Qt::Key_F7}, {"F8", Qt::Key_F8}, {"F9", Qt::Key_F9},
            {"F10", Qt::Key_F10}, {"F11", Qt::Key_F11}, {"F12", Qt::Key_F12}
        };

        if (keyMap.contains(normalized)) {
            info.qtKey = keyMap[normalized];
            info.nativeKey = QKeySequence(info.qtKey)[0];
            info.keySequence = QKeySequence(info.qtKey);
            info.modifiers = Qt::NoModifier;
            return true;
        }

        if (normalized.length() == 1) {
            QChar c = normalized[0];
            if (c.isLetterOrNumber()) {
                info.qtKey = static_cast<Qt::Key>(c.toUpper().unicode());
                info.nativeKey = QKeySequence(info.qtKey)[0];
                info.keySequence = QKeySequence(info.qtKey);
                info.modifiers = Qt::NoModifier;
                return true;
            }
        }

        return false;
    } else {
        QKeySequence seq(normalized);
        if (seq.isEmpty()) {
            return false;
        }

        info.keySequence = seq;
        info.qtKey = static_cast<Qt::Key>(seq[0]);
        info.nativeKey = seq[0];

        int keyInt = seq[0];
        info.modifiers = static_cast<Qt::KeyboardModifier>(keyInt & Qt::KeyboardModifierMask);
        return true;
    }
}

bool ShortcutManager::isSingleKey(const QString& keyStr) const
{
    QString normalized = keyStr.trimmed();

    if (normalized.contains('+')) {
        return false;
    }

    static QSet<QString> modifiers = {"Ctrl", "Alt", "Shift", "Meta", "Control"};
    if (modifiers.contains(normalized)) {
        return false;
    }

    return true;
}

bool ShortcutManager::detectConflict(const QKeySequence& key, const QString& excludeId) const
{
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        if (it.key() == excludeId) continue;
        if (it.value().keySequence == key) {
            return true;
        }
    }
    return false;
}

void ShortcutManager::setupEventFilter()
{
    if (m_eventFilterInstalled || !m_parentWindow) {
        return;
    }

    m_parentWindow->installEventFilter(this);
    m_eventFilterInstalled = true;
}

bool ShortcutManager::eventFilter(QObject* obj, QEvent* event)
{
    if (!m_allEnabled || !m_parentWindow) {
        return QObject::eventFilter(obj, event);
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
        return QObject::eventFilter(obj, event);
    }

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    int keyCode = keyEvent->key();

    if (!m_singleKeyMap.contains(keyCode)) {
        return QObject::eventFilter(obj, event);
    }

    bool isPress = (event->type() == QEvent::KeyPress);

    for (const QString& id : m_singleKeyMap[keyCode]) {
        if (m_disabledShortcuts.contains(id)) {
            continue;
        }

        const ShortcutInfo& info = m_shortcuts[id];

        if (info.level == ShortcutLevel::Context) {
            if (info.context != m_activeContext) {
                continue;
            }
        }

        if (info.trigger == KeyTrigger::Press && !isPress) continue;
        if (info.trigger == KeyTrigger::Release && isPress) continue;

        if (info.modifiers != Qt::NoModifier) {
            if (keyEvent->modifiers() != info.modifiers) {
                continue;
            }
        }

        if (info.callback && isPress) {
            info.callback();
            emit shortcutTriggered(id);
        }
    }

    return QObject::eventFilter(obj, event);
}

// ============================================================
// GlobalShortcutImpl 实现
// ============================================================

// ----- Windows 实现 -----
#ifdef Q_OS_WIN

ShortcutManager::GlobalShortcutImpl::GlobalShortcutImpl(ShortcutManager* manager)
    : m_manager(manager)
{
    qApp->installNativeEventFilter(this);
}

ShortcutManager::GlobalShortcutImpl::~GlobalShortcutImpl()
{
    qApp->removeNativeEventFilter(this);

    for (int id : m_idByHotkeyId.keys()) {
        UnregisterHotKey(NULL, id);
    }
}

bool ShortcutManager::GlobalShortcutImpl::isSupported() const
{
    return true;
}

bool ShortcutManager::GlobalShortcutImpl::registerShortcut(
    const QString& id, const QKeySequence& key, std::function<void()> callback)
{
    int keyCode = key[0];
    int modifiers = 0;

    if (keyCode & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (keyCode & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
    if (keyCode & Qt::AltModifier) modifiers |= MOD_ALT;
    if (keyCode & Qt::MetaModifier) modifiers |= MOD_WIN;

    int qtKey = keyCode & ~Qt::KeyboardModifierMask;

    int winKey = 0;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        winKey = 'A' + (qtKey - Qt::Key_A);
    } else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        winKey = '0' + (qtKey - Qt::Key_0);
    } else {
        static QMap<int, int> keyMap = {
            {Qt::Key_Space, VK_SPACE},
            {Qt::Key_Escape, VK_ESCAPE},
            {Qt::Key_Tab, VK_TAB},
            {Qt::Key_Backspace, VK_BACK},
            {Qt::Key_Return, VK_RETURN},
            {Qt::Key_Enter, VK_RETURN},
            {Qt::Key_Delete, VK_DELETE},
            {Qt::Key_Insert, VK_INSERT},
            {Qt::Key_Home, VK_HOME},
            {Qt::Key_End, VK_END},
            {Qt::Key_PageUp, VK_PRIOR},
            {Qt::Key_PageDown, VK_NEXT},
            {Qt::Key_Up, VK_UP},
            {Qt::Key_Down, VK_DOWN},
            {Qt::Key_Left, VK_LEFT},
            {Qt::Key_Right, VK_RIGHT},
            {Qt::Key_F1, VK_F1}, {Qt::Key_F2, VK_F2},
            {Qt::Key_F3, VK_F3}, {Qt::Key_F4, VK_F4},
            {Qt::Key_F5, VK_F5}, {Qt::Key_F6, VK_F6},
            {Qt::Key_F7, VK_F7}, {Qt::Key_F8, VK_F8},
            {Qt::Key_F9, VK_F9}, {Qt::Key_F10, VK_F10},
            {Qt::Key_F11, VK_F11}, {Qt::Key_F12, VK_F12},
        };
        winKey = keyMap.value(qtKey, 0);
    }

    if (winKey == 0) {
        qWarning() << "无法注册全局快捷键: 不支持的键" << qtKey;
        return false;
    }

    int hotkeyId = m_nextHotkeyId++;

    if (!RegisterHotKey(NULL, hotkeyId, modifiers, winKey)) {
        qWarning() << "RegisterHotKey 失败:" << GetLastError();
        return false;
    }

    m_callbacks[id] = callback;
    m_hotkeyIds[id] = hotkeyId;
    m_idByHotkeyId[hotkeyId] = id;

    return true;
}

void ShortcutManager::GlobalShortcutImpl::unregisterShortcut(const QString& id)
{
    if (!m_hotkeyIds.contains(id)) {
        return;
    }

    int hotkeyId = m_hotkeyIds[id];
    UnregisterHotKey(NULL, hotkeyId);

    m_callbacks.remove(id);
    m_hotkeyIds.remove(id);
    m_idByHotkeyId.remove(hotkeyId);
}

bool ShortcutManager::GlobalShortcutImpl::nativeEventFilter(
    const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    return winEvent(message);
}

bool ShortcutManager::GlobalShortcutImpl::winEvent(void* message)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);

    if (msg->message == WM_HOTKEY) {
        int hotkeyId = msg->wParam;
        if (m_idByHotkeyId.contains(hotkeyId)) {
            QString id = m_idByHotkeyId[hotkeyId];
            if (m_callbacks.contains(id)) {
                m_callbacks[id]();
                emit m_manager->shortcutTriggered(id);
                return true;
            }
        }
    }
#endif

    return false;
}

#endif // Q_OS_WIN

// ----- Linux 实现（简化版） -----
#ifdef Q_OS_LINUX

ShortcutManager::GlobalShortcutImpl::GlobalShortcutImpl(ShortcutManager* manager)
    : m_manager(manager)
{
    qApp->installNativeEventFilter(this);
}

ShortcutManager::GlobalShortcutImpl::~GlobalShortcutImpl()
{
    qApp->removeNativeEventFilter(this);
}

bool ShortcutManager::GlobalShortcutImpl::isSupported() const
{
    return true;
}

bool ShortcutManager::GlobalShortcutImpl::registerShortcut(
    const QString& id, const QKeySequence& key, std::function<void()> callback)
{
    // 简化实现：只保存回调
    m_callbacks[id] = callback;
    qDebug() << "Linux 全局快捷键注册（简化版）:" << id << key.toString();
    return true;
}

void ShortcutManager::GlobalShortcutImpl::unregisterShortcut(const QString& id)
{
    m_callbacks.remove(id);
}

bool ShortcutManager::GlobalShortcutImpl::nativeEventFilter(
    const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

#endif // Q_OS_LINUX

// ----- macOS 实现（简化版） -----
#ifdef Q_OS_MAC

ShortcutManager::GlobalShortcutImpl::GlobalShortcutImpl(ShortcutManager* manager)
    : m_manager(manager)
{
    qApp->installNativeEventFilter(this);
}

ShortcutManager::GlobalShortcutImpl::~GlobalShortcutImpl()
{
    qApp->removeNativeEventFilter(this);
}

bool ShortcutManager::GlobalShortcutImpl::isSupported() const
{
    return true;
}

bool ShortcutManager::GlobalShortcutImpl::registerShortcut(
    const QString& id, const QKeySequence& key, std::function<void()> callback)
{
    m_callbacks[id] = callback;
    qDebug() << "macOS 全局快捷键注册（简化版）:" << id << key.toString();
    return true;
}

void ShortcutManager::GlobalShortcutImpl::unregisterShortcut(const QString& id)
{
    m_callbacks.remove(id);
}

bool ShortcutManager::GlobalShortcutImpl::nativeEventFilter(
    const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

#endif // Q_OS_MAC

// ----- 不支持的平台 -----
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)

ShortcutManager::GlobalShortcutImpl::GlobalShortcutImpl(ShortcutManager* manager)
    : m_manager(manager)
{
}

ShortcutManager::GlobalShortcutImpl::~GlobalShortcutImpl()
{
}

bool ShortcutManager::GlobalShortcutImpl::isSupported() const
{
    return false;
}

bool ShortcutManager::GlobalShortcutImpl::registerShortcut(
    const QString& id, const QKeySequence& key, std::function<void()> callback)
{
    Q_UNUSED(id);
    Q_UNUSED(key);
    Q_UNUSED(callback);
    return false;
}

void ShortcutManager::GlobalShortcutImpl::unregisterShortcut(const QString& id)
{
    Q_UNUSED(id);
}

bool ShortcutManager::GlobalShortcutImpl::nativeEventFilter(
    const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

#endif

} // namespace Sqz::Utils
