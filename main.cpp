
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
    ProtocolSchema schema;

        // ========== 1. 定义协议 ==========
        schema.addField("mode", 0, 0, 8, ProtocolSchema::UInt)
              .addField("status", 1, 0, 8, ProtocolSchema::UInt)
              .addField("error", 2, 0, 8, ProtocolSchema::UInt);

        // ========== 2. 添加枚举映射 ==========
        // mode映射
        schema.map("mode", {
            {0, "停止"},
            {1, "启动"},
            {2, "手动"},
            {3, "调试"}
        });

        // status映射
        schema.map("status", {
            {0, "空闲"},
            {1, "运行"},
            {2, "故障"},
            {3, "待机"}
        });

        // error映射
        schema.map("error", {
            {0, "正常"},
            {1, "超时"},
            {2, "参数错误"},
            {3, "硬件故障"}
        });

        // ========== 3. 启用反向映射（支持打包时使用文本） ==========
        schema.rmap("mode")
              .rmap("status")
              .rmap("error");

        // ========== 4. 测试解析 ==========
        qDebug() << "=== 解析测试 ===";
        QByteArray rxData;
        rxData.append(0x01);  // mode=1 → "启动"
        rxData.append(0x02);  // status=2 → "故障"
        rxData.append(0x01);  // error=1 → "超时"

        QJsonObject result = schema.parse(rxData);

        qDebug() << "mode:" << result["mode"].toString();      // "启动"
        qDebug() << "status:" << result["status"].toString();   // "故障"
        qDebug() << "error:" << result["error"].toString();     // "超时"

        // 查看原始数值（如果有需要）
        qDebug() << "mode_raw:" << result["mode_raw"].toInt();   // 1 (如果映射中有未知值会添加)

        // ========== 5. 测试打包（使用文本） ==========
        qDebug() << "\n=== 打包测试 ===";
        QJsonObject txData;
        txData["mode"] = "启动";    // 文本 → 自动转 1
        txData["status"] = "运行";   // 文本 → 自动转 1
        txData["error"] = "正常";    // 文本 → 自动转 0

        QByteArray packed;
        QString errorMsg;
        if (schema.pack(txData, packed, &errorMsg)) {
            qDebug() << "打包成功！";
            qDebug() << "packed hex:" << packed.toHex();        // "010100"
            qDebug() << "mode字节:" << (int)(quint8)packed[0];  // 1
            qDebug() << "status字节:" << (int)(quint8)packed[1]; // 1
            qDebug() << "error字节:" << (int)(quint8)packed[2];  // 0
        } else {
            qDebug() << "打包失败:" << errorMsg;
        }

        // ========== 6. 验证对称性 ==========
        qDebug() << "\n=== 对称性验证 ===";
        QJsonObject parsed = schema.parse(packed);

        QJsonDocument doc1(txData);
        QJsonDocument doc2(parsed);

        // ✅ 正确方式
        qDebug().noquote() << "原始JSON:" << QString::fromUtf8(doc1.toJson());
        qDebug().noquote() << "解析JSON:" << QString::fromUtf8(doc2.toJson());

        // 验证是否一致
        bool isEqual = (txData["mode"].toString() == parsed["mode"].toString());
        qDebug() << "mode一致:" << (isEqual ? "✅" : "❌");

        isEqual = (txData["status"].toString() == parsed["status"].toString());
        qDebug() << "status一致:" << (isEqual ? "✅" : "❌");

        isEqual = (txData["error"].toString() == parsed["error"].toString());
        qDebug() << "error一致:" << (isEqual ? "✅" : "❌");
}


int main(int argc, char *argv[])
{
    QApplication coreApp(argc, argv);
    Logger::instance().init("./","log");
    SqzApplication App;

    if (!App.Init())
        return -1;
        App.LogRegClass();

        ProtocolSchemaTest();
    return coreApp.exec();
}
