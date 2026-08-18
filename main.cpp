#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include "ProtocolSchema.h"
#include "SqzApplication.h"

using namespace Sqz;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Logger::instance().init(".","log");
    SqzApplication sq;
//    sq.Init();
    sq.LogRegClass();

    loginfo << QString::number(1).rightJustified(3,'0').left(3);
    loginfo << QString::number(19).rightJustified(3,'0').left(3);
    loginfo << QString::number(194).rightJustified(3,'0').left(3);
    loginfo << QString::number(7465).rightJustified(3,'0').left(3);
    loginfo << QString::number(99999).rightJustified(3,'0').left(3);

    return app.exec();
}
