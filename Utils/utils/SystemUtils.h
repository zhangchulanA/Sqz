#ifndef SYSTEMUTILS_H
#define SYSTEMUTILS_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include "SqzGlobal.h"
/**
 * @brief 系统工具类（完整修复版，Qt 5.12.9 跨平台）
 * @details
 * - 程序路径、系统类型与版本、硬件信息、磁盘信息
 * - 用户与主机、常用目录、环境变量、系统状态
 * - 架构检测、权限判断、系统控制（关机/重启/注销）
 * - 支持 Windows 7/8/10/11、Ubuntu 及其他 Linux 发行版
 * @note 纯静态类，禁止实例化
 */

namespace Sqz {
class SQZ_FRAMEWORK_API SystemUtils
{
public:
    SystemUtils() = delete;

    // ==============================================
    // 1. 程序路径与启动
    // ==============================================
    static QString appDirPath();            // 程序运行目录
    static QString appFilePath();           // 程序完整路径
    static QString appName();               // 程序名称
    static QStringList appArguments();      // 启动参数
    static QString appDataPath();           // 程序数据目录（AppDataLocation）

    // ==============================================
    // 2. 系统类型判断
    // ==============================================
    static bool isWindows();                // 是否 Windows
    static bool isLinux();                  // 是否 Linux

    // ==============================================
    // 3. 系统版本详细判断
    // ==============================================
    static QString systemName();            // 完整系统名称（如 "Windows 10 Pro"）
    static QString kernelVersion();         // 内核版本
    static bool isWindows10OrHigher();      // Windows 10 或更高版本
    static bool isWindows11();              // 是否 Windows 11
    static bool isUbuntu();                 // 是否 Ubuntu 发行版
    static bool isSystemDarkMode();         // 系统是否处于暗色模式

    // ==============================================
    // 4. 硬件信息
    // ==============================================
    static int cpuCoreCount();              // CPU 逻辑核心数
    static QString cpuArchitecture();       // CPU 架构（x86_64/arm64）
    static qint64 totalMemoryMB();          // 总内存（MB）
    static qint64 availableMemoryMB();      // 可用内存（MB）

    // ==============================================
    // 5. 磁盘信息
    // ==============================================
    static qint64 diskTotalBytes(const QString &path);  // 磁盘总容量（字节）
    static qint64 diskFreeBytes(const QString &path);   // 磁盘剩余容量（字节）

    // ==============================================
    // 6. 计算机 & 用户信息
    // ==============================================
    static QString computerName();          // 计算机名
    static QString userName();              // 当前登录用户名
    static QString systemLanguage();        // 系统语言（如 "zh_CN"）

    // ==============================================
    // 7. 常用系统目录
    // ==============================================
    static QString desktopPath();           // 桌面目录
    static QString documentPath();          // 文档目录
    static QString downloadPath();          // 下载目录
    static QString tempPath();              // 临时目录
    static QString homePath();              // 用户主目录

    // ==============================================
    // 8. 环境变量
    // ==============================================
    static QString getEnv(const QString &key);          // 读取环境变量
    static bool setEnv(const QString &key, const QString &value);  // 设置当前进程环境变量

    // ==============================================
    // 9. 系统状态
    // ==============================================
    static qint64 systemBootTime();         // 开机时间戳（Unix 秒）
    static qint64 systemUptimeSec();        // 系统已运行秒数

    // ==============================================
    // 10. 系统架构
    // ==============================================
    static bool is64BitSystem();            // 是否 64 位系统
    static bool is32BitSystem();            // 是否 32 位系统
    static bool isArmArchitecture();        // 是否 ARM 架构

    // ==============================================
    // 11. 权限检测
    // ==============================================
    static bool isRunAsAdmin();             // 是否以管理员 / root 运行

    // ==============================================
    // 12. 系统控制（关机/重启/注销）
    // ==============================================
    static bool shutdownSystem();           // 关机（成功返回 true）
    static bool rebootSystem();             // 重启
    static bool logoutUser();               // 注销当前用户
};
}
#endif // SYSTEMUTILS_H
