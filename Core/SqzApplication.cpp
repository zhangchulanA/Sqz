#include "SqzApplication.h"
#include "Logger.h"
#include "SqzHub.h"

using namespace Sqz;

SqzApplication::SqzApplication(int &argc, char **argv, const QString &Prefix)
    :QApplication(argc,argv)
{
    Logger::instance().init("./log","logger",10,true);
    SetPrefix(Prefix);
    SqzIn.PrintRegClass();
}

SqzApplication::~SqzApplication()
{
    Close();
}

void SqzApplication::SetPrefix(const QString &prefix)
{
    logdebug << "SQZ_Prefix:" << prefix;
    SqzHub::SetThreadPrefix(prefix);
}

void SqzApplication::SetMainWidget(const QString& WidgetName)
{
    SqzIn.CreateWidget(WidgetName);
}

void SqzApplication::SetMainService(const QString &ServiceName)
{
    SqzIn.CreateObject(ServiceName);
}

void SqzApplication::SetLogger(const QString &logDir, const QString &filePrefix, qint64 maxSizeMB, bool enableConsole, bool enableFile, int keepDays)
{
    Logger::instance().init(logDir,filePrefix,maxSizeMB,enableConsole,enableFile,keepDays);
}

void SqzApplication::Close()
{
    SqzIn.CloseAll();
}
