#include "SqzQuickTest.h"
#include "SqzClassReg.h"
#include "SqzBus.h"
SqzQuickTest::SqzQuickTest(QObject *parent) : SqzQuick(parent)
{
    loginfo << " SqzQuickTest start";
    SqzBus::Receive(this,"123",[=](const QVariant& var){
        logdebug << var.toInt();
    });
}

void SqzQuickTest::QuitApp()
{
    SqzApp->QuitApp();
}
SQZ_REG_NOARG(SqzQuickTest);
