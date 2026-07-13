#ifndef SQZAPPLICATION_H
#define SQZAPPLICATION_H

#include <QObject>
#include <QApplication>
#include "SqzGlobal.h"

class SQZ_FRAMEWORK_API SqzApplication : public QApplication
{
    Q_OBJECT
public:

    explicit SqzApplication(int &argc, char **argv,const QString& Prefix = "");
    ~SqzApplication();

public:

    //设置前缀
    void SetPrefix(const QString &prefix = "");
    //设置主窗口
    void SetMainWidget(const QString& WidgetName);
    //设置主服务
    void SetMainService(const QString& ServiceName);
    //设置logger
    void SetLogger(const QString& logDir,
                   const QString& filePrefix,
                   qint64 maxSizeMB     = 10,
                   bool   enableConsole = true,
                   bool   enableFile    = false,
                   int    keepDays      = 7);

    //关闭全部
    void Close();
private:
};

#endif // SQZAPPLICATION_H
