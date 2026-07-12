#include "SystemUtils.h"
#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QStorageInfo>
#include <QDateTime>
#include <QtGlobal>
#include <QProcess>
#include <QSettings>
#include <QThread>
#include <QLocale>
#include <QRegExp>
#include <QTextStream>
#include <QDebug>

#ifdef Q_OS_WIN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <Windows.h>
#endif
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

//=============================================================================
// 程序路径
//=============================================================================

namespace Sqz {
QString SystemUtils::appDirPath()
{
    return QCoreApplication::applicationDirPath();
}

QString SystemUtils::appFilePath()
{
    return QCoreApplication::applicationFilePath();
}

QString SystemUtils::appName()
{
    return QCoreApplication::applicationName();
}

QStringList SystemUtils::appArguments()
{
    return QCoreApplication::arguments();
}

QString SystemUtils::appDataPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

//=============================================================================
// 系统类型判断
//=============================================================================

bool SystemUtils::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool SystemUtils::isLinux()
{
#ifdef Q_OS_LINUX
    return true;
#else
    return false;
#endif
}

//=============================================================================
// 系统版本详细判断
//=============================================================================

QString SystemUtils::systemName()
{
    return QSysInfo::prettyProductName();
}

QString SystemUtils::kernelVersion()
{
    return QSysInfo::kernelVersion();
}

bool SystemUtils::isWindows10OrHigher()
{
#ifdef Q_OS_WIN
    QString ver = QSysInfo::kernelVersion(); // 如 "10.0.19041"
    QStringList parts = ver.split('.');
    if (parts.size() >= 2) {
        int major = parts[0].toInt();
        int minor = parts[1].toInt();
        return (major > 10) || (major == 10 && minor >= 0);
    }
#endif
    return false;
}

bool SystemUtils::isWindows11()
{
#ifdef Q_OS_WIN
    // Windows 11 内核版本号 >= 10.0.22000
    QString ver = QSysInfo::kernelVersion();
    QStringList parts = ver.split('.');
    if (parts.size() >= 3) {
        int build = parts[2].toInt();
        return build >= 22000;
    }
#endif
    return false;
}

bool SystemUtils::isUbuntu()
{
    return systemName().contains("Ubuntu", Qt::CaseInsensitive);
}

bool SystemUtils::isSystemDarkMode()
{
#ifdef Q_OS_WIN
    // Windows 10 1809+ 暗色模式检测
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                  QSettings::NativeFormat);
    // AppsUseLightTheme = 0 表示使用暗色模式
    return reg.value("AppsUseLightTheme", 1).toInt() == 0;
#else
    // Linux: 优先尝试 GNOME 的 gsettings 查询
    QProcess proc;
    proc.start("gsettings", QStringList() << "get" << "org.gnome.desktop.interface" << "gtk-theme");
    if (proc.waitForFinished(1000)) {
        QString output = proc.readAllStandardOutput().trimmed().toLower();
        if (output.contains("dark") || output.contains("adapta") || output.contains("yaru-dark")) {
            return true;
        }
    }
    // 回退：检查环境变量
    QString env = getEnv("XDG_CURRENT_DESKTOP").toLower();
    QString theme = getEnv("GTK_THEME").toLower();
    if (theme.contains("dark") || env.contains("gnome")) {
        return true;
    }
    return false;
#endif
}

//=============================================================================
// 硬件信息
//=============================================================================

int SystemUtils::cpuCoreCount()
{
    return QThread::idealThreadCount();
}

QString SystemUtils::cpuArchitecture()
{
    return QSysInfo::currentCpuArchitecture();
}

qint64 SystemUtils::totalMemoryMB()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<qint64>(statex.ullTotalPhys / (1024 * 1024));
    }
    return 0;
#else
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemUtils::totalMemoryMB: cannot open /proc/meminfo";
        return 0;
    }
    QTextStream in(&file);
    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith("MemTotal:")) {
            QRegExp rx("MemTotal:\\s*(\\d+)\\s*kB");
            if (rx.indexIn(line) != -1) {
                qint64 kb = rx.cap(1).toLongLong();
                return kb / 1024;  // 转换为 MB
            }
            break;
        }
    }
    return 0;
#endif
}

qint64 SystemUtils::availableMemoryMB()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<qint64>(statex.ullAvailPhys / (1024 * 1024));
    }
    return 0;
#else
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemUtils::availableMemoryMB: cannot open /proc/meminfo";
        return 0;
    }
    QTextStream in(&file);
    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith("MemAvailable:")) {
            QRegExp rx("MemAvailable:\\s*(\\d+)\\s*kB");
            if (rx.indexIn(line) != -1) {
                qint64 kb = rx.cap(1).toLongLong();
                return kb / 1024;
            }
            break;
        }
    }
    // 如果没有 MemAvailable（旧内核），尝试使用 MemFree + Cached 近似
    // 简化处理：返回 totalMemoryMB - 估算已用，但直接返回 0 避免误导
    qWarning() << "SystemUtils::availableMemoryMB: MemAvailable not found in /proc/meminfo";
    return 0;
#endif
}

//=============================================================================
// 磁盘信息
//=============================================================================

qint64 SystemUtils::diskTotalBytes(const QString &path)
{
    QStorageInfo info(path);
    if (!info.isValid()) {
        qWarning() << "SystemUtils::diskTotalBytes: invalid path" << path;
        return 0;
    }
    return info.bytesTotal();
}

qint64 SystemUtils::diskFreeBytes(const QString &path)
{
    QStorageInfo info(path);
    if (!info.isValid()) {
        qWarning() << "SystemUtils::diskFreeBytes: invalid path" << path;
        return 0;
    }
    return info.bytesFree();
}

//=============================================================================
// 计算机 & 用户
//=============================================================================

QString SystemUtils::computerName()
{
    return QSysInfo::machineHostName();
}

QString SystemUtils::userName()
{
    return QProcessEnvironment::systemEnvironment().value(
        isWindows() ? "USERNAME" : "USER", "unknown");
}

QString SystemUtils::systemLanguage()
{
    return QLocale::system().name();
}

//=============================================================================
// 常用目录
//=============================================================================

QString SystemUtils::desktopPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

QString SystemUtils::documentPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

QString SystemUtils::downloadPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

QString SystemUtils::tempPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

QString SystemUtils::homePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

//=============================================================================
// 环境变量
//=============================================================================

QString SystemUtils::getEnv(const QString &key)
{
    return QProcessEnvironment::systemEnvironment().value(key);
}

bool SystemUtils::setEnv(const QString &key, const QString &value)
{
    // 注意：仅修改当前进程的环境变量，不影响系统永久设置
    return qputenv(key.toUtf8(), value.toUtf8());
}

//=============================================================================
// 系统运行时长
//=============================================================================

qint64 SystemUtils::systemBootTime()
{
    return QDateTime::currentSecsSinceEpoch() - systemUptimeSec();
}

qint64 SystemUtils::systemUptimeSec()
{
#ifdef Q_OS_WIN
    return static_cast<qint64>(GetTickCount64() / 1000);
#else
    QFile file("/proc/uptime");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemUtils::systemUptimeSec: cannot open /proc/uptime";
        return 0;
    }
    QTextStream in(&file);
    QString line = in.readLine();
    if (line.isEmpty()) {
        return 0;
    }
    QStringList parts = line.split(' ', QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        return 0;
    }
    bool ok = false;
    double uptime = parts.first().toDouble(&ok);
    return ok ? static_cast<qint64>(uptime) : 0;
#endif
}

//=============================================================================
// 系统架构
//=============================================================================

bool SystemUtils::is64BitSystem()
{
    QString arch = QSysInfo::currentCpuArchitecture();
    return arch.contains("x86_64") || arch.contains("arm64") || arch.contains("mips64");
}

bool SystemUtils::is32BitSystem()
{
    return !is64BitSystem();
}

bool SystemUtils::isArmArchitecture()
{
    return QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive);
}

//=============================================================================
// 管理员权限
//=============================================================================

bool SystemUtils::isRunAsAdmin()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, cbSize, &cbSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isAdmin == TRUE;
#else
    return (getuid() == 0);
#endif
}

//=============================================================================
// 系统控制（关机/重启/注销）—— 返回命令执行是否成功
//=============================================================================

bool SystemUtils::shutdownSystem()
{
#ifdef Q_OS_WIN
    // Windows: 使用 shutdown 命令，-s 关机，-t 0 立即执行
    return QProcess::startDetached("shutdown", QStringList() << "-s" << "-t" << "0");
#else
    // Linux: 尝试 systemctl，如果失败则回退到 poweroff
    if (QProcess::startDetached("systemctl", QStringList() << "poweroff")) {
        return true;
    }
    return QProcess::startDetached("poweroff", QStringList());
#endif
}

bool SystemUtils::rebootSystem()
{
#ifdef Q_OS_WIN
    return QProcess::startDetached("shutdown", QStringList() << "-r" << "-t" << "0");
#else
    if (QProcess::startDetached("systemctl", QStringList() << "reboot")) {
        return true;
    }
    return QProcess::startDetached("reboot", QStringList());
#endif
}

bool SystemUtils::logoutUser()
{
#ifdef Q_OS_WIN
    // Windows: shutdown -l 注销当前用户
    return QProcess::startDetached("shutdown", QStringList() << "-l");
#else
    // Linux: 尝试多种注销命令
    // 1. GNOME
    if (QProcess::startDetached("gnome-session-quit", QStringList() << "--logout" << "--no-prompt")) {
        return true;
    }
    // 2. KDE
    if (QProcess::startDetached("qdbus", QStringList() << "org.kde.ksmserver" << "/KSMServer" << "logout" << "0" << "0" << "0")) {
        return true;
    }
    // 3. XFCE
    if (QProcess::startDetached("xfce4-session-logout", QStringList() << "--logout")) {
        return true;
    }
    // 4. 通用: 使用 loginctl 终止用户会话（需要确定会话 ID，简化处理）
    // 最后尝试 pkill -KILL -u $USER（不推荐），直接返回 false
    qWarning() << "SystemUtils::logoutUser: No known logout command found for this desktop environment";
    return false;
#endif
}
}
