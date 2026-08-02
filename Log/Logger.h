#ifndef Logger_H
#define Logger_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QAtomicInt>

/**
 * @brief 日志等级枚举
 *
 * 定义了5个日志级别，数值越小优先级越高。
 * 可以通过 setLogLevel() 设置全局过滤等级，低于该等级的日志不会被输出。
 */
enum LogLevel {
    E_LOG_DEBUG = 0,   ///< 调试信息，最详细
    E_LOG_INFO  = 1,   ///< 一般信息
    E_LOG_WARN  = 2,   ///< 警告信息
    E_LOG_ERROR = 3,   ///< 错误信息
    E_LOG_OFF   = 4    ///< 关闭所有日志输出
};

/**
 * @class Logger
 * @brief 线程安全、支持滚动切割与自动清理的日志类（单例模式）
 *
 * @details 主要特性：
 * - **单例模式**：全局唯一实例，通过 `instance()` 访问。
 * - **多级别过滤**：支持 DEBUG/INFO/WARN/ERROR/OFF，可动态设置最低输出等级。
 * - **双通道输出**：可同时或单独输出到控制台（带 ANSI 颜色）和本地文件。
 * - **自动滚动切割**：
 *   - 按单文件最大大小（MB）自动分片（part1, part2...）。
 *   - 按日期自动切换新文件（跨天时 run 序号自动递增）。
 *   - 支持同一程序多次运行：每次启动生成新的 `runN` 序号。
 * - **自动清理**：可配置保留最近 N 天的日志文件，防止磁盘占满。
 * - **流式宏接口**：提供 `logdebug` / `loginfo` / `logwarn` / `logerror` 宏，支持 `<<` 语法，便捷高效。
 * - **高性能**：减少文件 flush 频率（每64条日志 flush 一次），预编译正则表达式。
 * - **健壮性**：
 *   - 日志消息中的换行符会被转义，保证每条日志为单行。
 *   - 文件打开失败时自动降级为仅控制台输出。
 *   - 跨天时重新扫描已有文件，避免序号冲突。
 *
 * @note 跨平台支持：Linux 下控制台彩色输出（ANSI 转义码）；Windows 下为纯文本（避免控制台 ANSI 乱码）。
 *
 * @par 使用示例：
 * @code
 * // 初始化：日志目录 ./logs，文件前缀 app，单文件5MB，保留7天
 * Logger::instance().init("./logs", "app", 5, true, true, 7);
 * Logger::instance().setLogLevel(E_LOG_DEBUG);
 *
 * logdebug << "Debug value: " << 42;
 * loginfo  << "Application started";
 * logwarn  << "Disk usage: " << 85 << "%";
 * logerror << "Failed to open file: " << "/tmp/test.txt";
 * @endcode
 */
class Logger
{
public:
    /**
     * @brief 获取单例实例
     * @return Logger& 全局唯一的日志对象
     */
    static Logger& instance();

    /**
     * @brief 初始化日志系统
     * @param logDir        日志保存目录（自动创建）
     * @param filePrefix    日志文件名前缀（例如 "app" -> app_20260531_run1.log）
     * @param maxSizeMB     单个日志文件最大大小(MB)，超出自动分片（part）
     * @param enableConsole 是否开启控制台彩色输出（ANSI颜色）
     * @param enableFile    是否开启本地文件保存
     * @param keepDays      日志文件保留天数（超出后自动删除），0 表示不删除
     *
     * @note 必须在任何日志输出前调用一次。重复调用会重新初始化，清空原有状态。
     * @note 如果 `enableFile` 为 true 但无法创建日志目录或文件，会自动禁用文件保存。
     */
    void init(const QString& logDir,
              const QString& filePrefix,
              qint64 maxSizeMB     = 10,
              bool   enableConsole = true,
              bool   enableFile    = false,
              int    keepDays      = 7);

    /**
     * @brief 设置全局日志最低输出等级
     * @param level 低于此等级的日志将被忽略
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 强制刷新文件缓冲区，确保已写入的日志落盘
     * @note 内部加锁，线程安全。供关键节点（如程序异常前、测试断言前）调用，
     *       保证数据持久化。文件未打开时为空操作。
     */
    void flush();

    /**
     * @brief 核心日志写入接口（通常由 LoggerStream 宏调用）
     * @param level     日志等级
     * @param file      源文件名（__FILE__）
     * @param line      行号（__LINE__）
     * @param function  函数名（__FUNCTION__）
     * @param msg       格式化后的日志内容
     *
     * @note 此函数内部已加锁，线程安全。
     * @note 会自动判断是否需要滚动文件，并执行文件切换。
     */
    void log(LogLevel level, const char* file, int line, const char* function, const QString& msg);

    // ---------- 纯工具函数（static，不依赖实例，便于单元测试） ----------
    /**
     * @brief Logger.cpp 的构建标签，用于验证构建机编译的是哪版源码
     * @return 形如 "logger-v3-switch-20260802" 的版本字符串
     *
     * @note main.cpp 在启动时打印该字符串；若实际输出不是最新 tag，
     *       即可断定构建使用了旧 .o/旧源码，需强制清理重建。
     */
    static QString buildTag();

    /**
     * @brief 将日志等级转换为字符串
     * @param level 日志等级
     * @return 对应的字符串（"DEBUG"/"INFO"/"WARN"/"ERROR"/"UNKNOWN"）
     */
    static QString levelToStr(LogLevel level);

    /**
     * @brief 获取控制台 ANSI 颜色前缀
     * @param level 日志等级
     * @return 对应的 ANSI 转义码（如 "\033[34m"）；Windows 下返回空串（禁用颜色）
     */
    static QString colorPrefix(LogLevel level);

    /**
     * @brief 获取颜色重置后缀
     * @return ANSI 重置码 "\033[0m"；Windows 下返回空串（禁用颜色）
     */
    static QString colorSuffix();

    /**
     * @brief 转义日志消息中的换行符和回车符
     * @param msg 原始消息
     * @return 将 \n 转为 "\\n"，\r 转为 "\\r"，保证单行输出
     */
    static QString escapeNewlines(const QString& msg);

private:
    // 构造函数与析构函数私有（单例模式）
    Logger();
    ~Logger();

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ---------- 内部辅助函数 ----------
    /**
     * @brief 检查是否需要滚动文件（跨天 / 超大小 / 文件未打开）
     * @param now 当前时间（由调用方传入，避免每条日志重复系统调用）
     * @return true 表示需要创建新文件
     * @note 若跨天，会自动更新 m_curDate、m_curJulianDay 与 m_runIndex。
     *       使用儒略日整数比较做廉价跨天判定，避免每条日志格式化日期字符串。
     */
    bool needRollFile(const QDateTime& now);

    /**
     * @brief 创建下一个滚动日志文件
     * @note 文件名格式：前缀_日期_runN[_partM].log
     *       如果文件打开失败，会自动禁用文件输出并打印警告。
     */
    void createNewRollFile();

    /**
     * @brief 关闭当前打开的日志文件并解绑文本流（统一资源收尾）
     * @note 内部判断是否已打开，避免重复关闭；供 init()/createNewRollFile()/析构复用，
     *       防止重复 init 时句柄泄漏。
     */
    void closeCurrentFile();

    /**
     * @brief 获取指定日期下最大的 run 序号
     * @param date 日期字符串（yyyyMMdd）
     * @return 最大 run 值（若无匹配文件则返回 0）
     */
    int getMaxRunForDate(const QString& date);

    /**
     * @brief 构造匹配日志文件名的正则表达式（对前缀做正则转义）
     * @param prefix       文件名前缀（自动转义元字符，避免 "." 等误匹配）
     * @param dateSegment  日期段字符串，如 "20260801" 或通配 "\\d{8}"
     * @return 形如 "^前缀_日期_run(\\d+)(_part\\d+)?\\.log$" 的正则字符串
     */
    static QString buildRunRegex(const QString& prefix, const QString& dateSegment);

    /**
     * @brief 清理超过保留天数的旧日志文件
     * @param keepDays 保留天数（>0 才执行清理）
     * @note 仅删除符合命名规则（前缀_日期_runN[_partM].log）的文件。
     */
    void cleanOldLogs(int keepDays);

    // ---------- 成员变量 ----------
    QMutex      m_mutex;           ///< 互斥锁，保证线程安全（所有公共函数及内部关键操作均加锁）
    QString     m_logDir;          ///< 日志文件存储目录
    QString     m_filePrefix;      ///< 日志文件名前缀（例如 "myapp"）
    QString     m_curDate;         ///< 当前日志日期（yyyyMMdd），用于文件命名
    qint64      m_curJulianDay;    ///< 当前日期的儒略日，用于廉价的跨天整数判定（避免每条日志格式化日期串）
    int         m_runIndex;        ///< 当天程序启动序号（从1开始，每次启动递增）
    int         m_partIndex;       ///< 单次运行内分片序号（从1开始，文件超大小后递增）

    QFile       m_logFile;         ///< 当前打开的日志文件对象
    QTextStream m_fileStream;      ///< 文件流，用于写入文件（UTF-8编码）
    LogLevel    m_logLevel;        ///< 全局日志过滤等级（低于此等级不输出）
    qint64      m_maxFileSize;     ///< 单日志文件最大字节数（从 MB 转换而来）
    bool        m_enableConsole;   ///< 控制台输出开关
    bool        m_enableFile;      ///< 文件保存开关（若文件打开失败会自动置为 false）

    int         m_flushCounter;    ///< 文件 flush 计数器（用于减少频繁 flush）
    static const int FLUSH_INTERVAL = 64; ///< 每 64 条日志执行一次 flush

    QAtomicInt  m_destroyed;       ///< 析构标志位：销毁前置 1，log() 入口尽力而为地跳过写入，缓解静态析构顺序隐患

    // 允许测试类访问私有成员进行白盒测试（仅在测试翻译单元中定义该类时生效）
    friend class LoggerTest;
};

// ==================== 流式宏接口 ====================
/**
 * @def logdebug
 * @brief 输出 DEBUG 级别日志，用法同 qDebug() << ...
 */
#define logdebug  LoggerStream(E_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__)

/**
 * @def loginfo
 * @brief 输出 INFO 级别日志
 */
#define loginfo   LoggerStream(E_LOG_INFO,  __FILE__, __FUNCTION__, __LINE__)

/**
 * @def logwarn
 * @brief 输出 WARN 级别日志
 */
#define logwarn   LoggerStream(E_LOG_WARN,  __FILE__, __FUNCTION__, __LINE__)

/**
 * @def logerror
 * @brief 输出 ERROR 级别日志
 */
#define logerror  LoggerStream(E_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__)

/**
 * @class LoggerStream
 * @brief 流式日志临时对象，用于支持 `<<` 语法
 *
 * @details 当临时对象析构时，自动将缓冲区内容提交给 Logger 单例。
 * 内部使用 `QDebug` 并调用 `noquote()` 来避免自动添加引号，同时保持对 Qt 所有类型的完美支持。
 *
 * @note 不要直接使用此类，请使用上述四个宏。
 */
class LoggerStream
{
public:
    /**
     * @brief 构造函数：记录日志等级、文件位置，并初始化 QDebug 缓冲区
     * @param lvl       日志等级
     * @param file      源文件名（由宏传入 __FILE__）
     * @param function  函数名（由宏传入 __FUNCTION__）
     * @param line      行号（由宏传入 __LINE__）
     */
    explicit LoggerStream(LogLevel lvl, const char* file, const char* function, int line)
        : m_level(lvl)
        , m_file(file)
        , m_line(line)
        , m_function(function)
        , m_debug(&m_buffer)
    {
        m_debug.noquote();   // 关键：禁止 QDebug 自动添加引号，避免 "hello" 变成 "\"hello\""
    }

    /**
     * @brief 析构函数：将缓冲区内容提交给 Logger
     */
    ~LoggerStream()
    {
        QString content = m_buffer.trimmed();  // 去除首尾空白（通常由末尾换行造成）
        Logger::instance().log(m_level, m_file, m_line, m_function, content);
    }

    /**
     * @brief 通用模板：支持所有可通过 QDebug 输出的类型
     * @param val 要输出的值（整数、浮点、字符串、容器等）
     * @return 自身引用，支持链式调用
     */
    template<typename T>
    LoggerStream& operator<<(const T& val)
    {
        m_debug << val;
        return *this;
    }

    /**
     * @brief 忽略 QTextStream 操纵符（如 endl），避免编译错误
     * @param manip 操纵符（不会被实际处理）
     * @return 自身引用
     */
    LoggerStream& operator<<(QTextStreamFunction /*manip*/)
    {
        return *this;
    }

private:
    LogLevel     m_level;      ///< 本条日志的等级
    const char*  m_file;       ///< 源文件名指针
    int          m_line;       ///< 行号
    const char*  m_function;   ///< 函数名指针
    QString      m_buffer;     ///< 内部字符串缓冲区
    QDebug       m_debug;      ///< QDebug 对象，负责格式化并写入缓冲区
};

#endif // Logger_H
