
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

void ProtocolSchemaTest()
{
    QByteArray data;
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));
    data.push_back(static_cast<char>(0x01));

    ProtocolSchema schema;
    schema.addField("KZZ",0,0,8,ProtocolSchema::ValueType::UInt)
          .addField("CZZ",1,0,8,ProtocolSchema::ValueType::UInt)
          .addField("CD",2,0,16,ProtocolSchema::ValueType::UInt)
          .addField("IP",4,0,32,ProtocolSchema::ValueType::UInt,ProtocolSchema::Endian::BigEndian)
          .addField("YM",8,0,32,ProtocolSchema::ValueType::UInt);


    QString error;
    QJsonObject json = schema.parse(data,&error);

    if(!error.isEmpty()){
        logerror << error;
    }

    QJsonObject txData;
    txData["KZZ"] = 0xF0;
    txData["CZZ"] = 0x02;
    txData["CD"] = 0x08;
    txData["IP"] = 10;
    txData["YM"] = 10;

    QByteArray packed;
    QString errorMsg;
    if(schema.pack(txData,packed,&error)){
        LogData(packed);
        logdebug << QString::number(static_cast<uchar>(packed[0]) ,16);
    }else {
        logerror << error;
    }

}


int main(int argc, char *argv[])
{
    QApplication coreApp(argc, argv);
    Logger::instance().init("./","log");
    SqzApplication App;

    if (!App.Init())
        return -1;
        App.LogRegClass();
    return coreApp.exec();
}
