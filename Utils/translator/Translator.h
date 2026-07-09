#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <QSet>
#include <QPointer>
#include <algorithm>
#include "TranslatorMask.h"
#include "SqzGlobal.h"


namespace Sqz::Utils {
/**
 * @brief 工业级翻译管理单例（优化版：多屏、线程安全、懒加载、锁粒度细、自动注销）
 */
class SQZ_FRAMEWORK_API Translator : public QObject
{
    Q_OBJECT
public:
    static Translator& instance();

    Translator(const Translator&) = delete;
    Translator& operator=(const Translator&) = delete;

    // ====================== 基础配置接口 ======================
    void registerLanguage(const QString& langName, const QString& filePath);
    void preloadAllLanguages();                // 预加载所有语言（若开启懒加载则仅标记）
    void setDefaultLanguage(const QString& langName);
    void setLazyLoad(bool enable);             // 新增：懒加载开关（默认 false 兼容旧行为）

    // ====================== 语种手动禁用/启用 ======================
    void disableLanguage(const QString& langName);
    void enableLanguage(const QString& langName);
    bool isLanguageDisabled(const QString& langName) const;

    // ====================== 安全语言切换（防抖+遮罩） ======================
    Q_INVOKABLE void safeSwitchLanguage(const QString& langName);

    // ====================== 窗口自动注册（全同步核心） ======================
    void registerWidget(QWidget* widget);
    void unregisterWidget(QWidget* widget);

    // ====================== 查询接口 ======================
    QStringList languageList() const;
    QString currentLanguage() const;
    QString translate(const QString& key) const;

signals:
    void batchRefreshSignal();

private slots:
    void onDebounceTriggered();
    void onBatchRefreshWidgets();

private:
    explicit Translator(QObject* parent = nullptr);
    ~Translator() override = default;

    // 文件与翻译加载
    bool loadLanguageFile(const QString& langName);
    void markLanguageInvalid(const QString& langName);
    bool ensureLanguageLoaded(const QString& langName) const;  // 懒加载：确保语言已载入缓存

    // UI 辅助
    void lockUI();
    void unlockUI();

    // 默认语言有效性保证
    void ensureDefaultLanguageValid();

    // 工具函数
    static bool isUtf8Encoded(const QByteArray& data);
    static bool checkJsonValidity(const QByteArray& fileData);

private:
    mutable QMutex  m_mutex{QMutex::Recursive};
    std::atomic<bool> m_isSwitching{false};
    QTimer*          m_debounceTimer;
    QString          m_pendingLang;
    QString          m_defaultLang;
    QString          m_currentLang;

    QMap<QString, QString>   m_langPath;               // 语言名 → 文件路径
    mutable QMap<QString, QMap<QString, QString>> m_langCache; // 翻译缓存（mutable 允许懒加载时修改）
    QSet<QString>            m_invalidLangs;           // 自动禁用（文件损坏）
    QSet<QString>            m_disabledLangs;          // 手动禁用

    QList<QWidget*>          m_widgetList;             // 注册窗口原始指针（自动注销时移除）
    bool                     m_uiLocked;
    TranslatorMask*       m_mask;

    bool                     m_lazyLoad;               // 懒加载标志（默认 false）
};

// 全局翻译宏
#define TR(key) Translator::instance().translate(key)
#define MyTranslator Translator::instance()
}
#endif // TRANSLATOR_H
