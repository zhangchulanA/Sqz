#ifndef SQZAPPLICATION_H
#define SQZAPPLICATION_H

#include <QObject>
#include <QApplication>
#include "SqzGlobal.h"

class SQZ_FRAMEWORK_API SqzApplication : public QObject
{
    Q_OBJECT
public:

    explicit SqzApplication(int &argc, char **argv,QObject *parent = nullptr);
    ~SqzApplication();

public:

    QApplication *App();

    int exec();
    //设置主窗口
    void SetMainWidget(const QString& name);
    //设置logger
    void SetLogger(const QString& logDir,
                   const QString& filePrefix,
                   qint64 maxSizeMB     = 10,
                   bool   enableConsole = true,
                   bool   enableFile    = false,
                   int    keepDays      = 7);

    void Close();
private:
    QApplication *m_app;
};

#endif // SQZAPPLICATION_H
