#ifndef Logger_H
#define Logger_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QAtomicInt>

// 日志等级枚举
enum LogLevel {
    E_LOG_DEBUG = 0,   // 调试信息，最详细
    E_LOG_INFO  = 1,   // 一般信息
    E_LOG_WARN  = 2,   // 警告信息
    E_LOG_ERROR = 3,   // 错误信息
    E_LOG_OFF   = 4    // 关闭所有日志输出
};

// 线程安全、支持滚动切割与自动清理的日志类（单例模式）
class Logger
{
public:
    // 获取单例实例
    static Logger& instance();

    // 初始化日志系统
    // logDir: 日志保存目录（自动创建）
    // filePrefix: 日志文件名前缀
    // maxSizeMB: 单个日志文件最大大小(MB)，超出自动分片，默认10MB
    // enableConsole: 是否开启控制台彩色输出，默认true
    // enableFile: 是否开启本地文件保存，默认false
    // keepDays: 日志文件保留天数，0表示不删除，默认7天
    // 注意：重复调用会重新初始化
    void init(const QString& logDir,
              const QString& filePrefix,
              bool   enableConsole = true,
              bool   enableFile    = false,
              qint64 maxSizeMB     = 10,
              int    keepDays      = 7);

    // 设置全局日志最低输出等级，低于此等级的日志将被忽略
    void setLogLevel(LogLevel level);

    // 强制刷新文件缓冲区，确保已写入的日志落盘
    // 文件未打开时为空操作，内部加锁线程安全
    void flush();

    // 核心日志写入接口（通常由 LoggerStream 宏调用）
    // level: 日志等级
    // file: 源文件名（__FILE__）
    // line: 行号（__LINE__）
    // function: 函数名（__FUNCTION__）
    // msg: 格式化后的日志内容
    // force: 是否强制输出（忽略控制台/文件开关和等级过滤）
    // 注意：force=true 时，如果文件未打开且日志目录不为空，会自动创建文件
    void log(LogLevel level, const char* file, int line, const char* function, const QString& msg, bool force = false);

    // ---------- 纯工具函数（static，不依赖实例） ----------

    // 获取构建标签，用于验证编译的是哪版源码
    static QString buildTag();

    // 将日志等级转换为字符串
    static QString levelToStr(LogLevel level);

    // 获取控制台 ANSI 颜色前缀，Windows下返回空串（禁用颜色）
    static QString colorPrefix(LogLevel level);

    // 获取颜色重置后缀，Windows下返回空串
    static QString colorSuffix();

    // 转义日志消息中的换行符和回车符，保证单行输出
    static QString escapeNewlines(const QString& msg);

private:
    // 构造函数与析构函数私有（单例模式）
    Logger();
    ~Logger();

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ---------- 内部辅助函数 ----------

    // 检查是否需要滚动文件（跨天/超大小/文件未打开）
    // now: 当前时间
    // 返回true表示需要创建新文件
    bool needRollFile(const QDateTime& now);

    // 创建下一个滚动日志文件
    // 文件名格式：前缀_日期_runN[_partM].log
    // 打开失败时自动禁用文件输出
    void createNewRollFile();

    // 关闭当前打开的日志文件并解绑文本流
    // 供 init()/createNewRollFile()/析构复用，防止句柄泄漏
    void closeCurrentFile();

    // 获取指定日期下最大的 run 序号
    int getMaxRunForDate(const QString& date);

    // 构造匹配日志文件名的正则表达式（对前缀做正则转义）
    static QString buildRunRegex(const QString& prefix, const QString& dateSegment);

    // 清理超过保留天数的旧日志文件
    void cleanOldLogs(int keepDays);

    // ---------- 成员变量 ----------

    QMutex      m_mutex;           // 互斥锁，保证线程安全
    QString     m_logDir;          // 日志文件存储目录
    QString     m_filePrefix;      // 日志文件名前缀
    QString     m_curDate;         // 当前日志日期（yyyyMMdd）
    qint64      m_curJulianDay;    // 当前日期的儒略日，用于跨天判定
    int         m_runIndex;        // 当天程序启动序号
    int         m_partIndex;       // 单次运行内分片序号

    QFile       m_logFile;         // 当前打开的日志文件对象
    QTextStream m_fileStream;      // 文件流，UTF-8编码
    LogLevel    m_logLevel;        // 全局日志过滤等级
    qint64      m_maxFileSize;     // 单日志文件最大字节数
    bool        m_enableConsole;   // 控制台输出开关
    bool        m_enableFile;      // 文件保存开关

    int         m_flushCounter;    // 文件 flush 计数器
    static const int FLUSH_INTERVAL = 1; // 每64条日志执行一次flush

    QAtomicInt  m_destroyed;       // 析构标志位

    friend class LoggerTest;
};

// ==================== 流式宏接口 ====================

// 普通日志宏（受 enableConsole 和 enableFile 控制，受等级过滤影响）
#define logdebug  LoggerStream(E_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__)
#define loginfo   LoggerStream(E_LOG_INFO,  __FILE__, __FUNCTION__, __LINE__)
#define logwarn   LoggerStream(E_LOG_WARN,  __FILE__, __FUNCTION__, __LINE__)
#define logerror  LoggerStream(E_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__)

// 强制日志宏（忽略 enableConsole、enableFile 和等级过滤，自动初始化文件）
#define fdebug  LoggerStream(E_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, true)
#define finfo   LoggerStream(E_LOG_INFO,  __FILE__, __FUNCTION__, __LINE__, true)
#define fwarn   LoggerStream(E_LOG_WARN,  __FILE__, __FUNCTION__, __LINE__, true)
#define ferror  LoggerStream(E_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, true)

// 流式日志临时对象，用于支持 << 语法
class LoggerStream
{
public:
    // 构造函数
    // lvl: 日志等级
    // file: 源文件名
    // function: 函数名
    // line: 行号
    // force: 是否强制输出
    explicit LoggerStream(LogLevel lvl, const char* file, const char* function, int line, bool force = false)
        : m_level(lvl)
        , m_file(file)
        , m_line(line)
        , m_function(function)
        , m_force(force)
        , m_debug(&m_buffer)
    {
        m_debug.noquote(); // 禁止 QDebug 自动添加引号
    }

    // 析构函数：将缓冲区内容提交给 Logger
    ~LoggerStream()
    {
        QString content = m_buffer.trimmed();
        Logger::instance().log(m_level, m_file, m_line, m_function, content, m_force);
    }

    // 通用模板：支持所有可通过 QDebug 输出的类型
    template<typename T>
    LoggerStream& operator<<(const T& val)
    {
        m_debug << val;
        return *this;
    }

    // 忽略 QTextStream 操纵符（如 endl），避免编译错误
    LoggerStream& operator<<(QTextStreamFunction /*manip*/)
    {
        return *this;
    }

private:
    LogLevel     m_level;      // 本条日志的等级
    const char*  m_file;       // 源文件名指针
    int          m_line;       // 行号
    const char*  m_function;   // 函数名指针
    bool         m_force;      // 强制输出标志
    QString      m_buffer;     // 内部字符串缓冲区
    QDebug       m_debug;      // QDebug 对象
};

#endif // Logger_H
