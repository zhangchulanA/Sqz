
#include <QApplication>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QSpinBox>
#include <QTextEdit>
#include "Translator.h"
#include "SqzHub.h"
#include "ThreadPool.h"
#include "Logger.h"
#include "TimerUtils.h"

#include "CustomSearchBox.h"
#include "FlexData.h"
#include "ProtocolSchema.h"
#include "ChainBranch.h"
#include <FluentCard.h>
#include "MsgBox.h"
#include "SuperTableAll.h"
#include "UiUtils.h"
#include "RadioLink.h"
#include "SqzState.h"
#include <QAbstractNativeEventFilter>
#include <SqzApplication.h>
#include <DataTransporter.h>
#include <ShortcutManager.h>
#include "SERIALIZE.h"
using namespace Net;
using namespace Sqz;


int main(int argc, char *argv[])
{
    QApplication coreApp(argc, argv);
    Logger::instance().init("./","log");
    SqzApplication App;
    App.LogRegClass();
    if (!App.Init())
        return -1;
    return coreApp.exec();
}
