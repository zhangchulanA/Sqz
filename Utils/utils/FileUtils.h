#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QFileInfoList>
#include <QDir>
#include <QByteArray>
#include "SqzGlobal.h"
/**
 * @brief 跨平台超级文件工具类（修复版）
 * @details
 * - 支持 Windows / Linux / macOS
 * - 提供文件/文件夹创建、拷贝、移动、删除、遍历、读写、校验等全功能
 * - 所有操作均为静态函数，无需实例化
 * - 修复了跨设备移动、递归删除栈溢出、MD5大文件内存爆炸、快捷方式生成等问题
 * @note 适用于 Qt 5.12.9+
 */

namespace Sqz::Utils {
class SQZ_FRAMEWORK_API FileUtils
{
public:
    FileUtils() = delete;   // 纯静态类，禁止实例化
    ~FileUtils() = delete;

    // ========================== 1. 创建操作 ==========================
    /**
     * @brief 创建单个文件（自动创建父级目录，已存在不报错）
     * @param filePath 文件完整路径
     * @return 成功true 失败false
     */
    static bool createFile(const QString &filePath);

    /**
     * @brief 递归创建多层文件夹（已存在不报错）
     */
    static bool createDir(const QString &dirPath);

    // ========================== 2. 拷贝操作 ==========================
    /**
     * @brief 拷贝单个文件（支持覆盖）
     * @param srcFile  源文件
     * @param destFile 目标文件
     * @param overwrite 是否覆盖已存在目标文件
     */
    static bool copyFile(const QString &srcFile, const QString &destFile, bool overwrite = true);

    /**
     * @brief 递归拷贝整个文件夹（包含所有子目录/文件）
     * @param srcDir    源目录
     * @param destDir   目标目录
     * @param overwrite 是否覆盖已存在的文件
     */
    static bool copyDir(const QString &srcDir, const QString &destDir, bool overwrite = true);

    // ========================== 3. 移动操作（支持跨设备） ==========================
    /**
     * @brief 移动文件（跨设备自动回退到 复制+删除）
     * @param srcFile   源文件
     * @param destFile  目标文件
     * @param overwrite 是否覆盖已存在目标
     */
    static bool moveFile(const QString &srcFile, const QString &destFile, bool overwrite = true);

    /**
     * @brief 移动文件夹（支持跨设备，自动检测自身移动导致的数据丢失）
     * @param srcDir    源目录
     * @param destDir   目标目录
     * @param overwrite 是否覆盖已存在目标目录
     */
    static bool moveDir(const QString &srcDir, const QString &destDir, bool overwrite = true);

    // ========================== 4. 重命名（文件/目录） ==========================
    /**
     * @brief 重命名文件或文件夹
     * @param oldPath  原路径
     * @param newName  新名称（仅名称，不含路径）
     * @return 成功true 失败false
     */
    static bool rename(const QString &oldPath, const QString &newName);

    // ========================== 5. 删除操作（迭代实现，避免栈溢出） ==========================
    /**
     * @brief 删除单个文件
     */
    static bool deleteFile(const QString &filePath);

    /**
     * @brief 递归删除文件夹（内部使用迭代，避免深层递归栈溢出）
     */
    static bool deleteDir(const QString &dirPath);

    /**
     * @brief 清空文件夹（保留文件夹本身，删除所有内容）
     */
    static bool clearDir(const QString &dirPath);

    // ========================== 6. 遍历/查询 ==========================
    /**
     * @brief 获取文件列表（默认仅文件，不包含目录）
     * @param dirPath   目录路径
     * @param recursive 是否递归子目录
     * @param filters   过滤规则（默认：仅文件，排除.和..）
     */
    static QFileInfoList getFileList(const QString &dirPath,
                                     bool recursive = false,
                                     QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot);

    /**
     * @brief 根据后缀名筛选文件（不区分大小写）
     * @param dirPath   目录
     * @param suffixes  后缀列表，例如 QStringList() << "png" << "jpg"
     * @param recursive 是否递归
     */
    static QFileInfoList getFilesBySuffix(const QString &dirPath, const QStringList &suffixes, bool recursive = false);

    /**
     * @brief 获取文件夹总大小（字节，递归统计，不跟随符号链接）
     */
    static qint64 getDirSize(const QString &dirPath);

    // ========================== 7. 文件读写（支持追加模式） ==========================
    /**
     * @brief 写入文本文件（自动创建目录）
     * @param filePath  文件路径
     * @param content   文本内容
     * @param codec     编码名称，默认 UTF-8
     * @param append    是否追加到文件末尾（默认覆盖）
     */
    static bool writeTextFile(const QString &filePath, const QString &content,
                              const QString &codec = "UTF-8", bool append = false);

    /**
     * @brief 读取文本文件
     * @param filePath  文件路径
     * @param codec     编码名称
     */
    static QString readTextFile(const QString &filePath, const QString &codec = "UTF-8");

    /**
     * @brief 写入二进制文件（覆盖模式）
     */
    static bool writeBinaryFile(const QString &filePath, const QByteArray &data);

    /**
     * @brief 读取二进制文件
     */
    static QByteArray readBinaryFile(const QString &filePath);

    // ========================== 8. 路径工具 ==========================
    /**
     * @brief 标准化路径（自动适配平台分隔符，处理冗余../和./）
     */
    static QString formatPath(const QString &path);

    /**
     * @brief 获取文件所在目录
     */
    static QString getFileDir(const QString &filePath);

    /**
     * @brief 获取文件名称（带后缀）
     */
    static QString getFileName(const QString &filePath);

    /**
     * @brief 获取文件名称（无后缀）
     */
    static QString getFileNameWithoutSuffix(const QString &filePath);

    /**
     * @brief 获取文件后缀（不含点）
     */
    static QString getFileSuffix(const QString &filePath);

    // ========================== 9. 校验/判断 ==========================
    static bool isFileExist(const QString &filePath);
    static bool isDirExist(const QString &dirPath);
    static bool isPathExist(const QString &path);

    /**
     * @brief 获取文件 MD5（分块读取，支持大文件）
     */
    static QString getFileMd5(const QString &filePath);

    // ========================== 10. 桌面快捷方式（跨平台实用方案） ==========================
    /**
     * @brief 在桌面创建可运行的快捷方式
     * @param filePath  目标文件（可执行文件或文档）
     * @param linkName  快捷方式显示名称（不含扩展名）
     * @details
     * - Windows: 生成一个 .bat 批处理文件，内容为 start "" "目标路径"
     * - Linux:   生成一个 .desktop 文件，Exec=目标路径，并添加可执行权限
     * @note 这是无需外部依赖的跨平台实现，虽然不是原生 .lnk 文件但功能等价
     */
    static bool createDesktopLink(const QString &filePath, const QString &linkName);

private:
    // 内部辅助函数：递归复制目录内容（不处理顶层目录自身）
    static bool copyDirContent(const QString &srcDir, const QString &destDir, bool overwrite);
    // 内部辅助函数：递归删除目录内容（不删除目录自身）
    static bool clearDirContent(const QString &dirPath);
    // 内部辅助函数：跨设备移动的核心逻辑
    static bool moveFileCrossDevice(const QString &src, const QString &dst, bool overwrite);
    static bool moveDirCrossDevice(const QString &src, const QString &dst, bool overwrite);
};
}
#endif // FILEUTILS_H
