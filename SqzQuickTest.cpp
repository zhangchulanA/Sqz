#include "SqzQuickTest.h"
#include "SqzClassReg.h"
SqzQuickTest::SqzQuickTest(QObject *parent) : SqzQuick(parent)
{
    loginfo << " SqzQuickTest start";
}

void SqzQuickTest::QuitApp()
{
    SqzApp->QuitApp();
}
SQZ_REG_NOARG(SqzQuickTest);
