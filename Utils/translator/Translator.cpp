#include "Translator.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QPointer>
#include <QMetaMethod>
#include "Logger.h"


namespace Sqz {
Translator::Translator(QObject* parent)
    : QObject(parent)
    , m_uiLocked(false)
    , m_lazyLoad(false)      // 默认关闭懒加载，保持原有预加载行为
{
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(200);
    connect(m_debounceTimer, &QTimer::timeout, this, &Translator::onDebounceTriggered);

    connect(this, &Translator::batchRefreshSignal,
            this, &Translator::onBatchRefreshWidgets, Qt::QueuedConnection);

    m_mask = TranslatorMask::instance();
}

Translator& Translator::instance()
{
    static Translator s_instance;
    return s_instance;
}

void Translator::registerLanguage(const QString& langName, const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    if (!langName.isEmpty() && !filePath.isEmpty())
        m_langPath[langName] = filePath;
}

void Translator::preloadAllLanguages()
{
    QMutexLocker locker(&m_mutex);
    if (m_lazyLoad) {
        // 懒加载模式：不实际加载，仅清空缓存，等待按需加载
        m_langCache.clear();
        loginfo << "懒加载模式，预加载仅清空缓存，实际文件将在切换时加载";
        return;
    }
    // 原有行为：立即加载所有语言
    for (auto it = m_langPath.begin(); it != m_langPath.end(); ++it) {
        // 注意：loadLanguageFile 内部会加锁，此处递归锁安全，但为减少锁持有时间，可先复制路径再解锁调用
        // 为了清晰，直接调用（递归锁允许）
        if (!loadLanguageFile(it.key()))
            markLanguageInvalid(it.key());
    }
}

void Translator::setLazyLoad(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_lazyLoad = enable;
    if (enable) {
        // 开启懒加载后清空缓存，释放内存
        m_langCache.clear();
    } else {
        // 关闭懒加载时，立即加载所有已注册语言
        locker.unlock(); // 临时解锁避免死锁，实际递归锁无问题但为了性能
        preloadAllLanguages();
    }
}

void Translator::setDefaultLanguage(const QString& langName)
{
    QMutexLocker locker(&m_mutex);
    m_defaultLang = langName;
    locker.unlock();
    ensureDefaultLanguageValid(); // 确保默认语言有效
}

void Translator::ensureDefaultLanguageValid()
{
    QMutexLocker locker(&m_mutex);
    if (m_defaultLang.isEmpty())
        return;
    // 若默认语言自身被禁用或无效，则从有效语言列表中选择第一个作为新的默认语言
    if (m_disabledLangs.contains(m_defaultLang) || m_invalidLangs.contains(m_defaultLang)) {
        QStringList validList = languageList(); // 注意 languageList 内部会加锁，递归安全
        if (!validList.isEmpty()) {
            QString oldDefault = m_defaultLang;
            m_defaultLang = validList.first();
            loginfo << "默认语言" << oldDefault << "无效，已自动切换为" << m_defaultLang;
        } else {
            loginfo << "警告：没有任何有效语言，默认语言将为空";
        }
    }
}

void Translator::disableLanguage(const QString& langName)
{
    QMutexLocker locker(&m_mutex);
    if (m_langPath.contains(langName)) {
        m_disabledLangs.insert(langName);
        loginfo << "手动禁用语种：" << langName;
        // 若当前语言被禁用，则切换到默认语言
        if (m_currentLang == langName) {
            locker.unlock();
            safeSwitchLanguage(m_defaultLang);
        }
    }
}

void Translator::enableLanguage(const QString& langName)
{
    QMutexLocker locker(&m_mutex);
    m_disabledLangs.remove(langName);
    ensureDefaultLanguageValid(); // 可能默认语言被重新启用
}

bool Translator::isLanguageDisabled(const QString& langName) const
{
    QMutexLocker locker(&m_mutex);
    return m_disabledLangs.contains(langName);
}

void Translator::safeSwitchLanguage(const QString& langName)
{
    if (m_isSwitching) return;

    // 先检查该语言是否可用（注册、未禁用、未损坏）
    {
        QMutexLocker locker(&m_mutex);
        if (!m_langPath.contains(langName) || m_disabledLangs.contains(langName) || m_invalidLangs.contains(langName)) {
            // 无效语言，切换到默认语言（再次校验默认有效性）
            QString target = m_defaultLang;
            locker.unlock();
            ensureDefaultLanguageValid();
            QMutexLocker locker2(&m_mutex);
            if (m_defaultLang.isEmpty() || (!m_langPath.contains(m_defaultLang)) ||
                m_disabledLangs.contains(m_defaultLang) || m_invalidLangs.contains(m_defaultLang)) {
                // 默认语言也无效，尝试从有效列表中取第一个
                QStringList valid = languageList();
                target = valid.isEmpty() ? QString() : valid.first();
            }
            if (!target.isEmpty() && target != m_currentLang) {
                locker2.unlock();
                safeSwitchLanguage(target);
            }
            return;
        }
        m_pendingLang = langName;
    }
    m_debounceTimer->start();
}

void Translator::onDebounceTriggered()
{
    m_isSwitching = true;
    lockUI();
    m_mask->showMask();

    // 切换前确保待切换语言已加载（懒加载机制）
    if (!ensureLanguageLoaded(m_pendingLang)) {
        // 加载失败，回退到默认语言
        loginfo << "加载语言失败：" << m_pendingLang << "，回退默认";
        if (!ensureLanguageLoaded(m_defaultLang)) {
            loginfo << "默认语言也加载失败，可能无任何可用翻译";
        }
    }

    QMutexLocker locker(&m_mutex);
    if (m_langCache.contains(m_pendingLang) && !m_disabledLangs.contains(m_pendingLang) && !m_invalidLangs.contains(m_pendingLang))
        m_currentLang = m_pendingLang;
    else
        m_currentLang = m_defaultLang;
    locker.unlock();

    emit batchRefreshSignal();
}

bool Translator::ensureLanguageLoaded(const QString& langName) const
{
    QMutexLocker locker(&m_mutex);
    if (m_langCache.contains(langName))
        return true;
    if (!m_langPath.contains(langName) || m_disabledLangs.contains(langName))
        return false;
    // 递归锁允许，但为了清晰，先解锁再调用非 const 函数
    locker.unlock();
    // 注意：loadLanguageFile 是非 const 成员，此处 const_cast 不优雅，但为了保持接口 const 正确
    // 实际可以修改 translate 不声明为 const，但为了兼容现有代码，使用 mutable 成员或强制转换
    // 由于 m_langCache 是 mutable，但 loadLanguageFile 修改其他成员，因此需要强制转换
    return const_cast<Translator*>(this)->loadLanguageFile(langName);
}

void Translator::onBatchRefreshWidgets()
{
    // 遍历窗口列表时加锁复制，避免迭代器失效
    QList<QWidget*> widgets;
    {
        QMutexLocker locker(&m_mutex);
        widgets = m_widgetList;
    }
    for (QWidget* widget : widgets) {
        if (widget && widget->isVisible()) {
            // 使用 DirectConnection 保证立即刷新
            QMetaObject::invokeMethod(widget, "M_InitTrUi", Qt::DirectConnection);
        }
    }

    m_mask->hideMask();
    unlockUI();
    m_isSwitching = false;
}

void Translator::registerWidget(QWidget* widget)
{
    if (!widget) return;
    QMutexLocker locker(&m_mutex);
    if (!m_widgetList.contains(widget)) {
        m_widgetList.append(widget);
        // 自动注销：当 widget 销毁时，从列表中移除
        // 使用 QPointer 确保安全，但信号中直接捕获 widget 指针，注意 lambda 生命周期
        // 为避免重复连接，使用 UniqueConnection
        static int metaType = qRegisterMetaType<QWidget*>("QWidget*");
        connect(widget, &QObject::destroyed, this, [this, widget](QObject*) {
            QMutexLocker l(&m_mutex);
            m_widgetList.removeAll(widget);
        }, Qt::UniqueConnection);
    }
}

void Translator::unregisterWidget(QWidget* widget)
{
    if (!widget) return;
    QMutexLocker locker(&m_mutex);
    m_widgetList.removeAll(widget);
    // 断开自动注销连接（可选，因为对象销毁时会自动断开）
    disconnect(widget, &QObject::destroyed, this, nullptr);
}

void Translator::lockUI()
{
    if (!m_uiLocked) {
        m_uiLocked = true;
        qApp->setOverrideCursor(Qt::WaitCursor);
    }
}

void Translator::unlockUI()
{
    if (m_uiLocked) {
        m_uiLocked = false;
        qApp->restoreOverrideCursor();
    }
}

QString Translator::translate(const QString& key) const
{
    if (key.isEmpty()) return key;
    QMutexLocker locker(&m_mutex);
    QString cur = m_currentLang;
    if (cur.isEmpty()) {
        // 无当前语言，尝试默认
        cur = m_defaultLang;
        if (cur.isEmpty()) return key;
    }
    // 懒加载：若当前语言缓存不存在，尝试加载
    if (!m_langCache.contains(cur)) {
        locker.unlock();
        if (!ensureLanguageLoaded(cur)) {
            return key;
        }
        locker.relock();
    }
    auto it = m_langCache.find(cur);
    if (it != m_langCache.end()) {
        const auto& dict = it.value();
        if (dict.contains(key))
            return dict[key];
    }
    return key;
}

QStringList Translator::languageList() const
{
    QMutexLocker locker(&m_mutex);
    QStringList list = m_langPath.keys();
    list.erase(std::remove_if(list.begin(), list.end(),
        [this](const QString& l) {
            return m_disabledLangs.contains(l) || m_invalidLangs.contains(l);
        }), list.end());
    return list;
}

QString Translator::currentLanguage() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentLang;
}

// ========== 私有文件加载与校验（优化：合并JSON解析、减小锁粒度） ==========

bool Translator::loadLanguageFile(const QString& langName)
{
    QString filePath;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_langPath.contains(langName)) {
            markLanguageInvalid(langName);
            return false;
        }
        filePath = m_langPath[langName];
    }

    // 1. 文件完整性校验（不持锁）
    QFileInfo info(filePath);
    if (!info.exists() || info.size() < 10) {
        markLanguageInvalid(langName);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        markLanguageInvalid(langName);
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    // 2. UTF-8 校验
    if (!isUtf8Encoded(data)) {
        markLanguageInvalid(langName);
        return false;
    }

    // 3. 一次性解析 JSON 并校验（取代单独的 checkJsonValidity）
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || doc.isNull() || !doc.isObject()) {
        markLanguageInvalid(langName);
        return false;
    }
    QJsonObject rootObj = doc.object();
    if (rootObj.isEmpty()) {
        markLanguageInvalid(langName);
        return false;
    }

    // 4. 解析翻译表（压平处理）
    QMap<QString, QString> transMap;
    for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
        QJsonValue val = it.value();
        if (val.isObject()) {
            QJsonObject subObj = val.toObject();
            for (auto subIt = subObj.begin(); subIt != subObj.end(); ++subIt) {
                transMap[subIt.key()] = subIt.value().toString();
            }
        } else {
            // 兼容顶层直接键值对
            transMap[it.key()] = val.toString();
        }
    }
    if (transMap.isEmpty()) {
        markLanguageInvalid(langName);
        return false;
    }

    // 5. 写入缓存（加锁保护）
    {
        QMutexLocker locker(&m_mutex);
        m_langCache[langName] = transMap;
        // 如果该语言之前被标记无效，现在成功加载，移除无效标记
        m_invalidLangs.remove(langName);
    }
    return true;
}

void Translator::markLanguageInvalid(const QString& langName)
{
    QMutexLocker locker(&m_mutex);
    m_invalidLangs.insert(langName);
    m_langCache.remove(langName);
}

bool Translator::isUtf8Encoded(const QByteArray& data)
{
    int len = data.size(), i = 0;
    while (i < len) {
        if ((data[i] & 0x80) == 0x00) i++;
        else if ((data[i] & 0xE0) == 0xC0) {
            if (i+1>=len || (data[i+1]&0xC0)!=0x80) return false;
            i+=2;
        } else if ((data[i] & 0xF0) == 0xE0) {
            if (i+2>=len || (data[i+1]&0xC0)!=0x80 || (data[i+2]&0xC0)!=0x80) return false;
            i+=3;
        } else if ((data[i] & 0xF8) == 0xF0) {
            if (i+3>=len) return false;
            i+=4;
        } else return false;
    }
    return true;
}

bool Translator::checkJsonValidity(const QByteArray& fileData)
{
    // 此函数已不再单独使用，保留为了兼容性，直接调用解析逻辑
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &err);
    return (err.error == QJsonParseError::NoError && doc.isObject() && !doc.object().isEmpty());
}
}
