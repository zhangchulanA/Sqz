#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QDate>
#include <QThread>
#include <QAtomicInt>
#include <QRegularExpression>
#include <QStringList>

// 包含被测头文件（其中声明了 friend class LoggerTest;，故可白盒访问私有成员）
#include "Logger.h"

// ============================================================================
// 辅助：生成唯一临时目录，并在 cleanupTestCase 中统一清理
// ============================================================================
static QAtomicInt g_dirSeq(0);

// 生成一个不存在的唯一临时目录路径并创建
static QString makeTempDir(const QString& tag, QStringList& created)
{
    const QString path = QDir::tempPath()
                         + QStringLiteral("/loggertest_%1_%2_%3")
                           .arg(tag)
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(g_dirSeq.fetchAndAddRelaxed(1));
    QDir().mkpath(path);   // 创建目录（含父目录）
    created << path;
    return path;
}

// 读取目录下所有匹配 glob 的文件的总行数（按 '\n' 计数）
static int totalLineCount(const QString& dir, const QString& glob)
{
    int lines = 0;
    const QStringList names = QDir(dir).entryList(QStringList() << glob, QDir::Files);
    for (const QString& name : names) {
        QFile f(dir + QDir::separator() + name);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            lines += ts.readAll().count(QLatin1Char('\n'));
        }
    }
    return lines;
}

/**
 * @class LoggerTest
 * @brief Logger 模块单元测试与集成测试集合
 *
 * @details 覆盖范围：
 * - 单元测试：levelToStr / escapeNewlines / colorPrefix / buildRunRegex 等纯函数；
 * - 集成测试：文件创建、run 序号递增、入参校验、按大小滚动、重复 init 关闭旧文件、
 *   旧日志清理、含元字符前缀、跨天滚动、多线程并发、单行转义、等级保留。
 *
 * 通过 Logger.h 中的 `friend class LoggerTest;` 进行白盒访问。
 */
class LoggerTest : public QObject
{
    Q_OBJECT
private:
    QStringList m_tempDirs;   // 本测试创建的所有临时目录，结束时统一清理

    // 便捷：当前日期串（yyyyMMdd），用于断言文件名
    static QString todayStr() { return QDateTime::currentDateTime().date().toString("yyyyMMdd"); }

private slots:
    // ---------- 测试前置/后置 ----------
    void cleanupTestCase()
    {
        // 关闭可能仍打开的日志文件，便于在 Windows 上删除测试目录
        Logger::instance().init(QDir::tempPath(), QStringLiteral("cleanup"), 5, false, false, 0);
        // 用 const 引用遍历，避免隐式共享 detach（兼容 Qt 5.12 / C++11，无需 qAsConst）
        const QStringList& dirs = m_tempDirs;
        for (const QString& d : dirs) {
            QDir(d).removeRecursively();
        }
    }

    // ==================== 单元测试 ====================

    // 验证日志等级到字符串的映射（含 E_LOG_OFF → "UNKNOWN"）
    void testLevelToStr()
    {
        QCOMPARE(Logger::levelToStr(E_LOG_DEBUG), QStringLiteral("DEBUG"));
        QCOMPARE(Logger::levelToStr(E_LOG_INFO),  QStringLiteral("INFO"));
        QCOMPARE(Logger::levelToStr(E_LOG_WARN),  QStringLiteral("WARN"));
        QCOMPARE(Logger::levelToStr(E_LOG_ERROR), QStringLiteral("ERROR"));
        QCOMPARE(Logger::levelToStr(E_LOG_OFF),   QStringLiteral("UNKNOWN"));
    }

    // 验证换行/回车/反斜杠转义，且实际换行与字面 "\n" 输入可区分（无歧义）
    void testEscapeNewlines()
    {
        // 实际换行符 -> 字面 "\n"（两个字符：反斜杠 + n）
        QCOMPARE(Logger::escapeNewlines(QStringLiteral("a\nb")),  QStringLiteral("a\\nb"));
        // 实际回车符 -> 字面 "\r"
        QCOMPARE(Logger::escapeNewlines(QStringLiteral("a\rb")),  QStringLiteral("a\\rb"));
        // 反斜杠先转义为双反斜杠
        QCOMPARE(Logger::escapeNewlines(QStringLiteral("a\\b")),  QStringLiteral("a\\\\b"));
        // 无歧义：实际换行的转义结果 ≠ 字面反斜杠+n 的转义结果
        QVERIFY(Logger::escapeNewlines(QStringLiteral("a\nb"))
                != Logger::escapeNewlines(QStringLiteral("a\\nb")));
        // 空串保持空
        QVERIFY(Logger::escapeNewlines(QString()).isEmpty());
    }

    // 验证颜色前缀/后缀：Windows 为空，Linux 为 ANSI 码
    void testColorPrefix()
    {
#ifdef Q_OS_WIN
        QVERIFY(Logger::colorPrefix(E_LOG_DEBUG).isEmpty());
        QVERIFY(Logger::colorPrefix(E_LOG_ERROR).isEmpty());
        QVERIFY(Logger::colorSuffix().isEmpty());
#else
        QCOMPARE(Logger::colorPrefix(E_LOG_DEBUG), QStringLiteral("\033[34m"));
        QCOMPARE(Logger::colorPrefix(E_LOG_INFO),  QStringLiteral("\033[32m"));
        QCOMPARE(Logger::colorPrefix(E_LOG_WARN),  QStringLiteral("\033[33m"));
        QCOMPARE(Logger::colorPrefix(E_LOG_ERROR), QStringLiteral("\033[31m"));
        QVERIFY(Logger::colorPrefix(E_LOG_OFF).isEmpty());
        QCOMPARE(Logger::colorSuffix(), QStringLiteral("\033[0m"));
#endif
    }

    // 验证正则构造对前缀元字符的转义：前缀含 '.' 时 'X' 不应被误匹配
    void testBuildRunRegex()
    {
        const QRegularExpression rx(Logger::buildRunRegex(QStringLiteral("my.app"),
                                                         QStringLiteral("\\d{8}")));
        // 正确前缀（含字面 '.'）应匹配，且能捕获 run 数字
        const QRegularExpressionMatch m = rx.match(QStringLiteral("my.app_20260801_run9.log"));
        QVERIFY(m.hasMatch());
        QCOMPARE(m.captured(1).toInt(), 9);
        // 前缀中 '.' 不应匹配 'X'（转义后为字面点）
        QVERIFY(!rx.match(QStringLiteral("myXapp_20260801_run1.log")).hasMatch());
        // part 段可选
        QVERIFY(rx.match(QStringLiteral("my.app_20260801_run1_part3.log")).hasMatch());
        // 不符合命名规则的文件不匹配
        QVERIFY(!rx.match(QStringLiteral("notmy.app_20260801_run1.log")).hasMatch());
    }

    // ==================== 集成测试 ====================

    // 验证流式宏接口端到端可用
    void testStreamMacros()
    {
        const QString dir = makeTempDir("streammacros", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);
        Logger::instance().setLogLevel(E_LOG_DEBUG);
        logdebug << "debug" << 1;
        loginfo  << "info"  << 2;
        logwarn  << "warn"  << 3;
        logerror << "error" << 4;
        Logger::instance().m_fileStream.flush();   // 白盒：强制刷新以便读取

        QVERIFY(totalLineCount(dir, "app_*.log") >= 4);
    }

    // 验证 init 后生成符合命名规则的日志文件
    void testInit_createsLogFile()
    {
        const QString dir = makeTempDir("initcreate", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);
        const QString expected = dir + QDir::separator()
                                 + QStringLiteral("app_%1_run1.log").arg(todayStr());
        QVERIFY(QFileInfo::exists(expected));
    }

    // 验证同日重复 init 会使 run 序号递增（run1、run2 共存）
    void testInit_runIndexIncrement()
    {
        const QString dir = makeTempDir("runinc", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);  // run1
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);  // run2
        const QString t = todayStr();
        QVERIFY(QFileInfo::exists(dir + QDir::separator() + QStringLiteral("app_%1_run1.log").arg(t)));
        QVERIFY(QFileInfo::exists(dir + QDir::separator() + QStringLiteral("app_%1_run2.log").arg(t)));
    }

    // 验证 maxSizeMB 非法（0/-1）被钳制为 1，少量日志不会触发无限 part 滚动
    void testInit_maxSizeValidation()
    {
        const QString dir = makeTempDir("maxsize", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 0, false, true, 0);  // 0 → 钳制为 1MB
        Logger::instance().setLogLevel(E_LOG_DEBUG);
        for (int i = 0; i < 5; ++i) {
            Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, QStringLiteral("msg %1").arg(i));
        }
        Logger::instance().m_fileStream.flush();

        // 5 条短日志远小于 1MB，不应产生任何 part 文件（未修复时会无限滚动）
        const QStringList logs = QDir(dir).entryList(QStringList() << "app_*_part*.log", QDir::Files);
        QCOMPARE(logs.size(), 0);
    }

    // 验证文件超过 maxSize 时自动滚动到 part2
    void testFileRolling_onSize()
    {
        const QString dir = makeTempDir("rolling", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 1, false, true, 0);  // 1MB 上限
        Logger::instance().setLogLevel(E_LOG_DEBUG);
        const QString pad(2000, QLatin1Char('X'));   // 每条约 2KB
        for (int i = 0; i < 700; ++i) {               // 总计约 1.4MB，应触发滚动
            Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, pad);
        }
        Logger::instance().m_fileStream.flush();

        const QStringList parts = QDir(dir).entryList(QStringList() << "app_*_part*.log", QDir::Files);
        QVERIFY(!parts.isEmpty());   // 至少出现一个 part 文件
    }

    // 验证重复 init（关闭文件）会释放旧文件句柄
    void testReInit_closesOldFile()
    {
        const QString dir = makeTempDir("reinit", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);  // 启用文件
        Logger::instance().setLogLevel(E_LOG_DEBUG);
        Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, QStringLiteral("first"));
        // 再次 init 但禁用文件
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, false, 0);

        // 白盒：旧文件应已关闭
        QVERIFY(!Logger::instance().m_logFile.isOpen());
        // 行为：句柄已释放，文件可被删除（Windows 上若未关闭则删除失败）
        const QString file1 = dir + QDir::separator()
                              + QStringLiteral("app_%1_run1.log").arg(todayStr());
        QVERIFY(QFile::remove(file1));
    }

    // 验证旧日志清理：超过保留天数的文件被删除，近期文件保留
    void testCleanOldLogs()
    {
        const QString dir = makeTempDir("clean", m_tempDirs);
        const QDate today = QDateTime::currentDateTime().date();

        // 创建一个 10 天前的"旧"日志文件（文件名用旧日期，mtime 也设为旧时间）
        const QString oldDateStr = today.addDays(-10).toString("yyyyMMdd");
        const QString oldFile = dir + QDir::separator()
                                + QStringLiteral("app_%1_run1.log").arg(oldDateStr);
        QFile of(oldFile);
        QVERIFY(of.open(QIODevice::WriteOnly));
        of.write("old content");
        of.flush();
        const bool timeSet = of.setFileTime(QDateTime(today.addDays(-10), QTime(0, 0, 0)),
                                            QFileDevice::FileModificationTime);
        of.close();
        if (!timeSet) {
            QSKIP("QFileDevice::setFileTime unsupported on this platform/build, skipping deletion check.");
        }

        // 创建一个昨天的"近期"日志文件，应被保留
        const QString yestDateStr = today.addDays(-1).toString("yyyyMMdd");
        const QString yestFile = dir + QDir::separator()
                                 + QStringLiteral("app_%1_run1.log").arg(yestDateStr);
        QFile yf(yestFile);
        QVERIFY(yf.open(QIODevice::WriteOnly));
        yf.write("yesterday content");
        yf.flush();
        yf.setFileTime(QDateTime(today.addDays(-1), QTime(0, 0, 0)),
                       QFileDevice::FileModificationTime);
        yf.close();

        // init 时传入 keepDays=7，触发清理
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 7);

        QVERIFY(!QFileInfo::exists(oldFile));     // 10 天前 → 删除
        QVERIFY(QFileInfo::exists(yestFile));     // 1 天前 → 保留
    }

    // 验证前缀含正则元字符时，run 序号不受相似文件名干扰
    void testRegexPrefixWithMetachars()
    {
        const QString dir = makeTempDir("meta", m_tempDirs);
        const QString t = todayStr();
        // 预置一个"诱饵"文件：用 'X' 替代前缀中的 '.'
        const QString decoy = dir + QDir::separator()
                              + QStringLiteral("myXapp_%1_run9.log").arg(t);
        QFile df(decoy);
        QVERIFY(df.open(QIODevice::WriteOnly));
        df.write("decoy");
        df.close();

        // 前缀 "my.app"：转义后 decoy 不应被识别为同前缀 → run 序号应为 1
        Logger::instance().init(dir, QStringLiteral("my.app"), 5, false, true, 0);
        const QString expected = dir + QDir::separator()
                                 + QStringLiteral("my.app_%1_run1.log").arg(t);
        QVERIFY(QFileInfo::exists(expected));
        // 不应生成 run10（未转义时 maxRun=9 → runIndex=10）
        const QString wrong = dir + QDir::separator()
                              + QStringLiteral("my.app_%1_run10.log").arg(t);
        QVERIFY(!QFileInfo::exists(wrong));
    }

    // 验证跨天滚动：伪造昨日状态后写日志，应生成今日日期的新文件
    void testCrossDayRolling()
    {
        const QString dir = makeTempDir("crossday", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);
        Logger::instance().setLogLevel(E_LOG_DEBUG);

        // 白盒：将内部状态伪造为"昨天"，模拟跨天
        const QDate yesterday = QDateTime::currentDateTime().date().addDays(-1);
        Logger::instance().m_curDate      = yesterday.toString("yyyyMMdd");
        Logger::instance().m_curJulianDay = yesterday.toJulianDay();

        // 写一条日志：needRollFile 应检测到跨天并切到今日
        Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, QStringLiteral("new day"));
        Logger::instance().m_fileStream.flush();

        // init 已创建今日 run1，跨天后 getMaxRunForDate(今日)=1 → 新文件为 run2
        const QString expected = dir + QDir::separator()
                                 + QStringLiteral("app_%1_run2.log").arg(todayStr());
        QVERIFY(QFileInfo::exists(expected));
    }

    // 验证多线程并发写入：无崩溃、行数精确、无交错
    void testMultithreadedLogging()
    {
        const QString dir = makeTempDir("mt", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 10, false, true, 0);
        Logger::instance().setLogLevel(E_LOG_DEBUG);

        const int N = 8;     // 线程数
        const int M = 200;   // 每线程日志条数
        QVector<QThread*> threads;
        threads.reserve(N);
        for (int t = 0; t < N; ++t) {
            QThread* th = QThread::create([t]() {
                for (int i = 0; i < M; ++i) {
                    Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__,
                                           QStringLiteral("thread=%1 iter=%2").arg(t).arg(i));
                }
            });
            threads.append(th);
        }
        for (QThread* th : threads) th->start();
        for (QThread* th : threads) th->wait();
        for (QThread* th : threads) delete th;   // 线程已结束，安全释放

        Logger::instance().m_fileStream.flush();   // 强制刷新剩余缓冲

        // 总行数应严格等于 N*M（无丢失、无交错）
        QCOMPARE(totalLineCount(dir, "app_*.log"), N * M);
    }

    // 验证含换行的消息在文件中仅占一行
    void testEscapeNewlines_singleLine()
    {
        const QString dir = makeTempDir("singleline", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);
        Logger::instance().setLogLevel(E_LOG_DEBUG);
        Logger::instance().log(E_LOG_INFO, __FILE__, __LINE__, __FUNCTION__,
                               QStringLiteral("line1\nline2\nline3"));
        Logger::instance().m_fileStream.flush();

        const QStringList logs = QDir(dir).entryList(QStringList() << "app_*.log", QDir::Files);
        QCOMPARE(logs.size(), 1);
        QFile f(dir + QDir::separator() + logs.first());
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QTextStream(&f).readAll();
        // 整条消息仅产生 1 个换行（即行尾），内部换行已被转义
        QCOMPARE(content.count(QLatin1Char('\n')), 1);
    }

    // 验证重复 init 后日志等级被保留（DEBUG 仍被 WARN 等级过滤）
    void testReinit_preservesLogLevel()
    {
        const QString dir = makeTempDir("preserve", m_tempDirs);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);
        Logger::instance().setLogLevel(E_LOG_WARN);
        Logger::instance().init(dir, QStringLiteral("app"), 5, false, true, 0);  // 重复 init

        // 白盒：等级仍为 WARN
        QCOMPARE(Logger::instance().m_logLevel, E_LOG_WARN);

        // 行为：DEBUG 日志不应落盘
        Logger::instance().log(E_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__,
                               QStringLiteral("should_not_appear"));
        Logger::instance().m_fileStream.flush();

        const QStringList logs = QDir(dir).entryList(QStringList() << "app_*.log", QDir::Files);
        QVERIFY(!logs.isEmpty());
        QFile f(dir + QDir::separator() + logs.last());
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QVERIFY(!QTextStream(&f).readAll().contains(QStringLiteral("should_not_appear")));
    }
};

QTEST_MAIN(LoggerTest)
#include "tst_loggertest.moc"
