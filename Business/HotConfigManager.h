/**
 * @file HotConfigManager.h
 * @brief 配置热加载管理器 - 支持配置文件变更自动重载
 *
 * 核心功能：
 * 1. 监控配置文件变化
 * 2. 自动重新加载配置
 * 3. 配置校验和安全回退
 * 4. 支持多种格式（JSON、INI、XML）
 * 5. 配置变更通知
 * 6. 版本管理
 *
 * 使用场景：
 * 1. 运营活动配置：实时调整折扣、活动时间
 * 2. 功能开关：动态开启/关闭功能
 * 3. 系统参数调优：调整连接池大小、缓存大小
 * 4. 日志级别动态调整：线上问题时开启详细日志
 * 5. 第三方服务切换：API地址变更
 * 6. A/B测试：实时切换算法版本
 * 7. 限流配置：动态调整限流阈值
 */

#ifndef HOTCONFIGMANAGER_H
#define HOTCONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QMap>
#include <QFile>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <functional>
#include <memory>
#include <atomic>
#include <QFileInfo>

#include <QCryptographicHash>
namespace Sqz {
/**
 * @brief 配置变更类型
 */
enum class ConfigChangeType {
    Added,      // 新增
    Modified,   // 修改
    Deleted,    // 删除
    Reloaded    // 重新加载
};

/**
 * @brief 配置信息结构体
 */
struct ConfigInfo {
    QString path;               // 文件路径
    QString format;             // 格式（json/ini/xml）
    QDateTime loadTime;         // 加载时间
    QDateTime lastModified;     // 最后修改时间
    QString version;            // 版本号
    QString hash;               // 文件哈希（用于检测变化）
    bool isValid;               // 是否有效
    QString errorMessage;       // 错误信息
    int loadCount;              // 加载次数
    QVariantMap metadata;       // 元数据
};

/**
 * @brief 配置变更事件
 */
struct ConfigChangeEvent {
    QString configName;         // 配置名称
    ConfigChangeType type;      // 变更类型
    QVariantMap oldValue;       // 旧值
    QVariantMap newValue;       // 新值
    QDateTime timestamp;        // 时间戳
    QString changedBy;          // 变更者（用于审计）
};

/**
 * @brief 配置热加载管理器
 */
class HotConfigManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 配置加载器函数类型
     * @param path 配置文件路径
     * @return 配置数据（QVariantMap）
     */
    using ConfigLoader = std::function<QVariantMap(const QString& path)>;

    /**
     * @brief 配置校验器函数类型
     * @param config 配置数据
     * @param error 错误信息（输出）
     * @return true表示校验通过
     */
    using ConfigValidator = std::function<bool(const QVariantMap& config, QString& error)>;

    /**
     * @brief 配置变更回调函数类型
     * @param config 新配置数据
     */
    using ConfigChangeCallback = std::function<void(const QVariantMap& config)>;

    /**
     * @brief 单例模式获取实例
     */
    static HotConfigManager* instance() {
        static HotConfigManager manager;
        return &manager;
    }

    /**
     * @brief 构造函数（私有，单例模式）
     */
    explicit HotConfigManager(QObject* parent = nullptr)
        : QObject(parent),
          m_isInitialized(false),
          m_autoReload(true),
          m_checkInterval(5000) {  // 5秒检查一次
        // 启动文件监控
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged,
                this, &HotConfigManager::onFileChanged);

        // 启动定时检查（作为文件监控的备份）
        m_checkTimer.setInterval(m_checkInterval);
        connect(&m_checkTimer, &QTimer::timeout,
                this, &HotConfigManager::checkAllConfigs);
        m_checkTimer.start();

        qDebug() << "[HotConfigManager] 配置管理器已启动";
    }

    /**
     * @brief 析构函数
     */
    ~HotConfigManager() {
        qDebug() << "[HotConfigManager] 配置管理器已关闭";
    }

    // ========== 配置加载接口 ==========

    /**
     * @brief 注册JSON配置
     * @param name 配置名称（唯一标识）
     * @param path 文件路径
     * @param validator 校验器（可选）
     * @param callback 变更回调（可选）
     * @return true表示注册成功
     *
     * 特点：
     * 1. 自动加载JSON文件
     * 2. 文件变化自动重新加载
     * 3. 校验失败自动回退
     */
    bool registerJsonConfig(const QString& name, const QString& path,
                           ConfigValidator validator = nullptr,
                           ConfigChangeCallback callback = nullptr) {
        return registerConfig(name, path, "json",
            [](const QString& p) -> QVariantMap {
                return loadJsonConfig(p);
            },
            validator, callback);
    }

    /**
     * @brief 注册INI配置
     * @param name 配置名称（唯一标识）
     * @param path 文件路径
     * @param validator 校验器（可选）
     * @param callback 变更回调（可选）
     * @return true表示注册成功
     */
    bool registerIniConfig(const QString& name, const QString& path,
                          ConfigValidator validator = nullptr,
                          ConfigChangeCallback callback = nullptr) {
        return registerConfig(name, path, "ini",
            [](const QString& p) -> QVariantMap {
                return loadIniConfig(p);
            },
            validator, callback);
    }

    /**
     * @brief 注册自定义配置
     * @param name 配置名称
     * @param path 文件路径
     * @param format 格式名称（用于显示）
     * @param loader 自定义加载器
     * @param validator 校验器（可选）
     * @param callback 变更回调（可选）
     * @return true表示注册成功
     */
    bool registerConfig(const QString& name, const QString& path,
                        const QString& format, ConfigLoader loader,
                        ConfigValidator validator = nullptr,
                        ConfigChangeCallback callback = nullptr) {
        if (m_configs.contains(name)) {
            qWarning() << "[HotConfigManager] 配置已存在:" << name;
            return false;
        }

        // 检查文件是否存在
        if (!QFile::exists(path)) {
            qWarning() << "[HotConfigManager] 配置文件不存在:" << path;
            return false;
        }

        ConfigEntry entry;
        entry.name = name;
        entry.path = path;
        entry.format = format;
        entry.loader = loader;
        entry.validator = validator;
        entry.callback = callback;
        entry.isLoaded = false;
        entry.info.path = path;
        entry.info.format = format;

        // 加载配置
        if (!loadConfig(entry)) {
            qWarning() << "[HotConfigManager] 配置加载失败:" << name;
            return false;
        }

        // 添加到监控
        m_watcher->addPath(path);

        m_configs[name] = entry;
        m_configPaths[path] = name;

        qDebug() << "[HotConfigManager] 注册配置:" << name << "路径:" << path;

        return true;
    }

    /**
     * @brief 获取配置数据
     * @param name 配置名称
     * @return 配置数据（如果不存在返回空Map）
     */
    QVariantMap getConfig(const QString& name) const {
        if (!m_configs.contains(name)) {
            qWarning() << "[HotConfigManager] 配置不存在:" << name;
            return QVariantMap();
        }

        QMutexLocker locker(&m_mutex);
        const auto& entry = m_configs[name];
        return entry.currentConfig;
    }

    /**
     * @brief 获取配置值
     * @param name 配置名称
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    QVariant getConfigValue(const QString& name, const QString& key,
                           const QVariant& defaultValue = QVariant()) {
        QVariantMap config = getConfig(name);
        if (config.contains(key)) {
            return config[key];
        }
        return defaultValue;
    }

    /**
     * @brief 获取配置信息
     */
    ConfigInfo getConfigInfo(const QString& name) const {
        if (!m_configs.contains(name)) {
            return ConfigInfo();
        }
        return m_configs[name].info;
    }

    /**
     * @brief 获取所有配置名称
     */
    QStringList getConfigNames() const {
        return m_configs.keys();
    }

    /**
     * @brief 检查配置是否存在
     */
    bool hasConfig(const QString& name) const {
        return m_configs.contains(name);
    }

    // ========== 配置管理接口 ==========

    /**
     * @brief 重新加载指定配置
     * @param name 配置名称
     * @return true表示重新加载成功
     */
    bool reloadConfig(const QString& name) {
        if (!m_configs.contains(name)) {
            qWarning() << "[HotConfigManager] 配置不存在:" << name;
            return false;
        }

        qDebug() << "[HotConfigManager] 手动重新加载配置:" << name;

        ConfigEntry& entry = m_configs[name];
        return loadConfig(entry);
    }

    /**
     * @brief 重新加载所有配置
     */
    void reloadAllConfigs() {
        qDebug() << "[HotConfigManager] 重新加载所有配置";

        for (auto& entry : m_configs) {
            loadConfig(entry);
        }
    }

    /**
     * @brief 设置自动重载开关
     */
    void setAutoReload(bool enable) {
        m_autoReload = enable;
        qDebug() << "[HotConfigManager] 自动重载:" << (enable ? "开启" : "关闭");
    }

    /**
     * @brief 设置检查间隔
     */
    void setCheckInterval(int ms) {
        m_checkInterval = ms;
        m_checkTimer.setInterval(ms);
        qDebug() << "[HotConfigManager] 检查间隔:" << ms << "ms";
    }

    /**
     * @brief 获取配置变更历史
     * @param name 配置名称
     * @param count 获取最近N条
     */
    QList<ConfigChangeEvent> getChangeHistory(const QString& name, int count = 100) {
        if (!m_configs.contains(name)) {
            return QList<ConfigChangeEvent>();
        }

        const auto& entry = m_configs[name];
        QList<ConfigChangeEvent> history = entry.changeHistory;
        if (history.size() > count) {
            return history.mid(history.size() - count);
        }
        return history;
    }

    /**
     * @brief 清空变更历史
     */
    void clearChangeHistory(const QString& name) {
        if (m_configs.contains(name)) {
            m_configs[name].changeHistory.clear();
        }
    }

    // ========== 统计信息 ==========

    struct Statistics {
        int totalConfigs;           // 总配置数
        int loadedConfigs;          // 已加载配置数
        int failedConfigs;          // 加载失败配置数
        qint64 totalLoads;          // 总加载次数
        qint64 totalChanges;        // 总变更次数
        qint64 totalErrors;         // 总错误次数
        QHash<QString, int> loadCounts;  // 各配置加载次数
    };

    Statistics getStatistics() const {
        Statistics stats{0, 0, 0, 0, 0, 0};

        for (const auto& entry : m_configs) {
            stats.totalConfigs++;
            if (entry.isLoaded) {
                stats.loadedConfigs++;
            } else {
                stats.failedConfigs++;
            }
            stats.totalLoads += entry.info.loadCount;
            stats.totalChanges += entry.changeHistory.size();
            stats.loadCounts[entry.name] = entry.info.loadCount;
        }

        return stats;
    }

    // ========== 导出/备份 ==========

    /**
     * @brief 导出当前配置
     * @param name 配置名称
     * @param outputPath 输出路径
     * @return true表示成功
     */
    bool exportConfig(const QString& name, const QString& outputPath) {
        if (!m_configs.contains(name)) {
            return false;
        }

        QVariantMap config = getConfig(name);
        QJsonDocument doc = QJsonDocument::fromVariant(config);

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }

        file.write(doc.toJson());
        file.close();

        qDebug() << "[HotConfigManager] 导出配置:" << name << "->" << outputPath;
        return true;
    }

    /**
     * @brief 备份当前配置
     * @param name 配置名称
     * @return 备份文件路径
     */
    QString backupConfig(const QString& name) {
        if (!m_configs.contains(name)) {
            return "";
        }

        const auto& entry = m_configs[name];
        QString backupPath = entry.path + ".backup." +
                            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

        if (exportConfig(name, backupPath)) {
            qDebug() << "[HotConfigManager] 备份配置:" << name << "->" << backupPath;
            return backupPath;
        }

        return "";
    }

    // ========== 信号 ==========
signals:
    /**
     * @brief 配置加载信号
     * @param name 配置名称
     * @param success 是否成功
     */
    void configLoaded(const QString& name, bool success);

    /**
     * @brief 配置变更信号
     * @param name 配置名称
     * @param oldConfig 旧配置
     * @param newConfig 新配置
     */
    void configChanged(const QString& name,
                       const QVariantMap& oldConfig,
                       const QVariantMap& newConfig);

    /**
     * @brief 配置验证失败信号
     * @param name 配置名称
     * @param error 错误信息
     */
    void configValidationFailed(const QString& name, const QString& error);

    /**
     * @brief 配置加载失败信号
     * @param name 配置名称
     * @param error 错误信息
     */
    void configLoadFailed(const QString& name, const QString& error);

private:
    /**
     * @brief 配置条目结构体
     */
    struct ConfigEntry {
        QString name;
        QString path;
        QString format;
        ConfigLoader loader;
        ConfigValidator validator;
        ConfigChangeCallback callback;
        QVariantMap currentConfig;
        QVariantMap lastConfig;
        ConfigInfo info;
        bool isLoaded;
        QList<ConfigChangeEvent> changeHistory;

        ConfigEntry() : isLoaded(false) {}
    };

    /**
     * @brief 加载JSON配置
     */
    static QVariantMap loadJsonConfig(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[HotConfigManager] 打开JSON文件失败:" << path;
            return QVariantMap();
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "[HotConfigManager] 无效的JSON格式:" << path;
            return QVariantMap();
        }

        return doc.object().toVariantMap();
    }

    /**
     * @brief 加载INI配置
     */
    static QVariantMap loadIniConfig(const QString& path) {
        QSettings settings(path, QSettings::IniFormat);
        QVariantMap result;

        // 读取所有键
        QStringList keys = settings.allKeys();
        for (const auto& key : keys) {
            result[key] = settings.value(key);
        }

        return result;
    }

    /**
     * @brief 加载配置
     */
    bool loadConfig(ConfigEntry& entry) {
        QMutexLocker locker(&m_mutex);

        qDebug() << "[HotConfigManager] 加载配置:" << entry.name
                 << "路径:" << entry.path;

        // 读取文件修改时间
        QFileInfo fileInfo(entry.path);
        if (!fileInfo.exists()) {
            qWarning() << "[HotConfigManager] 配置文件不存在:" << entry.path;
            entry.info.isValid = false;
            entry.info.errorMessage = "文件不存在";
            entry.isLoaded = false;
            emit configLoadFailed(entry.name, "文件不存在");
            return false;
        }

        QDateTime lastModified = fileInfo.lastModified();
        QByteArray hash = calculateFileHash(entry.path);

        // 检查是否需要重新加载
        if (entry.isLoaded && entry.info.lastModified == lastModified &&
            entry.info.hash == hash) {
            qDebug() << "[HotConfigManager] 文件未变化，跳过加载:" << entry.name;
            return true;
        }

        // 加载新配置
        QVariantMap newConfig;
        QString errorMessage;

        try {
            newConfig = entry.loader(entry.path);
        } catch (const std::exception& e) {
            errorMessage = e.what();
            qCritical() << "[HotConfigManager] 加载异常:" << errorMessage;
        } catch (...) {
            errorMessage = "未知异常";
            qCritical() << "[HotConfigManager] 加载未知异常";
        }

        if (newConfig.isEmpty()) {
            qWarning() << "[HotConfigManager] 配置加载结果为空:" << entry.name;
            entry.info.isValid = false;
            entry.info.errorMessage = "配置为空";
            entry.isLoaded = false;
            emit configLoadFailed(entry.name, "配置为空");
            return false;
        }

        // 验证配置
        if (entry.validator) {
            QString validateError;
            if (!entry.validator(newConfig, validateError)) {
                qWarning() << "[HotConfigManager] 配置验证失败:" << entry.name
                           << "错误:" << validateError;
                entry.info.isValid = false;
                entry.info.errorMessage = validateError;
                entry.isLoaded = false;
                emit configValidationFailed(entry.name, validateError);
                return false;
            }
        }

        // 保存旧配置
        QVariantMap oldConfig = entry.currentConfig;

        // 更新配置
        entry.lastConfig = entry.currentConfig;
        entry.currentConfig = newConfig;
        entry.info.loadTime = QDateTime::currentDateTime();
        entry.info.lastModified = lastModified;
        entry.info.hash = hash;
        entry.info.isValid = true;
        entry.info.errorMessage = "";
        entry.info.loadCount++;
        entry.isLoaded = true;

        // 记录变更历史
        if (!oldConfig.isEmpty()) {
            ConfigChangeEvent event;
            event.configName = entry.name;
            event.type = ConfigChangeType::Modified;
            event.oldValue = oldConfig;
            event.newValue = newConfig;
            event.timestamp = QDateTime::currentDateTime();
            entry.changeHistory.append(event);

            // 保留最近100条记录
            if (entry.changeHistory.size() > 100) {
                entry.changeHistory.removeFirst();
            }
        }

        qDebug() << "[HotConfigManager] 配置加载成功:" << entry.name
                 << "版本:" << entry.info.version
                 << "加载次数:" << entry.info.loadCount;

        // 触发回调
        if (entry.callback) {
            try {
                entry.callback(newConfig);
            } catch (const std::exception& e) {
                qCritical() << "[HotConfigManager] 回调执行失败:" << e.what();
            }
        }

        // 发送信号
        emit configLoaded(entry.name, true);
        if (!oldConfig.isEmpty()) {
            emit configChanged(entry.name, oldConfig, newConfig);
        }

        return true;
    }

    /**
     * @brief 文件变化回调
     */
    void onFileChanged(const QString& path) {
        qDebug() << "[HotConfigManager] 文件变化:" << path;

        if (!m_configPaths.contains(path)) {
            qDebug() << "[HotConfigManager] 未注册的配置文件:" << path;
            return;
        }

        QString name = m_configPaths[path];

        if (!m_autoReload) {
            qDebug() << "[HotConfigManager] 自动重载已关闭，忽略变化";
            return;
        }

        // 延迟重载（避免文件还在写入）
        QTimer::singleShot(500, [this, name]() {
            reloadConfig(name);
        });
    }

    /**
     * @brief 定时检查所有配置
     */
    void checkAllConfigs() {
        if (!m_autoReload) {
            return;
        }

        for (auto& entry : m_configs) {
            // 检查文件是否变化
            QFileInfo fileInfo(entry.path);
            if (!fileInfo.exists()) {
                continue;
            }

            QDateTime lastModified = fileInfo.lastModified();
            if (lastModified != entry.info.lastModified) {
                qDebug() << "[HotConfigManager] 检测到配置变化:" << entry.name;
                reloadConfig(entry.name);
            }
        }
    }

    /**
     * @brief 计算文件哈希
     */
    QByteArray calculateFileHash(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }

        QByteArray data = file.readAll();
        file.close();

        return QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();
    }

private:
    bool m_isInitialized;
    bool m_autoReload;
    int m_checkInterval;

    mutable QMutex m_mutex;
    QHash<QString, ConfigEntry> m_configs;          // 配置名称 -> 配置条目
    QHash<QString, QString> m_configPaths;          // 文件路径 -> 配置名称

    QFileSystemWatcher* m_watcher;                  // 文件监控器
    QTimer m_checkTimer;                            // 定时检查器
};
}
#endif // HOTCONFIGMANAGER_H
