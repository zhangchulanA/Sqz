#include "SqzApplication.h"
#include "Logger.h"
#include "SqzHub.h"

using namespace Sqz;

SqzApplication::SqzApplication(int &argc, char **argv, QObject *parent)
    :QObject(parent)
{
    m_app = new QApplication(argc,argv);

    // 绑定pro注入的版本号到全局qApp
    m_app->setApplicationVersion(APP_VERSION);
    m_app->setApplicationName("Sqz");

    Logger::instance().init("./log","chatlog",10,true);
    SqzHub::SetThreadPrefix(SQZNAME);

    SqzIn.PrintRegClass();
}

SqzApplication::~SqzApplication()
{
    Close();
}

QApplication* SqzApplication::App()
{
    return m_app;
}

int SqzApplication::exec()
{
    return m_app->exec();
}

void SqzApplication::SetMainWidget(const QString &name)
{
    SqzIn.CreateWidget(name);
}

void SqzApplication::SetLogger(const QString &logDir, const QString &filePrefix, qint64 maxSizeMB, bool enableConsole, bool enableFile, int keepDays)
{
    Logger::instance().init(logDir,filePrefix,maxSizeMB,enableConsole,enableFile,keepDays);
}

void SqzApplication::Close()
{
    SqzIn.CloseAll();
}
