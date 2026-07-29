#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include "ProtocolSchema.h"

using namespace Sqz;

int mainTest(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QString err;

    // 测试1：固定长度整数（小端 + Physical）
    ProtocolSchema s1;
    s1.addField("temp", 0, 0, 16, ProtocolSchema::Int, ProtocolSchema::LittleEndian,
                ProtocolSchema::Physical, true, 0.1, -273.15);
    QByteArray raw1 = QByteArray::fromHex("0A00");
    QJsonObject parsed1 = s1.parse(raw1, &err);
    qDebug() << "Test1 temp:" << parsed1["temp"].toDouble() << "err:" << err;

    // 测试2：固定长度整数（大端 + MsbFirst）
    ProtocolSchema s2;
    s2.addField("value", 0, 0, 12, ProtocolSchema::UInt, ProtocolSchema::BigEndian,
                ProtocolSchema::MsbFirst);
    QByteArray raw2 = QByteArray::fromHex("ABC0");
    QJsonObject parsed2 = s2.parse(raw2, &err);
    qDebug() << "Test2 value:" << parsed2["value"].toInt() << "err:" << err;

    // 测试3：变长字段 + 长度字段
    ProtocolSchema s3;
    s3.addField("len", 0, 0, 8, ProtocolSchema::UInt);
    s3.addVariableField("data", 1, 0, "len", ProtocolSchema::HexString);
    QByteArray raw3 = QByteArray::fromHex("034A4B4C");
    QJsonObject parsed3 = s3.parse(raw3, &err);
    qDebug() << "Test3 len:" << parsed3["len"].toInt() << "data:" << parsed3["data"].toString();

    // 测试4：打包对称性验证
    QJsonObject pack3{{"len", 3}, {"data", "4A4B4C"}};
    QByteArray packed3;
    if (s3.pack(pack3, packed3, &err)) {
        qDebug() << "Test4 packed:" << packed3.toHex() << "match:" << (packed3 == raw3);
    } else {
        qDebug() << "Test4 pack failed:" << err;
    }

    // 测试5：字段重叠检测
    ProtocolSchema s4;
    s4.addField("f1", 0, 0, 8);
    s4.addField("f2", 0, 2, 4);
    QStringList overlaps = s4.checkOverlap({}, &err);
    qDebug() << "Test5 overlaps:" << overlaps << "err:" << err;

    // 测试6：错误场景 - 缺少字段
    ProtocolSchema s5;
    s5.addField("required", 0, 0, 8);
    QJsonObject empty;
    QByteArray out;
    bool ok = s5.pack(empty, out, &err);
    qDebug() << "Test6 pack ok:" << ok << "err:" << err;

    return 0;
}
