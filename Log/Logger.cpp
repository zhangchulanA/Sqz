#include "Logger.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDebug>
#include <QRegularExpression>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <exception>
#include <cerrno>
#include "SqzBus.h"

#ifndef Q_OS_WIN
#  include <unistd.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <execinfo.h>
#else
#  include <io.h>
#  include <fcntl.h>
#  include <signal.h>
#endif

using namespace Sqz;

#define COLOR_DEBUG  "\033[34m"
#define COLOR_INFO   "\033[32m"
#define COLOR_WARN   "\033[33m"
#define COLOR_ERROR  "\033[31m"
#define COLOR_CLEAR  "\033[0m"

// ==================== 文件作用域：stderr 重定向相关 ====================
// 为什么不放类里：这两个 fd 需要被文件内的裸函数访问，且不该暴露给外部。
namespace {
    int               g_origStderrFd      = -1;   // 原始 stderr 副本
    int               g_stderrPipeReadFd  = -1;
    int               g_stderrPipeWriteFd = -1;
    std::thread       g_stderrThread;
    std::atomic<bool> g_stderrRunning{false};
    std::atomic<bool> g_stderrInstalled{false};
    const int         STDERR_BUF_SIZE     = 4096;
}

// ==================== 静态成员定义 ====================
Logger::CrashSlot         Logger::s_ring[Logger::CRASH_RING_SIZE];
std::atomic<int>          Logger::s_ringHead{0};
int                       Logger::s_crashFd = -1;
std::atomic<bool>         Logger::s_crashHandlersInstalled{false};
bool                      Logger::s_qtHandlerInstalled = false;

// ==================== 底层写接口 ====================

// 写一行到原始 stderr（保留终端可见性）
static void writeOriginalStderr(const QByteArray& utf8)
{
    const int fd = (g_origStderrFd >= 0) ? g_origStderrFd : 2;
#ifdef Q_OS_WIN
    ::_write(fd, utf8.constData(), static_cast<unsigned>(utf8.size()));
#else
    (void)!::write(fd, utf8.constData(), static_cast<size_t>(utf8.size()));
#endif
}

// 输出日志行到控制台（带 ANSI 颜色）
static void writeConsoleLine(const QString& line, LogLevel level)
{
    QString colored = Logger::colorPrefix(level) + line
                      + Logger::colorSuffix() + QLatin1Char('\n');
    writeOriginalStderr(colored.toUtf8());
}

// ==================== writeRawStderr：Logger 内部错误提示 ====================
// 不走 qWarning，因为 qInstallMessageHandler 装好后会递归
void Logger::writeRawStderr(const QString& msg)
{
    QByteArray utf8 = msg.toUtf8();
    utf8.append('\n');
    writeOriginalStderr(utf8);
}

// ==================== 崩溃环形缓冲区写入 ====================
// 无锁：fetch_add 分配槽位；先置 len=0 再写，写完置 len，
// 崩溃发生在 memcpy 时 len 仍为 0，信号处理器自动跳过
void Logger::writeCrashRing(const QString& logText)
{
    QByteArray utf8 = (logText + QLatin1Char('\n')).toUtf8();
    const int n = qMin<int>(utf8.size(), CRASH_LINE_MAX - 1);
    if (n <= 0) return;

    const int idx = s_ringHead.fetch_add(1, std::memory_order_relaxed)
                    % CRASH_RING_SIZE;

    s_ring[idx].len = 0;
    std::memcpy(s_ring[idx].buf, utf8.constData(), static_cast<size_t>(n));
    s_ring[idx].buf[n] = '\0';
    s_ring[idx].len = n;
}

// ==================== 第 1 层：Qt 消息处理器 ====================
// 把 qDebug/qWarning/qCritical/qFatal 路由到 Logger
static void sqzQtMessageHandler(QtMsgType type,
                                const QMessageLogContext& ctx,
                                const QString& msg)
{
    LogLevel lvl   = E_LOG_DEBUG;
    bool     force = false;
    switch (type) {
    case QtDebugMsg:    lvl = E_LOG_DEBUG; break;
    case QtInfoMsg:     lvl = E_LOG_INFO;  break;
    case QtWarningMsg:  lvl = E_LOG_WARN;  break;
    case QtCriticalMsg: lvl = E_LOG_ERROR; break;
    case QtFatalMsg:    lvl = E_LOG_ERROR; force = true; break;
    }

    Logger::instance().log(lvl,
                           ctx.file     ? ctx.file     : "qt",
                           ctx.line,
                           ctx.function ? ctx.function : "qt",
                           msg, force);

    if (type == QtFatalMsg) {
        Logger::instance().flush();
        std::abort();   // 触发 SIGABRT，让崩溃处理器 dump 环形区
    }
}

// ==================== 第 3 层：信号处理器 ====================

// 打印调用栈到 fd（backtrace_symbols_fd 异步信号安全，不 malloc）
static void dumpBacktrace(int fd, int maxFrames)
{
    if (fd < 0 || maxFrames <= 0) return;
#ifndef Q_OS_WIN
    void* frames[64];
    if (maxFrames > 64) maxFrames = 64;

    int n = ::backtrace(frames, maxFrames);
    if (n > 0) {
        static const char head[] = "---------- CALL STACK ----------\n";
        (void)!::write(fd, head, sizeof(head) - 1);
        ::backtrace_symbols_fd(frames, n, fd);
        static const char tail[] = "---------- END STACK ----------\n";
        (void)!::write(fd, tail, sizeof(tail) - 1);
    }
#endif
}

// 信号处理器：只用异步信号安全函数
static void sqzCrashSignalHandler(int sig)
{
    const char* sigName = "UNKNOWN";
    switch (sig) {
    case SIGSEGV: sigName = "SIGSEGV"; break;
    case SIGABRT: sigName = "SIGABRT"; break;
    case SIGFPE:  sigName = "SIGFPE";  break;
    case SIGILL:  sigName = "SIGILL";  break;
#ifdef SIGBUS
    case SIGBUS:  sigName = "SIGBUS";  break;
#endif
    case SIGTERM: sigName = "SIGTERM"; break;
    case SIGINT:  sigName = "SIGINT";  break;
    default: break;
    }

    if (Logger::s_crashFd >= 0) {
        char head[192];
        const int nHead = std::snprintf(head, sizeof(head),
                                        "\n========== CRASH: %s (%d) ==========\n",
                                        sigName, sig);
        if (nHead > 0) {
            (void)!::write(Logger::s_crashFd, head, static_cast<size_t>(nHead));
        }

        // 顺序 dump 环形缓冲区（从最旧到最新）
        const int h = Logger::s_ringHead.load(std::memory_order_relaxed);
        for (int i = 0; i < Logger::CRASH_RING_SIZE; ++i) {
            const int idx = (h + i) % Logger::CRASH_RING_SIZE;
            const int len = Logger::s_ring[idx].len;
            if (len > 0) {
                (void)!::write(Logger::s_crashFd,
                               Logger::s_ring[idx].buf,
                               static_cast<size_t>(len));
            }
        }

        // 调用栈
        dumpBacktrace(Logger::s_crashFd, 48);

        static const char tail[] = "========== END CRASH ==========\n";
        (void)!::write(Logger::s_crashFd, tail, sizeof(tail) - 1);
        ::fsync(Logger::s_crashFd);
    }

    // 恢复默认处理器，重新触发让系统生成 core dump
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

// ==================== 第 4 层：std::terminate ====================
static void sqzTerminateHandler()
{
    Logger::instance().log(E_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__,
                           QStringLiteral("std::terminate: uncaught C++ exception"),
                           true);
    Logger::instance().flush();
    std::abort();
}

// ==================== Logger 实现 ====================

Logger::Logger()
    : m_curJulianDay(0)
    , m_runIndex(1)
    , m_partIndex(1)
    , m_logLevel(E_LOG_DEBUG)
    , m_maxFileSize(10 * 1024 * 1024)
    , m_enableConsole(false)
    , m_enableFile(false)
    , m_flushCounter(0)
    , m_destroyed(0)
{
}

Logger::~Logger()
{
    // 先停 stderr 读线程（可能仍在阻塞读）
    stopStderrRedirect();

    m_destroyed.storeRelease(1);

    QMutexLocker locker(&m_mutex);
    closeCurrentFile();

    // 关闭崩溃日志 fd
    if (s_crashFd >= 0) {
#ifdef Q_OS_WIN
        ::_close(s_crashFd);
#else
        ::close(s_crashFd);
#endif
        s_crashFd = -1;
    }

    // 关闭原始 stderr 副本
    if (g_origStderrFd >= 0) {
#ifdef Q_OS_WIN
        ::_close(g_origStderrFd);
#else
        ::close(g_origStderrFd);
#endif
        g_origStderrFd = -1;
    }
}

Logger& Logger::instance()
{
    static Logger obj;
    return obj;
}

void Logger::init(const QString& logDir,
                  const QString& filePrefix,
                  bool enableConsole,
                  bool enableFile,
                  qint64 maxSizeMB,
                  int keepDays)
{
    QMutexLocker locker(&m_mutex);

    closeCurrentFile();

    m_logDir        = logDir;
    m_filePrefix    = filePrefix;
    m_enableConsole = enableConsole;
    m_enableFile    = enableFile;

    qint64 effectiveMB = maxSizeMB;
    if (effectiveMB < 1) {
        writeRawStderr(QString("[Logger] maxSizeMB=%1 invalid, clamped to 1.").arg(maxSizeMB));
        effectiveMB = 1;
    }
    m_maxFileSize = effectiveMB * 1024 * 1024;

    const QDate today = QDateTime::currentDateTime().date();
    m_curDate      = today.toString("yyyyMMdd");
    m_curJulianDay = today.toJulianDay();
    m_partIndex    = 1;

    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        m_runIndex = 1;
    } else {
        m_runIndex = getMaxRunForDate(m_curDate) + 1;
    }

    if (m_enableFile) {
        createNewRollFile();
    }

    int effectiveKeepDays = keepDays;
    if (effectiveKeepDays < 0) {
        writeRawStderr(QString("[Logger] keepDays=%1 invalid, treated as 0.").arg(keepDays));
        effectiveKeepDays = 0;
    }
    if (effectiveKeepDays > 0) {
        cleanOldLogs(effectiveKeepDays);
    }

    // 装崩溃处理器（含 stderr 重定向）
    installCrashHandlers();
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::flush()
{
    if (m_destroyed.loadAcquire()) return;
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen())
        m_fileStream.flush();
}

QString Logger::levelToStr(LogLevel level)
{
    switch (level) {
        case E_LOG_DEBUG: return "DEBUG";
        case E_LOG_INFO:  return "INFO";
        case E_LOG_WARN:  return "WARN";
        case E_LOG_ERROR: return "ERROR";
        default:          return "UNKNOWN";
    }
}

QString Logger::colorPrefix(LogLevel level)
{
#ifdef Q_OS_WIN
    Q_UNUSED(level)
    return QString();
#else
    switch (level) {
        case E_LOG_DEBUG: return COLOR_DEBUG;
        case E_LOG_INFO:  return COLOR_INFO;
        case E_LOG_WARN:  return COLOR_WARN;
        case E_LOG_ERROR: return COLOR_ERROR;
        default:          return QString();
    }
#endif
}

QString Logger::colorSuffix()
{
#ifdef Q_OS_WIN
    return QString();
#else
    return COLOR_CLEAR;
#endif
}

QString Logger::escapeNewlines(const QString& msg)
{
    QString result;
    result.reserve(msg.length() + 16);
    const ushort kBackslash = QLatin1Char('\\').unicode();
    const ushort kLF        = QLatin1Char('\n').unicode();
    const ushort kCR        = QLatin1Char('\r').unicode();

    for (int i = 0; i < msg.length(); ++i) {
        const ushort u = msg.at(i).unicode();
        switch (u) {
        case kBackslash: result.append("\\\\"); break;
        case kLF:        result.append("\\n");  break;
        case kCR:        result.append("\\r");  break;
        default:         result.append(msg.at(i)); break;
        }
    }
    return result;
}

QString Logger::buildRunRegex(const QString& prefix, const QString& dateSegment)
{
    return QString("^%1_%2_run(\\d+)(_part\\d+)?\\.log$")
           .arg(QRegularExpression::escape(prefix))
           .arg(dateSegment);
}

void Logger::closeCurrentFile()
{
    if (m_logFile.isOpen()) {
        m_fileStream.flush();
        m_logFile.close();
    }
    m_fileStream.setDevice(nullptr);
}

bool Logger::needRollFile(const QDateTime& now)
{
    if (!m_enableFile) return false;

    const qint64 todayJd = now.date().toJulianDay();
    if (todayJd != m_curJulianDay) {
        m_curJulianDay = todayJd;
        m_curDate      = now.date().toString("yyyyMMdd");
        m_runIndex     = getMaxRunForDate(m_curDate) + 1;
        m_partIndex    = 1;
        return true;
    }

    if (!m_logFile.isOpen()) return true;

    if (m_logFile.size() >= m_maxFileSize) {
        m_partIndex++;
        return true;
    }
    return false;
}

int Logger::getMaxRunForDate(const QString& date)
{
    int maxRun = 0;
    QDir dir(m_logDir);
    QRegularExpression rx(buildRunRegex(m_filePrefix, date));
    rx.optimize();

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        QRegularExpressionMatch match = rx.match(info.fileName());
        if (match.hasMatch()) {
            int run = match.captured(1).toInt();
            if (run > maxRun) maxRun = run;
        }
    }
    return maxRun;
}

void Logger::createNewRollFile()
{
    closeCurrentFile();

    QString fileName;
    if (m_partIndex <= 1) {
        fileName = QString("%1_%2_run%3.log")
                   .arg(m_filePrefix).arg(m_curDate).arg(m_runIndex);
    } else {
        fileName = QString("%1_%2_run%3_part%4.log")
                   .arg(m_filePrefix).arg(m_curDate)
                   .arg(m_runIndex).arg(m_partIndex);
    }

    QString fullPath = QDir(m_logDir).filePath(fileName);
    m_logFile.setFileName(fullPath);

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_enableFile = false;
        m_partIndex = 1;
        writeRawStderr(QString("[Logger] Failed to open: %1").arg(fullPath));
        return;
    }

    m_fileStream.setDevice(&m_logFile);
    m_fileStream.setCodec("UTF-8");
    m_flushCounter = 0;
}

void Logger::cleanOldLogs(int keepDays)
{
    if (keepDays <= 0) return;

    QDateTime now = QDateTime::currentDateTime();
    qint64 keepMsecs = static_cast<qint64>(keepDays) * 24 * 3600 * 1000;

    QDir dir(m_logDir);
    QRegularExpression rx(buildRunRegex(m_filePrefix, QStringLiteral("\\d{8}")));
    rx.optimize();

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        if (!rx.match(info.fileName()).hasMatch()) continue;
        if (info.lastModified().msecsTo(now) > keepMsecs) {
            if (QFile::remove(info.absoluteFilePath())) {
                writeRawStderr(QString("[Logger] Removed old log: %1").arg(info.fileName()));
            } else {
                writeRawStderr(QString("[Logger] Failed to remove: %1").arg(info.fileName()));
            }
        }
    }
}

// 核心日志写入
void Logger::log(LogLevel level, const char* file, int line,
                 const char* function, const QString& msg, bool force)
{
    if (m_destroyed.loadAcquire()) return;

    QMutexLocker locker(&m_mutex);

    if (!force && (level < m_logLevel || m_logLevel == E_LOG_OFF))
        return;

    const QDateTime now = QDateTime::currentDateTime();

    // 强制日志自动初始化文件
    if (force && !m_logFile.isOpen() && !m_logDir.isEmpty()) {
        bool oldEnableFile = m_enableFile;
        m_enableFile = true;
        if (needRollFile(now)) createNewRollFile();
        m_enableFile = oldEnableFile;
    }

    if (needRollFile(now))
        createNewRollFile();

    QString timeStr   = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString shortFile = QFileInfo(file).fileName();
    QString safeMsg   = escapeNewlines(msg);
    QString logText   = QString("[%1] [%2] [%3 : %4 : %5] | %6")
                        .arg(timeStr).arg(levelToStr(level))
                        .arg(shortFile).arg(function).arg(line).arg(safeMsg);

    // 1. 文件
    if ((m_enableFile || force) && m_logFile.isOpen()) {
        m_fileStream << logText << "\n";
        if (++m_flushCounter >= FLUSH_INTERVAL || force) {
            m_fileStream.flush();
            m_flushCounter = 0;
        }
    }

    // 2. 控制台
    if (force || m_enableConsole) {
        writeConsoleLine(logText, level);
        SqzBus::Send("SQZ_LOG_DATA", logText);
    }

    // 3. 环形缓冲区
    writeCrashRing(logText);
}

// ==================== 第 1 层：安装 Qt 消息处理器 ====================
void Logger::installQtMessageHandler()
{
    if (s_qtHandlerInstalled) return;
    qInstallMessageHandler(sqzQtMessageHandler);
    s_qtHandlerInstalled = true;
}

// ==================== 第 3 层：安装崩溃处理器 ====================
void Logger::installCrashHandlers()
{
    bool expected = false;
    if (!s_crashHandlersInstalled.compare_exchange_strong(expected, true)) {
        installQtMessageHandler();
        return;
    }

    // 1. 打开崩溃日志
    if (!m_logDir.isEmpty()) {
        const QByteArray crashPath = (m_logDir + "/crash.log").toUtf8();
#ifdef Q_OS_WIN
        s_crashFd = ::_open(crashPath.constData(),
                            _O_WRONLY | _O_CREAT | _O_APPEND,
                            _S_IREAD | _S_IWRITE);
#else
        s_crashFd = ::open(crashPath.constData(),
                           O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    }

    // 2. 安装信号处理器
#ifdef Q_OS_WIN
    if (s_crashFd >= 0) {
        ::signal(SIGABRT, sqzCrashSignalHandler);
        ::signal(SIGTERM, sqzCrashSignalHandler);
        ::signal(SIGINT,  sqzCrashSignalHandler);
    }
#else
    if (s_crashFd >= 0) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sqzCrashSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESETHAND;   // 触发一次后恢复默认，防止处理器自身崩溃死循环

        ::sigaction(SIGSEGV, &sa, nullptr);
        ::sigaction(SIGABRT, &sa, nullptr);
        ::sigaction(SIGFPE,  &sa, nullptr);
        ::sigaction(SIGILL,  &sa, nullptr);
#ifdef SIGBUS
        ::sigaction(SIGBUS,  &sa, nullptr);
#endif
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT,  &sa, nullptr);
    }
#endif

    // 3. std::terminate
    std::set_terminate(sqzTerminateHandler);

    // 4. stderr 重定向
    installStderrRedirect();

    // 5. Qt 消息处理器
    installQtMessageHandler();
}

// ==================== stderr 重定向 ====================
void Logger::installStderrRedirect()
{
    bool expected = false;
    if (!g_stderrInstalled.compare_exchange_strong(expected, true)) {
        return;
    }

#ifndef Q_OS_WIN
    // 1. 保存原始 stderr（用户控制台可见性）
    g_origStderrFd = ::dup(2);
    if (g_origStderrFd < 0) {
        writeRawStderr(QStringLiteral("[Logger] dup(2) failed"));
        g_stderrInstalled = false;
        return;
    }

    // 2. 创建管道
    int pipeFd[2] = {-1, -1};
    if (::pipe(pipeFd) != 0) {
        writeRawStderr(QStringLiteral("[Logger] pipe() failed"));
        ::close(g_origStderrFd);
        g_origStderrFd = -1;
        g_stderrInstalled = false;
        return;
    }
    g_stderrPipeReadFd  = pipeFd[0];
    g_stderrPipeWriteFd = pipeFd[1];

    // 3. fd 2 指向管道写端
    if (::dup2(g_stderrPipeWriteFd, 2) < 0) {
        writeRawStderr(QStringLiteral("[Logger] dup2() failed"));
        ::close(g_stderrPipeReadFd);
        ::close(g_stderrPipeWriteFd);
        ::close(g_origStderrFd);
        g_stderrPipeReadFd  = -1;
        g_stderrPipeWriteFd = -1;
        g_origStderrFd      = -1;
        g_stderrInstalled   = false;
        return;
    }

    // 4. 关掉写端多余副本（fd 2 已指向 pipe）
    if (g_stderrPipeWriteFd != 2) {
        ::close(g_stderrPipeWriteFd);
        g_stderrPipeWriteFd = -1;
    }

    // 5. 启动读线程
    g_stderrRunning.store(true);
    g_stderrThread = std::thread([]() {
        Logger::instance().stderrReaderLoop();
    });

    writeRawStderr(QStringLiteral("[Logger] stderr redirect installed"));
#else
    // Windows 未实现（CRT assert 走 _CrtDbgReport，需要额外 hook）
    g_origStderrFd = ::_dup(2);
#endif
}

void Logger::stopStderrRedirect()
{
    if (!g_stderrRunning.load()) return;
    g_stderrRunning.store(false);

#ifndef Q_OS_WIN
    // 恢复 fd 2 → 管道写端关闭 → 读端读到 EOF → 线程退出
    if (g_origStderrFd >= 0) {
        ::dup2(g_origStderrFd, 2);
    }
    if (g_stderrPipeReadFd >= 0) {
        ::close(g_stderrPipeReadFd);
        g_stderrPipeReadFd = -1;
    }
#endif

    if (g_stderrThread.joinable()) {
        g_stderrThread.join();
    }
    g_stderrInstalled.store(false);
}

// 后台读线程：从管道读 stderr 内容，按行转发到 logExternal
void Logger::stderrReaderLoop()
{
#ifdef Q_OS_WIN
    return;
#else
    if (g_stderrPipeReadFd < 0) return;

    char buf[STDERR_BUF_SIZE];
    QByteArray lineBuffer;

    while (g_stderrRunning.load()) {
        const ssize_t n = ::read(g_stderrPipeReadFd, buf, sizeof(buf));
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }

        int start = 0;
        for (int i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                lineBuffer.append(buf + start, i - start);
                if (!lineBuffer.isEmpty()) {
                    logExternal(QString::fromUtf8(lineBuffer));
                }
                lineBuffer.clear();
                start = i + 1;
            }
        }
        if (start < n) {
            lineBuffer.append(buf + start, static_cast<int>(n) - start);
        }

        // 无换行的超大输出保护
        if (lineBuffer.size() > 64 * 1024) {
            logExternal(QString::fromUtf8(lineBuffer));
            lineBuffer.clear();
        }
    }

    if (!lineBuffer.isEmpty()) {
        logExternal(QString::fromUtf8(lineBuffer));
    }
#endif
}

// 外部捕获的 stderr 内容：只写文件 + 环形区，不重复输出到控制台
void Logger::logExternal(const QString& msg)
{
    if (m_destroyed.loadAcquire()) return;
    if (msg.isEmpty()) return;

    QMutexLocker locker(&m_mutex);

    const QDateTime now = QDateTime::currentDateTime();

    if (needRollFile(now))
        createNewRollFile();

    QString timeStr = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString safeMsg = escapeNewlines(msg);
    QString logText = QString("[%1] [%2] [%3 : %4 : %5] | %6")
                      .arg(timeStr)
                      .arg("ERROR")
                      .arg("[SYS]")
                      .arg("[stderr]")
                      .arg(0)
                      .arg(safeMsg);

    // 只写文件 + 环形区；不写控制台（避免与原始 stderr 输出重复）
    if (m_enableFile && m_logFile.isOpen()) {
        m_fileStream << logText << "\n";
        m_fileStream.flush();
    }

    writeCrashRing(logText);
}
