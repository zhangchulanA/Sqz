#include "SqzQuickTest.h"
#include "SqzClassReg.h"
#include "SqzBus.h"
SqzQuickTest::SqzQuickTest(QObject *parent) : SqzQuick(parent)
{
setQmlSourcePath("qrc:/test/SqzQuickTest.qml");
}

void SqzQuickTest::quitApp()
{
    SqzApp->QuitApp();
}
SQZ_REG(SqzQuickTest);
