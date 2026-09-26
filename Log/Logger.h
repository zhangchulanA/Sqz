#ifndef Logger_H
#define Logger_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QAtomicInt>
#include <atomic>
#include <cstring>
#include <thread>


#define llog (qDebug()<<"["<<__LINE__<<__FUNCTION__<<"]")

inline QString LogData(const QByteArray& data){
    QString text;
    for(int i=0;i<data.size();i++){
        text += QString::number((uchar)data[i],16)+',';
    }
    return  text;
}

// 日志等级枚举
enum LogLevel {
    E_LOG_DEBUG = 0,   // 调试信息，最详细
    E_LOG_INFO  = 1,   // 一般信息
    E_LOG_WARN  = 2,   // 警告信息
    E_LOG_ERROR = 3,   // 错误信息
    E_LOG_OFF   = 4    // 关闭所有日志输出
};

// 线程安全日志类（单例）。
//
// 崩溃捕获四层机制：
//   1. Qt 消息处理器   —— qDebug/qWarning/qCritical/qFatal 统一走 Logger
//   2. 崩溃环形缓冲区 —— 无锁保存最近 N 条日志，崩溃时可安全 dump
//   3. POSIX 信号处理器 + stderr 重定向 —— 捕获 SIGSEGV/SIGABRT 等，
//                        并转发 glibc/assert/libstdc++ 打到 stderr 的信息
//   4. std::terminate  —— 捕获未处理 C++ 异常
//
// 使用顺序（main 中）：
//   Logger::installQtMessageHandler();   // 尽早
//   ... QApplication app ...
//   Logger::instance().init(...);        // 内部装崩溃处理器
class Logger
{
public:
    static Logger& instance();

    // logDir: 日志目录（自动创建）
    // filePrefix: 日志文件名前缀
    // enableConsole: 控制台彩色输出
    // enableFile: 本地文件保存
    // maxSizeMB: 单文件分片大小（MB）
    // keepDays: 保留天数，0 表示不删
    void init(const QString& logDir,
              const QString& filePrefix,
              bool   enableConsole = true,
              bool   enableFile    = false,
              qint64 maxSizeMB     = 10,
              int    keepDays      = 7);

    void setLogLevel(LogLevel level);
    void flush();

    // 核心写入接口（由 LoggerStream 宏调用）
    void log(LogLevel level, const char* file, int line, const char* function,
             const QString& msg, bool force = false);

    // 安装 Qt 消息处理器，建议在 QApplication 之前调用。幂等。
    static void installQtMessageHandler();

    // 安装崩溃处理器（信号 + stderr 重定向 + std::terminate）。幂等。
    // 由 init() 内部自动调用，也可在 main 中提前手动调用。
    void installCrashHandlers();

    // 安装 stderr 重定向（内部由 installCrashHandlers 调用）。幂等。
    void installStderrRedirect();

    // 停止 stderr 重定向线程（析构时自动调用）
    void stopStderrRedirect();

    // 外部捕获的信息（来自 stderr 管道）：只写文件 + 环形区，不重复输出到控制台
    void logExternal(const QString& msg);

    // ---------- 纯工具函数（static） ----------
    static QString levelToStr(LogLevel level);
    static QString colorPrefix(LogLevel level);
    static QString colorSuffix();
    static QString escapeNewlines(const QString& msg);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ---------- 内部辅助 ----------
    bool needRollFile(const QDateTime& now);
    void createNewRollFile();
    void closeCurrentFile();
    int  getMaxRunForDate(const QString& date);
    static QString buildRunRegex(const QString& prefix, const QString& dateSegment);
    void cleanOldLogs(int keepDays);

    // 写入崩溃环形缓冲区（无锁）
    static void writeCrashRing(const QString& logText);

    // 直接写原始 stderr，避免与 qInstallMessageHandler 递归
    static void writeRawStderr(const QString& msg);

    // stderr 管道读线程主循环
    void stderrReaderLoop();

    // ---------- 成员变量 ----------
    QMutex      m_mutex;
    QString     m_logDir;
    QString     m_filePrefix;
    QString     m_curDate;
    qint64      m_curJulianDay;
    int         m_runIndex;
    int         m_partIndex;

    QFile       m_logFile;
    QTextStream m_fileStream;
    LogLevel    m_logLevel;
    qint64      m_maxFileSize;
    bool        m_enableConsole;
    bool        m_enableFile;

    int         m_flushCounter;
    static const int FLUSH_INTERVAL = 1;

    QAtomicInt  m_destroyed;

public:

    // ---------- 崩溃捕获 ----------
    static const int CRASH_RING_SIZE = 256;   // 环形槽位
    static const int CRASH_LINE_MAX  = 1024;  // 单条最大字节

    struct CrashSlot {
        char buf[CRASH_LINE_MAX];
        int  len;
        CrashSlot() : len(0) { buf[0] = '\0'; }
    };

    static CrashSlot         s_ring[CRASH_RING_SIZE];
    static std::atomic<int>  s_ringHead;
    static int               s_crashFd;
    static std::atomic<bool> s_crashHandlersInstalled;
    static bool              s_qtHandlerInstalled;
};

// ==================== 流式宏 ====================
#define logdebug  LoggerStream(E_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__)
#define loginfo   LoggerStream(E_LOG_INFO,  __FILE__, __FUNCTION__, __LINE__)
#define logwarn   LoggerStream(E_LOG_WARN,  __FILE__, __FUNCTION__, __LINE__)
#define logerror  LoggerStream(E_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__)

#define fdebug  LoggerStream(E_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, true)
#define finfo   LoggerStream(E_LOG_INFO,  __FILE__, __FUNCTION__, __LINE__, true)
#define fwarn   LoggerStream(E_LOG_WARN,  __FILE__, __FUNCTION__, __LINE__, true)
#define ferror  LoggerStream(E_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, true)

// 流式日志临时对象，支持 << 语法
class LoggerStream
{
public:
    explicit LoggerStream(LogLevel lvl, const char* file, const char* function, int line, bool force = false)
        : m_level(lvl)
        , m_file(file)
        , m_line(line)
        , m_function(function)
        , m_force(force)
        , m_debug(&m_buffer)
    {
        m_debug.noquote();
    }

    ~LoggerStream()
    {
        QString content = m_buffer.trimmed();
        Logger::instance().log(m_level, m_file, m_line, m_function, content, m_force);
    }

    template<typename T>
    LoggerStream& operator<<(const T& val)
    {
        m_debug << val;
        return *this;
    }

    LoggerStream& operator<<(QTextStreamFunction /*manip*/) { return *this; }

private:
    LogLevel     m_level;
    const char*  m_file;
    int          m_line;
    const char*  m_function;
    bool         m_force;
    QString      m_buffer;
    QDebug       m_debug;
};

#endif // Logger_H
