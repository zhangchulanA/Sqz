// ShortcutManager.h
#pragma once

#include <QObject>
#include <QShortcut>
#include <QAction>
#include <QKeySequence>
#include <QKeyEvent>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QAbstractNativeEventFilter>
#include <functional>

#include "Global/SqzGlobal.h"

namespace Sqz {

class SQZ_FRAMEWORK_API ShortcutManager : public QObject
{
    Q_OBJECT

public:
    static ShortcutManager* Instance();

    // ----- 枚举定义 -----
    enum class ShortcutLevel {
        Global,
        Window,
        Context
    };

    enum class KeyTrigger {
        Press,
        Release,
        Both
    };

    // ----- 数据结构 -----
    struct ShortcutInfo {
        QString id;
        QString displayName;
        QKeySequence keySequence;
        int nativeKey = 0;
        Qt::Key qtKey = Qt::Key_unknown;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        ShortcutLevel level = ShortcutLevel::Window;
        QString context;
        KeyTrigger trigger = KeyTrigger::Press;
        std::function<void()> callback;
        std::function<void(bool isPress)> callbackWithState;
        QShortcut* shortcut = nullptr;
        bool enabled = true;
        bool isSingleKey = false;
        bool isGlobal = false;
    };

    // ----- 批量注册 -----
    ShortcutManager& RegisterBatch(
        const QMap<QString, QString>& shortcuts,
        std::function<void(const QString&)> handler);

    ShortcutManager& RegisterBatch(
        const QMap<QString, QPair<QString, QString>>& shortcuts,
        std::function<void(const QString&)> handler);

    bool LoadFromJson(const QString& jsonPath,
                      std::function<void(const QString&)> handler);

    // ----- 单个注册 -----
    ShortcutManager& Register(const QString& id,
                              const QString& keySequence,
                              std::function<void()> callback,
                              const QString& displayName = QString());

    ShortcutManager& Register(const QString& id,
                              const QString& keySequence,
                              QObject* parent,
                              std::function<void()> callback,
                              const QString& displayName = QString());

    ShortcutManager& RegisterContext(const QString& id,
                                     const QString& keySequence,
                                     const QString& context,
                                     std::function<void()> callback,
                                     const QString& displayName = QString());

    ShortcutManager& RegisterGlobal(const QString& id,
                                    const QString& keySequence,
                                    std::function<void()> callback,
                                    const QString& displayName = QString());

    ShortcutManager& TriggerOnRelease(bool release);

    // ----- 查询 -----
    QString GetShortcut(const QString& id) const;
    QKeySequence GetKeySequence(const QString& id) const;
    bool HasShortcut(const QString& id) const;
    bool IsSingleKey(const QString& id) const;
    QList<ShortcutInfo> GetAllShortcuts() const;
    QString MenuText(const QString& id) const;

    // ----- 管理 -----
    ShortcutManager& SetParentWindow(QWidget* parent);
    void SetEnabled(const QString& id, bool enabled);
    void SetAllEnabled(bool enabled);
    void ActivateContext(const QString& context);

    static QString KeyFriendlyName(Qt::Key key);

signals:
    void shortcutTriggered(const QString& id);
    void shortcutChanged(const QString& id, const QString& oldKey, const QString& newKey);

private:
    ShortcutManager(QObject* parent = nullptr);
    ~ShortcutManager();

    bool registerShortcutInternal(const ShortcutInfo& info);
    void registerSingleKeyShortcut(const ShortcutInfo& info);
    void unregisterShortcut(const QString& id);
    bool parseKeySequence(const QString& keyStr, ShortcutInfo& info) const;
    bool isSingleKey(const QString& keyStr) const;
    void setupEventFilter();
    bool detectConflict(const QKeySequence& key, const QString& excludeId) const;
    bool eventFilter(QObject* obj, QEvent* event) override;

    // ============================================================
    // ⭐ 关键修复：GlobalShortcutImpl 完整定义（放在 private 区域）
    // ============================================================
    class GlobalShortcutImpl : public QAbstractNativeEventFilter
    {
    public:
        GlobalShortcutImpl(ShortcutManager* manager);
        ~GlobalShortcutImpl();

        bool registerShortcut(const QString& id, const QKeySequence& key,
                              std::function<void()> callback);
        void unregisterShortcut(const QString& id);
        bool isSupported() const;

        bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;

    private:
#ifdef Q_OS_WIN
        bool winEvent(void* message);
        QMap<int, QString> m_idByHotkeyId;
        int m_nextHotkeyId = 1;
#endif

        ShortcutManager* m_manager;
        QMap<QString, std::function<void()>> m_callbacks;
        QMap<QString, int> m_hotkeyIds;
    };

    // ----- 成员变量 -----
    QMap<QString, ShortcutInfo> m_shortcuts;
    QSet<QString> m_disabledShortcuts;
    QMap<QString, QList<QString>> m_contextShortcuts;
    QMap<int, QList<QString>> m_singleKeyMap;
    QString m_activeContext;
    QWidget* m_parentWindow = nullptr;
    bool m_allEnabled = true;
    bool m_eventFilterInstalled = false;

    // ⭐ 关键修复：使用原始指针，不是 unique_ptr
    GlobalShortcutImpl* m_globalImpl = nullptr;

    static ShortcutManager* s_instance;
};

} // namespace Sqz::Utils
