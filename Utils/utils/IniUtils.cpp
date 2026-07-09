#include "IniUtils.h"
#include <QTextCodec>
#include <QFile>
#include <QDir>
#include <QDebug>

// ==============================================
// 构造与析构
// ==============================================

namespace Sqz::Utils {
IniUtils::IniUtils(const QString& filePath, bool autoSync)
    : m_filePath(filePath)
    , m_autoSync(autoSync)
{
    // 创建 QSettings 对象，指定 INI 格式
    m_pSettings = new QSettings(filePath, QSettings::IniFormat);

    // 设置 UTF-8 编码，确保中文等非 ASCII 字符正确读写
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    if (codec) {
        m_pSettings->setIniCodec(codec);
    } else {
        qWarning() << "IniUtils: Failed to get UTF-8 codec, using system default.";
    }
}

IniUtils::~IniUtils()
{
    // 析构前同步，确保所有未写入的数据保存到磁盘
    {
        QMutexLocker locker(&m_mutex);
        doSyncToFile();
    }
    delete m_pSettings;
}

// ==============================================
// 基础文件操作
// ==============================================

QString IniUtils::getFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

bool IniUtils::isFileExist() const
{
    QMutexLocker locker(&m_mutex);
    return QFile::exists(m_filePath);
}

void IniUtils::reloadFile()
{
    QMutexLocker locker(&m_mutex);

    // 暂存旧的自动同步标志
    bool oldAutoSync = m_autoSync;

    // 重建 QSettings 对象以重新从磁盘加载文件
    delete m_pSettings;
    m_pSettings = new QSettings(m_filePath, QSettings::IniFormat);

    // 重新设置 UTF-8 编码
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    if (codec) {
        m_pSettings->setIniCodec(codec);
    }

    // 恢复自动同步标志
    m_autoSync = oldAutoSync;
}

void IniUtils::syncToFile()
{
    QMutexLocker locker(&m_mutex);
    doSyncToFile();
}

void IniUtils::flush()
{
    syncToFile();   // 与 syncToFile 行为完全一致
}

void IniUtils::doSyncToFile()
{
    if (m_pSettings) {
        m_pSettings->sync();
    }
}

bool IniUtils::backupFile(const QString& backupPath)
{
    // 先同步内存数据到磁盘
    {
        QMutexLocker locker(&m_mutex);
        if (!isFileExist()) {
            return false;
        }
        doSyncToFile();
    }
    // 执行文件拷贝
    return QFile::copy(m_filePath, backupPath);
}

void IniUtils::setAutoSync(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_autoSync = enabled;
}

bool IniUtils::autoSync() const
{
    QMutexLocker locker(&m_mutex);
    return m_autoSync;
}

// ==============================================
// 内部无锁实现
// ==============================================

void IniUtils::doWriteString(const QString& group, const QString& key, const QString& value)
{
    // 直接写入 QSettings（组/键 拼接）
    m_pSettings->setValue(QString("%1/%2").arg(group, key), value);
    if (m_autoSync) {
        doSyncToFile();
    }
}

// ==============================================
// 基础写入（公开接口，加锁）
// ==============================================

void IniUtils::writeString(const QString& group, const QString& key, const QString& value)
{
    QMutexLocker locker(&m_mutex);
    doWriteString(group, key, value);
}

void IniUtils::writeInt(const QString& group, const QString& key, int value)
{
    writeString(group, key, QString::number(value));
}

void IniUtils::writeDouble(const QString& group, const QString& key, double value, int precision)
{
    if (precision < 0) precision = 0;
    if (precision > 15) precision = 15;
    writeString(group, key, QString::number(value, 'f', precision));
}

void IniUtils::writeBool(const QString& group, const QString& key, bool value)
{
    writeString(group, key, value ? "true" : "false");
}

void IniUtils::writeByteArray(const QString& group, const QString& key, const QByteArray& value)
{
    QMutexLocker locker(&m_mutex);
    m_pSettings->setValue(QString("%1/%2").arg(group, key), value);
    if (m_autoSync) {
        doSyncToFile();
    }
}

// ==============================================
// 基础读取
// ==============================================

QString IniUtils::readString(const QString& group, const QString& key, const QString& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    return m_pSettings->value(QString("%1/%2").arg(group, key), defaultValue).toString();
}

int IniUtils::readInt(const QString& group, const QString& key, int defaultValue) const
{
    bool ok = false;
    int val = readString(group, key).toInt(&ok);
    return ok ? val : defaultValue;
}

double IniUtils::readDouble(const QString& group, const QString& key, double defaultValue) const
{
    bool ok = false;
    double val = readString(group, key).toDouble(&ok);
    return ok ? val : defaultValue;
}

bool IniUtils::readBool(const QString& group, const QString& key, bool defaultValue) const
{
    QString val = readString(group, key).toLower().trimmed();

    if (val == "true" || val == "1" || val == "yes" || val == "on")
        return true;
    if (val == "false" || val == "0" || val == "no" || val == "off")
        return false;

    return defaultValue;
}

QByteArray IniUtils::readByteArray(const QString& group, const QString& key, const QByteArray& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    return m_pSettings->value(QString("%1/%2").arg(group, key), defaultValue).toByteArray();
}

// ==============================================
// 批量操作
// ==============================================

void IniUtils::writeBatch(const QString& group, const QMap<QString, QString>& keyValueMap)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = keyValueMap.begin(); it != keyValueMap.end(); ++it) {
        doWriteString(group, it.key(), it.value());
    }
}

QMap<QString, QString> IniUtils::readBatch(const QString& group) const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QString> map;

    m_pSettings->beginGroup(group);
    QStringList keys = m_pSettings->childKeys();
    for (const QString& key : keys) {
        map.insert(key, m_pSettings->value(key).toString());
    }
    m_pSettings->endGroup();

    return map;
}

void IniUtils::removeKeys(const QString& group, const QStringList& keyList)
{
    QMutexLocker locker(&m_mutex);
    for (const QString& key : keyList) {
        m_pSettings->remove(QString("%1/%2").arg(group, key));
    }
    if (m_autoSync) {
        doSyncToFile();
    }
}

// ==============================================
// 分组高级操作
// ==============================================

QStringList IniUtils::getAllGroups() const
{
    QMutexLocker locker(&m_mutex);
    return m_pSettings->childGroups();
}

QStringList IniUtils::getGroupKeys(const QString& group) const
{
    QMutexLocker locker(&m_mutex);
    m_pSettings->beginGroup(group);
    QStringList keys = m_pSettings->childKeys();
    m_pSettings->endGroup();
    return keys;
}

bool IniUtils::isGroupExist(const QString& group) const
{
    return getAllGroups().contains(group);
}

bool IniUtils::isKeyExist(const QString& group, const QString& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_pSettings->contains(QString("%1/%2").arg(group, key));
}

QMap<QString, QMap<QString, QString>> IniUtils::getAllData() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QMap<QString, QString>> allData;

    QStringList groups = m_pSettings->childGroups();
    for (const QString& group : groups) {
        m_pSettings->beginGroup(group);
        QStringList keys = m_pSettings->childKeys();
        QMap<QString, QString> keyValues;
        for (const QString& key : keys) {
            keyValues.insert(key, m_pSettings->value(key).toString());
        }
        m_pSettings->endGroup();
        allData.insert(group, keyValues);
    }

    return allData;
}

// ==============================================
// 删除/清空操作
// ==============================================

void IniUtils::removeKey(const QString& group, const QString& key)
{
    QMutexLocker locker(&m_mutex);
    m_pSettings->remove(QString("%1/%2").arg(group, key));
    if (m_autoSync) {
        doSyncToFile();
    }
}

void IniUtils::removeGroup(const QString& group)
{
    QMutexLocker locker(&m_mutex);
    m_pSettings->remove(group);
    if (m_autoSync) {
        doSyncToFile();
    }
}

void IniUtils::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_pSettings->clear();
    if (m_autoSync) {
        doSyncToFile();
    }
}
}
