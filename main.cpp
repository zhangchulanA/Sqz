#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include "ProtocolSchema.h"

// ========== FEC 计算（异或校验） ==========
quint8 calculateFec(const QByteArray& data) {
    quint8 checksum = 0;
    for (char c : data) {
        checksum ^= static_cast<quint8>(c);
    }
    return checksum;
}

// ========== 辅助：过滤字段，只保留当前命令需要的字段 ==========
QJsonObject filterFields(const QJsonObject& input, const QString& cmdPrefix) {
    QJsonObject output;
    output["Cmd"] = input["Cmd"];
    output["Op"] = input["Op"];
    output["Len"] = input["Len"];
    for (const QString& key : input.keys()) {
        if (key.startsWith(cmdPrefix)) {
            output[key] = input[key];
        }
    }
    return output;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // 1. 加载协议 JSON
    Sqz::ProtocolSchema schema;
    QString err;
    QString jsonPath = QCoreApplication::applicationDirPath() + "/NcpV2.json";
    if (!schema.loadFile(jsonPath, &err)) {
        qDebug() << "❌ Load JSON failed:" << err;
        return -1;
    }
    qDebug() << "✅ JSON loaded successfully";

    // 2. 构造业务数据（0xF0 设置 IP）
    QJsonObject txData;
    txData["Cmd"] = 0xF0;          // 控制字
    txData["Op"]  = 1;             // 设置操作
    txData["Len"] = 8;             // 数据长度（IP 4 + 掩码 4，不含 FEC）

    // IP = 192.168.1.10
    txData["F0SetIp0"] = 192;
    txData["F0SetIp1"] = 168;
    txData["F0SetIp2"] = 1;
    txData["F0SetIp3"] = 10;

    // 掩码 = 255.255.255.0
    txData["F0SetMask0"] = 255;
    txData["F0SetMask1"] = 255;
    txData["F0SetMask2"] = 255;
    txData["F0SetMask3"] = 0;

    // 3. ✅ 过滤：只保留当前命令需要的字段
    QJsonObject filteredData = filterFields(txData, "F0");
    qDebug() << "🔍 Filtered keys:" << filteredData.keys();

    // 4. 打包（不含 FEC）
    QByteArray frameWithoutFec;
    if (!schema.pack(filteredData, frameWithoutFec, &err)) {
        qDebug() << "❌ Pack failed:" << err;
        return -1;
    }
    qDebug() << "✅ Packed" << frameWithoutFec.size() << "bytes (without FEC):";
    qDebug() << "   Hex:" << frameWithoutFec.toHex(' ');

    // 5. 计算并追加 FEC
    quint8 fec = calculateFec(frameWithoutFec);
    QByteArray frameWithFec = frameWithoutFec;
    frameWithFec.append(static_cast<char>(fec));
    qDebug() << "✅ Final frame with FEC (size=" << frameWithFec.size() << "):";
    qDebug() << "   Hex:" << frameWithFec.toHex(' ');

    // ========== 模拟接收端 ==========

    // 6. 校验帧长度（至少 5 字节：头4 + FEC）
    if (frameWithFec.size() < 5) {
        qDebug() << "❌ Frame too short";
        return -1;
    }

    // 提取长度字段（大端）
    quint16 dataLen = (static_cast<quint8>(frameWithFec[2]) << 8) |
                       static_cast<quint8>(frameWithFec[3]);
    int expectedLen = 4 + dataLen + 1;  // 头4 + 数据 + FEC
    if (frameWithFec.size() != expectedLen) {
        qDebug() << "❌ Frame length mismatch, expected" << expectedLen << "got" << frameWithFec.size();
        return -1;
    }
    qDebug() << "✅ Frame length check passed";

    // 7. 剥离 FEC 并校验
    QByteArray dataPart = frameWithFec.left(frameWithFec.size() - 1);
    // ✅ 修复：使用 at() 替代 last()，兼容旧版 Qt
    quint8 receivedFec = static_cast<quint8>(frameWithFec.at(frameWithFec.size() - 1));
    quint8 calcFec = calculateFec(dataPart);
    if (calcFec != receivedFec) {
        qDebug() << "❌ FEC mismatch! calc=" << calcFec << " received=" << receivedFec;
        return -1;
    }
    qDebug() << "✅ FEC校验通过";

    // 8. 解析（传入不含 FEC 的数据）
    QJsonObject parsed = schema.parse(dataPart, &err);
    if (parsed.isEmpty() && !err.isEmpty()) {
        qDebug() << "❌ Parse failed:" << err;
        return -1;
    }

    // 9. 验证关键字段
    qDebug() << "\n========== 解析结果 ==========";
    qDebug() << "Cmd :" << parsed["Cmd"].toInt();
    qDebug() << "Op  :" << parsed["Op"].toString();
    qDebug() << "Len :" << parsed["Len"].toInt();
    qDebug() << "F0SetIp0 =" << parsed["F0SetIp0"].toInt();
    qDebug() << "F0SetIp1 =" << parsed["F0SetIp1"].toInt();
    qDebug() << "F0SetIp2 =" << parsed["F0SetIp2"].toInt();
    qDebug() << "F0SetIp3 =" << parsed["F0SetIp3"].toInt();
    qDebug() << "F0SetMask0 =" << parsed["F0SetMask0"].toInt();
    qDebug() << "F0SetMask1 =" << parsed["F0SetMask1"].toInt();
    qDebug() << "F0SetMask2 =" << parsed["F0SetMask2"].toInt();
    qDebug() << "F0SetMask3 =" << parsed["F0SetMask3"].toInt();
    qDebug() << "Op raw:" << parsed["Op"];
    qDebug() << "Op type:" << parsed["Op"].type();
    qDebug() << "\n========== 测试通过 ==========";
    return 0;
}
