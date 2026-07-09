#ifndef INIUTILS_H
#define INIUTILS_H

#include <QString>
#include <QMutex>
#include <QSettings>
#include <QStringList>
#include <QMap>
#include <QByteArray>
#include "SqzGlobal.h"
/**
 * @brief 增强版 INI 配置文件读写工具类
 * @details
 * - 支持多文件操作（每个对象独立管理一个 INI 文件）
 * - 全类型读写（字符串、整数、浮点数、布尔、字节数组）
 * - 批量操作、分组管理、文件备份
 * - 线程安全（使用非递归互斥锁，通过拆分无锁私有函数避免递归）
 * - 跨平台（Windows / Linux / macOS）
 * - 可配置自动同步（性能优化）
 * @note 适用于 Qt 5.12.9+
 * @warning 禁止拷贝对象，请使用指针或引用传递
 */

namespace Sqz::Utils {
class SQZ_FRAMEWORK_API IniUtils
{
public:
    /**
     * @brief 构造函数 - 绑定 INI 文件路径并初始化 QSettings 对象
     * @param filePath INI 文件路径（相对或绝对路径均可）
     * @param autoSync 是否每次写入操作后自动同步到磁盘
     *                 true  = 每次写操作后立即写入磁盘（数据安全优先）
     *                 false = 手动调用 flush() 或 syncToFile() 才写入（性能优先）
     * @note 自动设置 QSettings 的编码为 UTF-8，确保中文正确读写
     */
    explicit IniUtils(const QString& filePath, bool autoSync = true);

    /**
     * @brief 析构函数 - 自动同步内存数据到磁盘并释放资源
     * @details 在销毁对象前，确保所有未写入的数据被保存，然后删除 QSettings 指针
     */
    ~IniUtils();

    // 禁用拷贝构造函数和赋值运算符，防止悬空指针和双重删除
    IniUtils(const IniUtils&) = delete;
    IniUtils& operator=(const IniUtils&) = delete;

    // ==============================================
    // 基础文件操作
    // ==============================================

    /**
     * @brief 获取当前绑定的 INI 文件路径
     * @return QString 文件路径（构造函数传入的原始路径）
     * @note 线程安全
     */
    QString getFilePath() const;

    /**
     * @brief 检查当前 INI 文件是否存在于磁盘上
     * @return true  文件存在
     * @return false 文件不存在
     * @note 线程安全
     */
    bool isFileExist() const;

    /**
     * @brief 重新加载磁盘上的 INI 文件
     * @details 销毁当前 QSettings 对象并重新创建，用于外部程序修改了文件后重新读取最新内容。
     *          注意：未同步的内存修改会丢失，建议在调用 reloadFile() 前先调用 syncToFile()。
     * @note 线程安全
     */
    void reloadFile();

    /**
     * @brief 手动将内存中的所有修改立即写入磁盘
     * @details 调用 QSettings::sync()，确保任何未写入的更改持久化。
     *          当 autoSync = false 时，必须调用此函数或 flush() 来保存修改。
     * @note 线程安全
     */
    void syncToFile();

    /**
     * @brief flush() 是 syncToFile() 的同名函数，提供更符合习惯的命名
     * @details 二者行为完全一致，均用于强制刷新缓冲区到磁盘。
     * @note 线程安全
     */
    void flush();

    /**
     * @brief 备份当前 INI 文件到指定路径
     * @param backupPath 目标备份文件路径（建议包含 .bak 后缀）
     * @return true  备份成功（原文件存在且拷贝成功）
     * @return false 备份失败（原文件不存在或拷贝失败）
     * @details 备份前会先调用 syncToFile() 确保内存数据已写入原文件，然后再执行文件拷贝。
     * @note 线程安全
     */
    bool backupFile(const QString& backupPath);

    /**
     * @brief 设置是否启用自动同步模式
     * @param enabled true  = 每次写操作后自动同步到磁盘
     *                false = 必须手动调用 flush() 或 syncToFile()
     * @note 线程安全
     */
    void setAutoSync(bool enabled);

    /**
     * @brief 获取当前的自动同步状态
     * @return true  自动同步已启用
     * @return false 自动同步未启用
     * @note 线程安全
     */
    bool autoSync() const;

    // ==============================================
    // 基础写入操作
    // ==============================================

    /**
     * @brief 写入字符串值到指定组和键
     * @param group 组名（INI 文件中的节名，如 "[Network]"）
     * @param key   键名
     * @param value 要写入的字符串值
     * @note 线程安全；如果 autoSync 为 true，则立即同步到磁盘
     */
    void writeString(const QString& group, const QString& key, const QString& value);

    /**
     * @brief 写入整数值到指定组和键
     * @param group 组名
     * @param key   键名
     * @param value 要写入的整数值
     * @note 线程安全；内部转为字符串后存储
     */
    void writeInt(const QString& group, const QString& key, int value);

    /**
     * @brief 写入浮点数值到指定组和键
     * @param group     组名
     * @param key       键名
     * @param value     要写入的浮点数值
     * @param precision 小数精度（有效范围 0～15，超出后自动裁剪）
     * @note 线程安全；使用定点表示法（'f'）存储，保证精度可控
     */
    void writeDouble(const QString& group, const QString& key, double value, int precision = 6);

    /**
     * @brief 写入布尔值到指定组和键
     * @param group 组名
     * @param key   键名
     * @param value 要写入的布尔值
     * @note 线程安全；存储为字符串 "true" 或 "false"
     */
    void writeBool(const QString& group, const QString& key, bool value);

    /**
     * @brief 写入字节数组到指定组和键
     * @param group 组名
     * @param key   键名
     * @param value 要写入的字节数组（可用于存储二进制数据或序列化对象）
     * @note 线程安全；Qt 的 QSettings 内部会对 QByteArray 进行 Base64 编码存储
     */
    void writeByteArray(const QString& group, const QString& key, const QByteArray& value);

    // ==============================================
    // 基础读取操作
    // ==============================================

    /**
     * @brief 读取字符串值
     * @param group        组名
     * @param key          键名
     * @param defaultValue 键不存在时返回的默认值（默认为空字符串）
     * @return QString 读取到的字符串或默认值
     * @note 线程安全
     */
    QString readString(const QString& group, const QString& key, const QString& defaultValue = "") const;

    /**
     * @brief 读取整数值
     * @param group        组名
     * @param key          键名
     * @param defaultValue 键不存在或转换失败时返回的默认值（默认为 0）
     * @return int 读取到的整数值或默认值
     * @note 线程安全
     */
    int readInt(const QString& group, const QString& key, int defaultValue = 0) const;

    /**
     * @brief 读取浮点数值
     * @param group        组名
     * @param key          键名
     * @param defaultValue 键不存在或转换失败时返回的默认值（默认为 0.0）
     * @return double 读取到的浮点数值或默认值
     * @note 线程安全
     */
    double readDouble(const QString& group, const QString& key, double defaultValue = 0.0) const;

    /**
     * @brief 读取布尔值
     * @param group        组名
     * @param key          键名
     * @param defaultValue 键不存在或无法识别时返回的默认值（默认为 false）
     * @return bool 读取到的布尔值或默认值
     * @details 支持的 true 字符串（不区分大小写）： "true", "1", "yes", "on"
     *          支持的 false 字符串： "false", "0", "no", "off"
     * @note 线程安全
     */
    bool readBool(const QString& group, const QString& key, bool defaultValue = false) const;

    /**
     * @brief 读取字节数组
     * @param group        组名
     * @param key          键名
     * @param defaultValue 键不存在时返回的默认值（默认为空 QByteArray）
     * @return QByteArray 读取到的字节数组或默认值
     * @note 线程安全
     */
    QByteArray readByteArray(const QString& group, const QString& key, const QByteArray& defaultValue = QByteArray()) const;

    // ==============================================
    // 批量操作
    // ==============================================

    /**
     * @brief 批量写入同一组下的多个键值对
     * @param group        组名
     * @param keyValueMap  要写入的键值对映射（QMap<QString, QString>）
     * @details 在一次加锁周期内完成所有写入，若 autoSync 为 true，则最后一次写入后统一同步一次，性能优于逐个调用 writeString。
     * @note 线程安全
     */
    void writeBatch(const QString& group, const QMap<QString, QString>& keyValueMap);

    /**
     * @brief 读取指定组下的所有键值对
     * @param group 组名
     * @return QMap<QString, QString> 该组下所有键值对
     * @note 线程安全；若组不存在，返回空 Map
     */
    QMap<QString, QString> readBatch(const QString& group) const;

    /**
     * @brief 批量删除同一组下的多个键
     * @param group   组名
     * @param keyList 要删除的键名列表
     * @note 线程安全；若 autoSync 为 true，则删除完成后统一同步一次
     */
    void removeKeys(const QString& group, const QStringList& keyList);

    // ==============================================
    // 分组高级操作
    // ==============================================

    /**
     * @brief 获取 INI 文件中所有顶级组名列表
     * @return QStringList 组名列表（不含重复，顺序不定）
     * @note 线程安全
     */
    QStringList getAllGroups() const;

    /**
     * @brief 获取指定组下的所有键名列表
     * @param group 组名
     * @return QStringList 键名列表（若组不存在，返回空列表）
     * @note 线程安全
     */
    QStringList getGroupKeys(const QString& group) const;

    /**
     * @brief 判断指定组是否存在
     * @param group 组名
     * @return true  组存在
     * @return false 组不存在
     * @note 线程安全
     */
    bool isGroupExist(const QString& group) const;

    /**
     * @brief 判断指定组下的键是否存在
     * @param group 组名
     * @param key   键名
     * @return true  键存在
     * @return false 键不存在
     * @note 线程安全
     */
    bool isKeyExist(const QString& group, const QString& key) const;

    /**
     * @brief 获取整个 INI 文件的所有数据
     * @return QMap<QString, QMap<QString, QString>>
     *         外层 key = 组名（group），内层 Map = 该组下所有 (key, value) 对
     * @details 一次性读取全部配置内容，适用于导出、对比、序列化等场景。
     *          注意：对于极大的 INI 文件，此操作可能消耗较多内存和时间。
     * @note 线程安全
     */
    QMap<QString, QMap<QString, QString>> getAllData() const;

    // ==============================================
    // 删除/清空操作
    // ==============================================

    /**
     * @brief 删除指定组下的单个键
     * @param group 组名
     * @param key   键名
     * @note 线程安全；若 autoSync 为 true，则删除后立即同步
     */
    void removeKey(const QString& group, const QString& key);

    /**
     * @brief 删除整个组及其所有键
     * @param group 组名
     * @note 线程安全；若 autoSync 为 true，则删除后立即同步
     */
    void removeGroup(const QString& group);

    /**
     * @brief 清空整个 INI 文件的所有内容（包括所有组和键）
     * @note 线程安全；若 autoSync 为 true，则清空后立即同步
     * @warning 此操作不可逆，请谨慎使用
     */
    void clearAll();

private:
    /**
     * @brief 内部同步实现（不加锁，由调用者保证互斥）
     * @details 直接调用 QSettings::sync()，将内存修改写入磁盘。
     *          仅供加锁的公有函数调用，避免递归锁问题。
     */
    void doSyncToFile();

    /**
     * @brief 内部写入实现（不加锁，由调用者保证互斥）
     * @param group 组名
     * @param key   键名
     * @param value 值
     * @details 实际执行 QSettings::setValue，并根据 m_autoSync 决定是否调用 doSyncToFile。
     *          仅供加锁的公有写入函数调用。
     */
    void doWriteString(const QString& group, const QString& key, const QString& value);

    QSettings*      m_pSettings;    ///< Qt 原生 INI 操作对象指针
    QString         m_filePath;     ///< 当前 INI 文件路径
    mutable QMutex  m_mutex;        ///< 线程互斥锁（非递归，性能更优）
    bool            m_autoSync;     ///< 是否自动同步标志
};
}
#endif // INIUTILS_H
