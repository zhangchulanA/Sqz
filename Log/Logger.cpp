#include "Logger.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDebug>
#include <QRegularExpression>
#include <cstdio>

// ---------- 控制台 ANSI 颜色定义 ----------
#define COLOR_DEBUG  "\033[34m"   // 蓝色
#define COLOR_INFO   "\033[32m"   // 绿色
#define COLOR_WARN   "\033[33m"   // 黄色
#define COLOR_ERROR  "\033[31m"   // 红色
#define COLOR_CLEAR  "\033[0m"    // 清空颜色

// 获取绑定到 stderr 的 UTF-8 文本流，用于控制台日志输出
// 不使用 qDebug() 输出控制台日志，避免受 QT_LOGGING_RULES 影响或引发递归
static QTextStream& consoleStream()
{
    static QTextStream s(stderr);
    s.setCodec("UTF-8");
    return s;
}

// ==================== Logger 实现 ====================

// 构建标签，用于交叉验证编译进二进制的是哪版源码
QString Logger::buildTag()
{
    return QStringLiteral("logger-v3-switch-20260802");
}

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
    // 构造函数不做复杂操作，由 init() 完成实际初始化
}

Logger::~Logger()
{
    // 标记销毁，缓解静态析构顺序隐患
    m_destroyed.storeRelease(1);
    QMutexLocker locker(&m_mutex);
    closeCurrentFile();
}

Logger& Logger::instance()
{
    static Logger obj; // C++11 保证线程安全的静态局部变量初始化
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

    // 收尾上一轮已打开的文件，避免句柄泄漏
    closeCurrentFile();

    // 赋值基本参数
    m_logDir        = logDir;
    m_filePrefix    = filePrefix;
    m_enableConsole = enableConsole;
    m_enableFile    = enableFile;

    // 入参校验，maxSizeMB 非法时钳制为 1MB
    qint64 effectiveMB = maxSizeMB;
    if (effectiveMB < 1) {
        qWarning().noquote() << QString("[Logger] maxSizeMB=%1 is invalid, clamped to 1.").arg(maxSizeMB);
        effectiveMB = 1;
    }
    m_maxFileSize = effectiveMB * 1024 * 1024;

    // 缓存日期串与儒略日
    const QDate today = QDateTime::currentDateTime().date();
    m_curDate      = today.toString("yyyyMMdd");
    m_curJulianDay = today.toJulianDay();
    m_partIndex    = 1;

    // 确保日志目录存在
    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        m_runIndex = 1;
    } else {
        m_runIndex = getMaxRunForDate(m_curDate) + 1;
    }

    // 如果启用文件日志，立即创建第一个日志文件
    if (m_enableFile) {
        createNewRollFile();
    }

    // keepDays 非法时视为 0（不清理）
    int effectiveKeepDays = keepDays;
    if (effectiveKeepDays < 0) {
        qWarning().noquote() << QString("[Logger] keepDays=%1 is invalid, treated as 0.").arg(keepDays);
        effectiveKeepDays = 0;
    }
    if (effectiveKeepDays > 0) {
        cleanOldLogs(effectiveKeepDays);
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

// 强制刷新文件缓冲区
void Logger::flush()
{
    if (m_destroyed.loadAcquire())
        return;
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen())
        m_fileStream.flush();
}

// 将日志等级转换为字符串
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

// 获取控制台 ANSI 颜色前缀
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

// 获取颜色重置后缀
QString Logger::colorSuffix()
{
#ifdef Q_OS_WIN
    return QString();
#else
    return COLOR_CLEAR;
#endif
}

// 转义日志消息中的反斜杠、换行符与回车符，保证一条日志=一行文本
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
        case kBackslash:
            result.append(QLatin1Char('\\'));
            result.append(QLatin1Char('\\'));
            break;
        case kLF:
            result.append(QLatin1Char('\\'));
            result.append(QLatin1Char('n'));
            break;
        case kCR:
            result.append(QLatin1Char('\\'));
            result.append(QLatin1Char('r'));
            break;
        default:
            result.append(msg.at(i));
            break;
        }
    }
    return result;
}

// 构造日志文件名匹配正则，对前缀做转义
QString Logger::buildRunRegex(const QString& prefix, const QString& dateSegment)
{
    return QString("^%1_%2_run(\\d+)(_part\\d+)?\\.log$")
           .arg(QRegularExpression::escape(prefix))
           .arg(dateSegment);
}

// 关闭当前文件并解绑流
void Logger::closeCurrentFile()
{
    if (m_logFile.isOpen()) {
        m_fileStream.flush();
        m_logFile.close();
    }
    m_fileStream.setDevice(nullptr);
}

// 检查是否需要滚动文件
bool Logger::needRollFile(const QDateTime& now)
{
    // 未开启文件保存，不需要切分
    if (!m_enableFile)
        return false;

    // 用儒略日整数比较做跨天判定
    const qint64 todayJd = now.date().toJulianDay();
    if (todayJd != m_curJulianDay) {
        m_curJulianDay = todayJd;
        m_curDate      = now.date().toString("yyyyMMdd");
        m_runIndex     = getMaxRunForDate(m_curDate) + 1;
        m_partIndex    = 1;
        return true;
    }

    // 文件未打开
    if (!m_logFile.isOpen())
        return true;

    // 文件大小超过限制
    if (m_logFile.size() >= m_maxFileSize) {
        m_partIndex++;
        return true;
    }

    return false;
}

// 获取指定日期下最大的 run 序号
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
            if (run > maxRun)
                maxRun = run;
        }
    }
    return maxRun;
}

// 创建下一个滚动日志文件
void Logger::createNewRollFile()
{
    closeCurrentFile();

    QString fileName;
    if (m_partIndex <= 1) {
        fileName = QString("%1_%2_run%3.log")
                   .arg(m_filePrefix)
                   .arg(m_curDate)
                   .arg(m_runIndex);
    } else {
        fileName = QString("%1_%2_run%3_part%4.log")
                   .arg(m_filePrefix)
                   .arg(m_curDate)
                   .arg(m_runIndex)
                   .arg(m_partIndex);
    }

    QString fullPath = QDir(m_logDir).filePath(fileName);
    m_logFile.setFileName(fullPath);

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_enableFile = false;
        m_partIndex = 1;
        qWarning().noquote() << QString("[Logger] Failed to open log file: %1, file logging disabled.")
                                     .arg(fullPath);
        return;
    }

    m_fileStream.setDevice(&m_logFile);
    m_fileStream.setCodec("UTF-8");
    m_flushCounter = 0;
}

// 清理超过保留天数的旧日志文件
void Logger::cleanOldLogs(int keepDays)
{
    if (keepDays <= 0) return;

    QDateTime now = QDateTime::currentDateTime();
    qint64 keepMsecs = static_cast<qint64>(keepDays) * 24 * 3600 * 1000;

    QDir dir(m_logDir);
    QRegularExpression rx(buildRunRegex(m_filePrefix, QStringLiteral("\\d{8}")));
    rx.optimize();

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        if (!rx.match(info.fileName()).hasMatch())
            continue;

        if (info.lastModified().msecsTo(now) > keepMsecs) {
            if (QFile::remove(info.absoluteFilePath())) {
                qDebug().noquote() << QString("[Logger] Removed old log: %1").arg(info.fileName());
            } else {
                qWarning().noquote() << QString("[Logger] Failed to remove old log: %1").arg(info.fileName());
            }
        }
    }
}

// 核心日志写入接口
void Logger::log(LogLevel level, const char* file, int line, const char* function, const QString& msg, bool force)
{
    // 销毁后尽力而为地跳过写入，避免锁已销毁的互斥量
    if (m_destroyed.loadAcquire())
        return;

    QMutexLocker locker(&m_mutex);

    // 等级过滤：强制日志不受等级过滤影响
    if (!force && (level < m_logLevel || m_logLevel == E_LOG_OFF))
        return;

    // 单条日志只取一次当前时间
    const QDateTime now = QDateTime::currentDateTime();

    // 强制日志：自动初始化文件（如果文件未打开且目录不为空）
    if (force && !m_logFile.isOpen() && !m_logDir.isEmpty()) {
        // 保存原状态
        bool oldEnableFile = m_enableFile;
        // 临时启用文件功能
        m_enableFile = true;
        // 创建文件
        if (needRollFile(now)) {
            createNewRollFile();
        }
        // 恢复原状态（但文件已打开，后续可以继续使用）
        m_enableFile = oldEnableFile;
    }

    // 检查是否需要滚动文件（跨天/超大小）
    if (needRollFile(now))
        createNewRollFile();

    // 组装日志文本
    QString timeStr = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString shortFile = QFileInfo(file).fileName();
    QString safeMsg = escapeNewlines(msg);
    QString logText = QString("[%1] [%2] [%3 : %4 : %5] | %6")
                      .arg(timeStr)
                      .arg(levelToStr(level))
                      .arg(shortFile)
                      .arg(function)
                      .arg(line)
                      .arg(safeMsg);

    // 1. 写入文件
    // 强制日志忽略 m_enableFile 开关，如果文件已打开则强制写入
    if ((m_enableFile || force) && m_logFile.isOpen()) {
        m_fileStream << logText << "\n";
        // 强制日志立即 flush，确保重要数据不丢失
        if (++m_flushCounter >= FLUSH_INTERVAL || force) {
            m_fileStream.flush();
            m_flushCounter = 0;
        }
    }

    // 2. 控制台输出
    // 强制日志忽略 m_enableConsole 开关，强制输出到控制台
    if (force || m_enableConsole) {
        consoleStream() << colorPrefix(level) << logText << colorSuffix() << '\n';
        consoleStream().flush();
    }
}
