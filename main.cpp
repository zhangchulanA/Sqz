

#include <QApplication>
#include <QDebug>
#include "SuperTableAll.h"
#include "SqzApplication.h"
using namespace Sqz;


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Logger::instance().init(".","log");
    SqzApplication sq;
    sq.Init();
    sq.LogRegClass();
    return app.exec();

}
