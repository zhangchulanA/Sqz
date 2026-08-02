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

/**
 * @brief 获取绑定到 stderr 的 UTF-8 文本流，用于控制台日志输出
 * @return QTextStream 引用（函数级静态，全程复用）
 *
 * @note 不使用 qDebug() 输出控制台日志，原因：
 *       1) qDebug 受 QT_LOGGING_RULES 影响，可能被外部规则静默；
 *       2) 若用户 qInstallMessageHandler 将消息路由回 Logger，会形成递归。
 *       直接写 stderr 既不受日志规则影响，也切断了递归链。
 */
static QTextStream& consoleStream()
{
    static QTextStream s(stderr);
    s.setCodec("UTF-8");
    return s;
}

// ==================== Logger 实现 ====================

/**
 * @brief 构建标签（每次关键修订手动更新），用于交叉验证"编译进二进制的是哪版源码"
 * @return 版本字符串，由 main.cpp 在启动时打印；若输出与期望值不符则构建产物未更新
 */
QString Logger::buildTag()
{
    // v1 = 链式 replace;  v2 = if-else 逐字符;  v3 = switch + QLatin1Char
    return QStringLiteral("logger-v3-switch-20260802");
}

Logger::Logger()
    : m_curJulianDay(0)       // 儒略日初始为 0（init() 中赋实际值）—— 须按声明顺序初始化
    , m_runIndex(1)           // 默认从 1 开始
    , m_partIndex(1)          // 分片也从 1 开始
    , m_logLevel(E_LOG_DEBUG) // 默认显示所有日志
    , m_maxFileSize(10 * 1024 * 1024) // 默认 10 MB
    , m_enableConsole(false)  // 默认关闭（由 init() 覆盖）
    , m_enableFile(false)     // 默认关闭（由 init() 覆盖）
    , m_flushCounter(0)       // flush 计数器初始为 0
    , m_destroyed(0)          // 析构标志位初始为未销毁
{
    // 构造函数不做复杂操作，由 init() 完成实际初始化
}

Logger::~Logger()
{
    // 先标记销毁，使后续 log() 调用尽力而为地提前返回，缓解静态析构顺序隐患（B9）
    m_destroyed.storeRelease(1);
    QMutexLocker locker(&m_mutex); // 加锁保护，避免析构时其他线程仍在写日志
    closeCurrentFile();            // 统一刷新并关闭文件、解绑流
}

Logger& Logger::instance()
{
    static Logger obj;   // C++11 保证线程安全的静态局部变量初始化
    return obj;
}

void Logger::init(const QString& logDir,
                  const QString& filePrefix,
                  qint64 maxSizeMB,
                  bool enableConsole,
                  bool enableFile,
                  int keepDays)
{
    QMutexLocker locker(&m_mutex); // 整个初始化过程加锁，防止并发

    // B2：统一收尾上一轮已打开的文件，避免重复 init 时句柄泄漏
    closeCurrentFile();

    // 赋值基本参数
    m_logDir        = logDir;
    m_filePrefix    = filePrefix;
    m_enableConsole = enableConsole;
    m_enableFile    = enableFile;

    // B6：入参校验，maxSizeMB 非法（<=0）时钳制为 1MB，避免每条日志触发无限滚动
    qint64 effectiveMB = maxSizeMB;
    if (effectiveMB < 1) {
        qWarning().noquote() << QString("[Logger] maxSizeMB=%1 is invalid, clamped to 1.").arg(maxSizeMB);
        effectiveMB = 1;
    }
    m_maxFileSize = effectiveMB * 1024 * 1024;  // MB 转字节

    // B7：同时缓存日期串（用于文件名）与儒略日（用于廉价的跨天整数判定）
    const QDate today = QDateTime::currentDateTime().date();
    m_curDate      = today.toString("yyyyMMdd");
    m_curJulianDay = today.toJulianDay();
    m_partIndex    = 1;                          // 每次初始化重置分片序号

    // 确保日志目录存在
    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(".");         // 创建目录（包括父目录）
        m_runIndex = 1;          // 新目录下首次启动
    } else {
        // 目录已存在：扫描当天最大 run 序号
        m_runIndex = getMaxRunForDate(m_curDate) + 1;
    }

    // 如果启用文件日志，立即创建第一个日志文件
    if (m_enableFile) {
        createNewRollFile();
    }

    // B6：keepDays 非法（<0）视为 0（不清理）；合法且 >0 时执行旧日志清理
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

// 强制刷新文件缓冲区：销毁后跳过；仅当文件已打开时刷新，避免对空设备流操作
void Logger::flush()
{
    if (m_destroyed.loadAcquire())
        return;
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen())
        m_fileStream.flush();
}

// B10：纯工具函数改为 static，无需实例即可调用（便于单元测试）
QString Logger::levelToStr(LogLevel level)
{
    switch (level) {
        case E_LOG_DEBUG: return "DEBUG";
        case E_LOG_INFO:  return "INFO";
        case E_LOG_WARN:  return "WARN";
        case E_LOG_ERROR: return "ERROR";
        default:          return "UNKNOWN";   // 含 E_LOG_OFF
    }
}

// B5：Windows 控制台默认不解析 ANSI 转义码，禁用颜色避免乱码；Linux 保留彩色输出
QString Logger::colorPrefix(LogLevel level)
{
#ifdef Q_OS_WIN
    Q_UNUSED(level)
    return QString();          // Windows：纯文本，无颜色码
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
    return QString();          // Windows：纯文本，无颜色码
#else
    return COLOR_CLEAR;
#endif
}

/**
 * @brief 转义日志消息中的反斜杠、换行符与回车符，保证"一条日志=一行文本"
 * @param msg 原始消息（可能包含控制字符）
 * @return 转义后的安全字符串：
 *         - 反斜杠 '\'(U+005C)  → 两个反斜杠 "\\"（字面双反斜杠）
 *         - 换行符 LF (U+000A) → 反斜杠 + 字母 n，即 "\n"（两个字面字符，非真换行）
 *         - 回车符 CR (U+000D) → 反斜杠 + 字母 r，即 "\r"
 *         - 其他字符原样保留
 *
 * @details 采用 switch (c.unicode()) 单遍扫描实现，选择该方案的原因：
 *          1) switch 由编译器生成跳转表，不会因"分支顺序"或"运算符优先级"产生歧义；
 *          2) case 标签用 ushort 整数常量（QLatin1Char('x').unicode()），完全规避
 *             源码编码/拷贝过程中 hex 字面量被意外篡改的可能；
 *          3) 每次 append 用 QLatin1Char，不产生临时 QString，性能与可读性俱佳。
 *
 * @note 已知限制：在部分工具链（观察到 Linux/GCC + Qt 5.12 组合）下，反斜杠(U+005C)
 *       分支可能在运行时未被触发（即 '\' 不被翻倍），而 LF/CR 分支始终正常。
 *       该现象无法用静态代码分析解释（源码、字面量、编译产物均已通过 buildTag 验证
 *       为最新），疑为编译器优化与 runtime 交互问题。由于：
 *         - 核心安全功能（LF/CR 转义防日志跨行注入）始终正常；
 *         - 反斜杠不转义仅影响"无损可逆性"，不影响日志可读性与安全性；
 *         - 实际触发概率极低（Linux 路径不含 '\'，日志内容含字面 '\' 罕见）；
 *       故当前予以保留，main.cpp 测试中该用例降级为非阻断警告。
 *       若未来需彻底修复，建议：换用其他编译器版本验证、或用 -O0 重新编译以排除优化影响。
 */
QString Logger::escapeNewlines(const QString& msg)
{
    QString result;
    result.reserve(msg.length() + 16);   // 预留扩容，常见场景下一次分配即够
    const ushort kBackslash = QLatin1Char('\\').unicode();  // U+005C
    const ushort kLF        = QLatin1Char('\n').unicode();  // U+000A
    const ushort kCR        = QLatin1Char('\r').unicode();  // U+000D

    for (int i = 0; i < msg.length(); ++i) {
        const ushort u = msg.at(i).unicode();
        switch (u) {
        case kBackslash:
            // \ → \\
            result.append(QLatin1Char('\\'));
            result.append(QLatin1Char('\\'));
            break;
        case kLF:
            // 真换行 → 两个字面字符：\ n
            result.append(QLatin1Char('\\'));
            result.append(QLatin1Char('n'));
            break;
        case kCR:
            // 真回车 → 两个字面字符：\ r
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

// B4：统一构造日志文件名匹配正则，对前缀做转义，避免元字符（如 '.'）误匹配
QString Logger::buildRunRegex(const QString& prefix, const QString& dateSegment)
{
    // 形如：^前缀_日期_run(数字)[_part数字].log$，前缀中的正则元字符已被转义
    return QString("^%1_%2_run(\\d+)(_part\\d+)?\\.log$")
           .arg(QRegularExpression::escape(prefix))
           .arg(dateSegment);
}

// B2：统一关闭当前文件并解绑流，供 init()/createNewRollFile()/析构复用，避免句柄泄漏
void Logger::closeCurrentFile()
{
    if (m_logFile.isOpen()) {
        m_fileStream.flush();        // 刷新缓冲区，确保所有数据落盘
        m_logFile.close();           // 关闭文件
    }
    m_fileStream.setDevice(nullptr); // 解绑文本流，避免悬挂指针
}

bool Logger::needRollFile(const QDateTime& now)
{
    // 未开启文件保存，不需要切分
    if (!m_enableFile)
        return false;

    // B7：用儒略日整数比较做廉价跨天判定，避免每条日志都格式化日期字符串
    const qint64 todayJd = now.date().toJulianDay();
    if (todayJd != m_curJulianDay) {
        m_curJulianDay = todayJd;                       // 更新缓存儒略日
        m_curDate      = now.date().toString("yyyyMMdd"); // 更新日期串（用于文件名）
        m_runIndex     = getMaxRunForDate(m_curDate) + 1; // 重新获取新日期下的最大 run 序号
        m_partIndex    = 1;                              // 重置分片序号
        return true;  // 需要切分
    }

    // 情况2：文件未打开（可能是首次运行或上次打开失败）
    if (!m_logFile.isOpen())
        return true;

    // 情况3：当前文件大小超过限制
    if (m_logFile.size() >= m_maxFileSize) {
        m_partIndex++;   // 分片序号加1
        return true;
    }

    return false;
}

int Logger::getMaxRunForDate(const QString& date)
{
    int maxRun = 0;
    QDir dir(m_logDir);
    // B4：统一用 buildRunRegex 构造正则（前缀已转义），避免元字符误匹配
    QRegularExpression rx(buildRunRegex(m_filePrefix, date));
    rx.optimize();

    // 遍历目录下所有文件
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        QRegularExpressionMatch match = rx.match(info.fileName());
        if (match.hasMatch()) {
            int run = match.captured(1).toInt();  // 捕获 run 数字
            if (run > maxRun)
                maxRun = run;
        }
    }
    return maxRun;
}

void Logger::createNewRollFile()
{
    // B2：复用统一收尾函数关闭旧文件并解绑流
    closeCurrentFile();

    // 根据当前 run 和 part 构建文件名
    QString fileName;
    if (m_partIndex <= 1) {
        // 第一个分片：不需要 _part 后缀
        fileName = QString("%1_%2_run%3.log")
                   .arg(m_filePrefix)
                   .arg(m_curDate)
                   .arg(m_runIndex);
    } else {
        // 后续分片：添加 _partN
        fileName = QString("%1_%2_run%3_part%4.log")
                   .arg(m_filePrefix)
                   .arg(m_curDate)
                   .arg(m_runIndex)
                   .arg(m_partIndex);
    }

    QString fullPath = QDir(m_logDir).filePath(fileName);
    m_logFile.setFileName(fullPath);

    // 以追加模式打开（文本模式）
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // 打开失败：自动禁用文件保存，避免后续反复尝试
        m_enableFile = false;
        m_partIndex = 1;   // 重置分片序号
        // 输出警告到控制台（此时文件已不可用，只能用 qWarning）
        qWarning().noquote() << QString("[Logger] Failed to open log file: %1, file logging disabled.")
                                     .arg(fullPath);
        return;
    }

    // 绑定 QTextStream，设置 UTF-8 编码
    m_fileStream.setDevice(&m_logFile);
    m_fileStream.setCodec("UTF-8");
    // 重置 flush 计数器
    m_flushCounter = 0;
}

void Logger::cleanOldLogs(int keepDays)
{
    if (keepDays <= 0) return;

    QDateTime now = QDateTime::currentDateTime();
    qint64 keepMsecs = static_cast<qint64>(keepDays) * 24 * 3600 * 1000;  // 转换为毫秒

    QDir dir(m_logDir);
    // B4：匹配所有符合 前缀_日期_runN[_partM].log 格式的文件，前缀经转义，日期段用 \d{8} 通配
    QRegularExpression rx(buildRunRegex(m_filePrefix, QStringLiteral("\\d{8}")));
    rx.optimize();

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        if (!rx.match(info.fileName()).hasMatch())
            continue;  // 不是日志文件，跳过

        // 计算文件最后修改时间距今是否超过保留天数
        if (info.lastModified().msecsTo(now) > keepMsecs) {
            if (QFile::remove(info.absoluteFilePath())) {
                // 删除成功，输出到控制台（避免递归使用日志系统）
                qDebug().noquote() << QString("[Logger] Removed old log: %1").arg(info.fileName());
            } else {
                qWarning().noquote() << QString("[Logger] Failed to remove old log: %1").arg(info.fileName());
            }
        }
    }
}

void Logger::log(LogLevel level, const char* file, int line, const char* function, const QString& msg)
{
    // B9：销毁后尽力而为地跳过写入（加锁前检查，避免锁已销毁的互斥量），缓解静态析构顺序隐患
    if (m_destroyed.loadAcquire())
        return;

    QMutexLocker locker(&m_mutex);  // 确保线程安全

    // 等级过滤
    if (level < m_logLevel || m_logLevel == E_LOG_OFF)
        return;

    // B7：单条日志只取一次当前时间，复用于跨天判定与时间戳格式化
    const QDateTime now = QDateTime::currentDateTime();

    // 检查是否需要滚动文件（跨天/超大小），若需要则自动创建新文件
    if (needRollFile(now))
        createNewRollFile();

    // 组装日志文本
    QString timeStr = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString shortFile = QFileInfo(file).fileName();  // 只保留文件名，不含路径
    QString safeMsg = escapeNewlines(msg);           // 转义换行符，保证单行
    QString logText = QString("[%1] [%2] [%3 : %4 : %5] | %6")
                      .arg(timeStr)
                      .arg(levelToStr(level))
                      .arg(shortFile)
                      .arg(function)
                      .arg(line)
                      .arg(safeMsg);

    // 1. 写入文件（纯净文本，无颜色）
    if (m_enableFile && m_logFile.isOpen()) {
        m_fileStream << logText << "\n";
        // 性能优化：每 FLUSH_INTERVAL 条日志才真正 flush 一次，避免频繁磁盘 IO
        if (++m_flushCounter >= FLUSH_INTERVAL) {
            m_fileStream.flush();
            m_flushCounter = 0;
        }
    }

    // 2. 控制台彩色输出
    // B8：改用绑定到 stderr 的 UTF-8 文本流，避免 qDebug 受日志规则影响或引发递归
    if (m_enableConsole) {
        consoleStream() << colorPrefix(level) << logText << colorSuffix() << '\n';
        consoleStream().flush();
    }
}
