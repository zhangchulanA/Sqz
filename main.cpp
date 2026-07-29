/*****************************************************************************
 * 文件: main.cpp
 * 功能: ProtocolSchema 全接口测试程序（非Qt Test框架，直接运行验证）
 * 接口覆盖:
 *   ProtocolSchema:
 *     1.  构造/析构
 *     2.  addField()         - 固定长度字段（全参数组合）
 *     3.  addVariableField() - 变长字段
 *     4.  when()/otherwise() - 条件分支
 *     5.  clear()           - 清空
 *     6.  checkOverlap()    - 字段重叠检测
 *     7.  validateSchema()  - schema 校验
 *     8.  parse()           - 二进制解析
 *     9.  pack()            - JSON 打包
 *     10. packToArray()     - 便捷打包
 *   ConditionalBuilder:
 *     11. addField()              - 条件分支内添加固定字段
 *     12. addVariableField()      - 条件分支内添加变长字段
 *     13. when()/otherwise()      - 嵌套条件
 *     14. endBranch()             - 结束分支
 * 作者: AI Test Generator
 * 日期: 2026-07-29
 *****************************************************************************/

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QDateTime>

// 包含被测头文件（实现由.pro的SOURCES提供）
#include "ProtocolSchema.h"

using namespace Sqz;

// ==================== 测试统计 ====================
struct TestStats {
    int passed = 0;
    int failed = 0;
    int total = 0;
    QString currentCase;

    void check(bool condition, const QString& desc) {
        total++;
        if (condition) {
            passed++;
            qDebug() << "  [PASS]" << desc;
        } else {
            failed++;
            qDebug() << "  [FAIL]" << desc;
        }
    }

    void startCase(const QString& name) {
        currentCase = name;
        qDebug() << "\n=====" << name << "=====";
    }

    void summary() {
        qDebug() << "\n========================================";
        qDebug() << "测试汇总: 总计=" << total
                 << " 通过=" << passed
                 << " 失败=" << failed;
        qDebug() << "通过率:" << (total > 0 ? QString::number(passed * 100.0 / total, 'f', 1) + "%" : "N/A");
        qDebug() << "========================================";
    }
};

static TestStats g_stats;

// ==================== 辅助宏 ====================
#define CHECK(cond, desc) g_stats.check(cond, desc)

// ==================== 主测试函数 ====================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    g_stats.startCase("ProtocolSchema 全接口测试");

    // ================================================
    // 1. 构造/析构 + addField 基础
    // ================================================
    {
        g_stats.startCase("1. 构造/析构 + addField 基础");

        ProtocolSchema schema;
        QString err;

        // 1.1 添加单字节无符号整数
        schema.addField("reg", 0, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(err.isEmpty(), "addField 基本: 无错误");

        // 1.2 解析验证
        QByteArray data = QByteArray::fromHex("2A");
        QJsonObject result = schema.parse(data, &err);
        CHECK(err.isEmpty(), "解析: 无错误");
        CHECK(result["reg"].toDouble() == 42.0, "解析值正确: 0x2A = 42");

        // 1.3 打包验证
        QJsonObject values;
        values["reg"] = QJsonValue(42);
        QByteArray packed = schema.packToArray(values, &err);
        CHECK(err.isEmpty(), "打包: 无错误");
        CHECK(packed == data, "打包数据正确");

        // 1.4 往返验证
        QJsonObject roundTrip = schema.parse(packed, &err);
        CHECK(roundTrip["reg"].toDouble() == 42.0, "往返一致");
    }

    // ================================================
    // 2. addField - 全部 ValueType 类型
    // ================================================
    {
        g_stats.startCase("2. addField 全类型");

        // 2.1 Int 类型（有符号整数）
        {
            ProtocolSchema schema;
            schema.addField("signed_val", 0, 0, 16, Int,
                            LittleEndian, Physical,
                            true, 1.0, 0.0);
            // 解析 -1 (0xFFFF in little endian)
            QByteArray data = QByteArray::fromHex("FFFF");
            QJsonObject result = schema.parse(data);
            CHECK(result["signed_val"].toDouble() == -1.0, "Int16 -1");

            // 解析 -32768 (最小值)
            data = QByteArray::fromHex("0080");
            result = schema.parse(data);
            CHECK(result["signed_val"].toDouble() == -32768.0, "Int16 -32768");

            // 解析 32767 (最大值)
            data = QByteArray::fromHex("FF7F");
            result = schema.parse(data);
            CHECK(result["signed_val"].toDouble() == 32767.0, "Int16 32767");
        }

        // 2.2 UInt 类型
        {
            ProtocolSchema schema;
            schema.addField("unsigned_val", 0, 0, 32, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("0A000000"); // 10
            QJsonObject result = schema.parse(data);
            CHECK(result["unsigned_val"].toDouble() == 10.0, "UInt32 10");
        }

        // 2.3 HexString 类型
        {
            ProtocolSchema schema;
            schema.addField("hex_data", 0, 0, 16, HexString,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("DEAD");
            QJsonObject result = schema.parse(data);
            QString hexStr = result["hex_data"].toString();
            CHECK(hexStr.toUpper().contains("DEAD"), "HexString 解析");

            // 打包
            QJsonObject values;
            values["hex_data"] = QJsonValue("DEAD");
            QByteArray packed = schema.packToArray(values);
            CHECK(packed == data, "HexString 打包");
        }

        // 2.4 Base64 类型
        {
            ProtocolSchema schema;
            schema.addField("b64_data", 0, 0, 8, Base64,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("41"); // 'A'
            QJsonObject result = schema.parse(data);
            CHECK(!result["b64_data"].toString().isEmpty(), "Base64 解析");
        }

        // 2.5 RawBytes 类型
        {
            ProtocolSchema schema;
            schema.addField("raw_data", 0, 0, 8, RawBytes,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("42"); // 'B'
            QJsonObject result = schema.parse(data);
            CHECK(!result["raw_data"].toString().isEmpty(), "RawBytes 解析");
        }

        // 2.6 String 类型
        {
            ProtocolSchema schema;
            schema.addField("text", 0, 0, 24, String,
                            LittleEndian, Physical);
            QByteArray data("Hi!", 3);
            QJsonObject result = schema.parse(data);
            CHECK(result["text"].toString() == "Hi!", "String 解析");

            // 打包
            QJsonObject values;
            values["text"] = QJsonValue("Hi!");
            QByteArray packed = schema.packToArray(values);
            CHECK(packed == data, "String 打包");
        }
    }

    // ================================================
    // 3. 字节序与位序组合
    // ================================================
    {
        g_stats.startCase("3. 字节序与位序组合");

        // 3.1 LittleEndian + Physical (默认小端)
        {
            ProtocolSchema schema;
            schema.addField("le_val", 0, 0, 16, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("3412"); // 0x1234
            QJsonObject result = schema.parse(data);
            CHECK(result["le_val"].toDouble() == 4660.0, "LE+Physical: 0x1234");
        }

        // 3.2 BigEndian + MsbFirst (网络字节序)
        {
            ProtocolSchema schema;
            schema.addField("be_val", 0, 0, 16, UInt,
                            BigEndian, MsbFirst);
            QByteArray data = QByteArray::fromHex("1234"); // 0x1234
            QJsonObject result = schema.parse(data);
            CHECK(result["be_val"].toDouble() == 4660.0, "BE+MsbFirst: 0x1234");
        }

        // 3.3 LittleEndian + MsbFirst
        {
            ProtocolSchema schema;
            schema.addField("le_msb", 0, 0, 16, UInt,
                            LittleEndian, MsbFirst);
            QByteArray data = QByteArray::fromHex("1234");
            QJsonObject result = schema.parse(data);
            qDebug() << "LE+MsbFirst result:" << result["le_msb"].toDouble();
            // MsbFirst先按大端组装，再做LittleEndian反转
            // 原始字节: 12 34 -> MsbFirst组装: 0x1234 -> LE反转: 0x3412 (13330)
            CHECK(result["le_msb"].toDouble() == 13330.0, "LE+MsbFirst: 0x3412");
        }

        // 3.4 单字节位域（字节序无效）
        {
            ProtocolSchema schema;
            schema.addField("low_nibble", 0, 0, 4, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("AF"); // 低4位 = 0xF = 15
            QJsonObject result = schema.parse(data);
            CHECK(result["low_nibble"].toDouble() == 15.0, "单字节位域: 无符号低4位=15");
        }

        // 3.5 单比特域
        {
            ProtocolSchema schema;
            schema.addField("bit7", 0, 7, 1, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("80"); // bit7 = 1
            QJsonObject result = schema.parse(data);
            CHECK(result["bit7"].toDouble() == 1.0, "单比特: bit7=1");

            data = QByteArray::fromHex("7F"); // bit7 = 0
            result = schema.parse(data);
            CHECK(result["bit7"].toDouble() == 0.0, "单比特: bit7=0");
        }

        // 3.6 MsbFirst 取高4位
        {
            ProtocolSchema schema;
            schema.addField("high_nibble", 0, 0, 4, UInt,
                            LittleEndian, MsbFirst);
            QByteArray data = QByteArray::fromHex("5F"); // 高4位 = 0x5 = 5
            QJsonObject result = schema.parse(data);
            CHECK(result["high_nibble"].toDouble() == 5.0, "MsbFirst取高4位=5");
        }
    }

    // ================================================
    // 4. 线性变换（系数/偏移）
    // ================================================
    {
        g_stats.startCase("4. 线性变换");

        // 4.1 正数系数
        {
            ProtocolSchema schema;
            schema.addField("temp", 0, 0, 8, UInt,
                            LittleEndian, Physical,
                            false, 0.1, 25.0);
            QByteArray data = QByteArray::fromHex("64"); // 100
            QJsonObject result = schema.parse(data);
            double temp = result["temp"].toDouble();
            CHECK(qAbs(temp - 35.0) < 0.01, QString("系数0.1+偏移25: 100*0.1+25=%1").arg(temp));

            // 打包往返
            QJsonObject values;
            values["temp"] = QJsonValue(35.0);
            QByteArray packed = schema.packToArray(values);
            QJsonObject rt = schema.parse(packed);
            CHECK(qAbs(rt["temp"].toDouble() - 35.0) < 0.01, "线性变换往返一致");
        }

        // 4.2 负数系数
        {
            ProtocolSchema schema;
            schema.addField("inverted", 0, 0, 8, UInt,
                            LittleEndian, Physical,
                            false, -2.0, 100.0);
            QByteArray data = QByteArray::fromHex("0A"); // 10
            QJsonObject result = schema.parse(data);
            double val = result["inverted"].toDouble();
            CHECK(qAbs(val - 80.0) < 0.01, QString("负系数-2+偏移100: 10*(-2)+100=%1").arg(val));
        }

        // 4.3 零系数（解析正常，打包失败）
        {
            ProtocolSchema schema;
            schema.addField("zero_factor", 0, 0, 8, UInt,
                            LittleEndian, Physical,
                            false, 0.0, 50.0);
            QByteArray data = QByteArray::fromHex("42");
            QJsonObject result = schema.parse(data);
            CHECK(qAbs(result["zero_factor"].toDouble() - 50.0) < 0.01,
                  "零系数: 所有值均为offset");

            // 打包应失败
            QJsonObject values;
            values["zero_factor"] = QJsonValue(50.0);
            QString err;
            QByteArray packed = schema.packToArray(values, &err);
            CHECK(packed.isEmpty(), "零系数打包应失败");
            CHECK(!err.isEmpty(), "零系数打包有错误信息");
        }
    }

    // ================================================
    // 5. 多字节整数边界
    // ================================================
    {
        g_stats.startCase("5. 多字节整数边界");

        // 5.1 64位最大值
        {
            ProtocolSchema schema;
            schema.addField("max_u64", 0, 0, 64, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("FFFFFFFFFFFFFFFF");
            QJsonObject result = schema.parse(data);
            double val = result["max_u64"].toDouble();
            qDebug() << "UINT64_MAX 解析值:" << val;
            CHECK(val > 9e18, "UINT64_MAX 接近1.8e19");
        }

        // 5.2 64位有符号最小值
        {
            ProtocolSchema schema;
            schema.addField("min_i64", 0, 0, 64, Int,
                            LittleEndian, Physical,
                            true);
            QByteArray data = QByteArray::fromHex("0000000000000080"); // INT64_MIN
            QJsonObject result = schema.parse(data);
            qDebug() << "INT64_MIN:" << result["min_i64"].toDouble();
            CHECK(result["min_i64"].toDouble() < 0, "INT64_MIN 为负数");
        }

        // 5.3 32位有符号负数
        {
            ProtocolSchema schema;
            schema.addField("neg_i32", 0, 0, 32, Int,
                            LittleEndian, Physical,
                            true);
            QByteArray data = QByteArray::fromHex("FFFFFFFF"); // -1
            QJsonObject result = schema.parse(data);
            CHECK(result["neg_i32"].toDouble() == -1.0, "INT32 -1");
        }

        // 5.4 16位有符号边界值
        {
            ProtocolSchema schema;
            schema.addField("i16", 0, 0, 16, Int,
                            LittleEndian, Physical,
                            true);
            struct TestCase { const char* hex; double expected; const char* desc; };
            TestCase cases[] = {
                {"0000", 0.0, "零值"},
                {"FF7F", 32767.0, "最大正数"},
                {"0080", -32768.0, "最小负数"},
                {"FFFF", -1.0, "-1"},
                {"0100", 1.0, "最小正数"}
            };
            for (const auto& tc : cases) {
                QByteArray data = QByteArray::fromHex(tc.hex);
                QJsonObject result = schema.parse(data);
                double actual = result["i16"].toDouble();
                CHECK(qAbs(actual - tc.expected) < 0.01,
                      QString("i16 %1: %2 -> %3").arg(tc.desc).arg(tc.hex).arg(actual));
            }
        }
    }

    // ================================================
    // 6. addVariableField 变长字段
    // ================================================
    {
        g_stats.startCase("6. addVariableField 变长字段");

        // 6.1 基本变长
        {
            ProtocolSchema schema;
            schema.addField("len", 0, 0, 8, UInt);
            schema.addVariableField("payload", 1, 0, "len", RawBytes);

            QByteArray data = QByteArray::fromHex("03414243"); // len=3, "ABC"
            QJsonObject result = schema.parse(data);
            CHECK(result.contains("payload"), "变长字段解析包含payload");

            // 打包
            QJsonObject values;
            values["len"] = QJsonValue(3);
            values["payload"] = QJsonValue(QString::fromLatin1(QByteArray::fromHex("414243").toBase64()));
            QString err;
            QByteArray packed = schema.packToArray(values, &err);
            CHECK(!packed.isEmpty(), "变长字段打包成功");
        }

        // 6.2 零长度变长
        {
            ProtocolSchema schema;
            schema.addField("len", 0, 0, 8, UInt);
            schema.addVariableField("data", 1, 0, "len", RawBytes);

            QByteArray data = QByteArray::fromHex("00"); // len=0
            QJsonObject result = schema.parse(data);
            CHECK(result.contains("data"), "零长度解析包含data");
        }

        // 6.3 缺失长度字段
        {
            ProtocolSchema schema;
            schema.addVariableField("orphan", 0, 0, "no_such_field", RawBytes);

            QByteArray data = QByteArray::fromHex("42");
            QString err;
            QJsonObject result = schema.parse(data, &err);
            CHECK(!err.isEmpty(), "缺失lenField应有错误");
        }
    }

    // ================================================
    // 7. 条件分支 when/otherwise
    // ================================================
    {
        g_stats.startCase("7. 条件分支 when/otherwise");

        // 7.1 基本 when 条件
        {
            ProtocolSchema schema;
            schema.addField("cmd", 0, 0, 8, UInt);

            // cmd=1 -> 解析 value1
            schema.when("cmd", 1)
                  .addField("value1", 1, 0, 8, UInt);

            // cmd=2 -> 解析 value2
            schema.when("cmd", 2)
                  .addField("value2", 1, 0, 16, UInt);

            // 测试 cmd=1
            QByteArray data1 = QByteArray::fromHex("0142");
            QJsonObject r1 = schema.parse(data1);
            CHECK(r1.contains("value1"), "cmd=1: 包含value1");
            CHECK(r1["value1"].toDouble() == 66.0, "cmd=1: value1=66");
            CHECK(!r1.contains("value2"), "cmd=1: 不含value2");

            // 测试 cmd=2
            QByteArray data2 = QByteArray::fromHex("027856");
            QJsonObject r2 = schema.parse(data2);
            CHECK(r2.contains("value2"), "cmd=2: 包含value2");
            CHECK(r2["value2"].toDouble() == 22136.0, "cmd=2: value2=0x5678");
            CHECK(!r2.contains("value1"), "cmd=2: 不含value1");

            // 测试 cmd=99 (不匹配任何条件)
            QByteArray data3 = QByteArray::fromHex("63");
            QJsonObject r3 = schema.parse(data3);
            CHECK(!r3.contains("value1"), "cmd=99: 不含value1");
            CHECK(!r3.contains("value2"), "cmd=99: 不含value2");
        }

        // 7.2 otherwise 默认分支
        {
            ProtocolSchema schema;
            schema.addField("type", 0, 0, 8, UInt);

            schema.when("type", 1)
                  .addField("temp", 1, 0, 16, Int,
                            LittleEndian, Physical, true);

            schema.otherwise()
                  .addField("error_code", 1, 0, 8, UInt);

            // type=1 匹配 when
            QByteArray data1 = QByteArray::fromHex("011A00");
            QJsonObject r1 = schema.parse(data1);
            CHECK(r1.contains("temp"), "type=1: 匹配when包含temp");
            CHECK(!r1.contains("error_code"), "type=1: 不含error_code");

            // type=5 匹配 otherwise
            QByteArray data2 = QByteArray::fromHex("05FF");
            QJsonObject r2 = schema.parse(data2);
            CHECK(r2.contains("error_code"), "type=5: 匹配otherwise包含error_code");
            CHECK(!r2.contains("temp"), "type=5: 不含temp");
        }

        // 7.3 嵌套条件
        {
            ProtocolSchema schema;
            schema.addField("cmd", 0, 0, 8, UInt);

            schema.when("cmd", 1)
                  .addField("sub", 1, 0, 8, UInt)
                  .when("sub", 2)
                  .addField("deep", 2, 0, 8, UInt)
                  .endBranch();

            // 完整匹配: cmd=1, sub=2
            QByteArray data = QByteArray::fromHex("010242");
            QJsonObject r = schema.parse(data);
            CHECK(r.contains("deep"), "嵌套条件: 完整匹配包含deep");
            CHECK(r["deep"].toDouble() == 66.0, "嵌套条件: deep=66");

            // 外层匹配，内层不匹配: cmd=1, sub=3
            QByteArray data2 = QByteArray::fromHex("0103");
            QJsonObject r2 = schema.parse(data2);
            CHECK(!r2.contains("deep"), "嵌套条件: 内层不匹配不含deep");
        }

        // 7.4 条件分支中的变长字段
        {
            ProtocolSchema schema;
            schema.addField("cmd", 0, 0, 8, UInt);

            schema.when("cmd", 1)
                  .addField("data_len", 1, 0, 8, UInt)
                  .addVariableField("payload", 2, 0, "data_len", String);

            QByteArray data = QByteArray::fromHex("010548656C6C6F"); // cmd=1, len=5, "Hello"
            QJsonObject r = schema.parse(data);
            CHECK(r.contains("payload"), "条件变长: 包含payload");
            CHECK(r["payload"].toString() == "Hello", "条件变长: payload=\"Hello\"");
        }
    }

    // ================================================
    // 8. clear() 清空
    // ================================================
    {
        g_stats.startCase("8. clear() 清空");

        ProtocolSchema schema;
        schema.addField("f1", 0, 0, 8);
        schema.addField("f2", 1, 0, 8);

        QByteArray data = QByteArray::fromHex("4243");
        QJsonObject r1 = schema.parse(data);
        CHECK(r1.contains("f1"), "清空前: 包含f1");

        schema.clear();
        QJsonObject r2 = schema.parse(data);
        CHECK(r2.isEmpty(), "清空后: 结果为空");

        // 重新添加应正常工作
        schema.addField("f3", 0, 0, 8);
        QJsonObject r3 = schema.parse(data);
        CHECK(r3.contains("f3"), "重添加后: 包含f3");
    }

    // ================================================
    // 9. checkOverlap 重叠检测
    // ================================================
    {
        g_stats.startCase("9. checkOverlap 重叠检测");

        // 9.1 无重叠
        {
            ProtocolSchema schema;
            schema.addField("f1", 0, 0, 8);
            schema.addField("f2", 1, 0, 8);
            QStringList overlaps = schema.checkOverlap();
            CHECK(overlaps.isEmpty(), "无重叠: 空列表");
        }

        // 9.2 有重叠
        {
            ProtocolSchema schema;
            schema.addField("f1", 0, 0, 16); // 占 bit 0-15
            schema.addField("f2", 0, 0, 8);  // 占 bit 0-7 (重叠)
            QStringList overlaps = schema.checkOverlap();
            CHECK(!overlaps.isEmpty(), "有重叠: 非空列表");
            qDebug() << "重叠字段:" << overlaps;
        }

        // 9.3 部分重叠
        {
            ProtocolSchema schema;
            schema.addField("f1", 0, 4, 8);  // bit 4-11
            schema.addField("f2", 0, 0, 8);  // bit 0-7 (与f1部分重叠)
            QStringList overlaps = schema.checkOverlap();
            CHECK(!overlaps.isEmpty(), "部分重叠: 非空列表");
        }
    }

    // ================================================
    // 10. validateSchema 校验
    // ================================================
    {
        g_stats.startCase("10. validateSchema 校验");

        // 10.1 有效 schema
        {
            ProtocolSchema schema;
            schema.addField("len", 0, 0, 8);
            schema.addVariableField("data", 1, 0, "len");
            QString err;
            bool valid = schema.validateSchema(&err);
            CHECK(valid, "有效schema验证通过");
        }

        // 10.2 简单 schema (无变长)
        {
            ProtocolSchema schema;
            schema.addField("a", 0, 0, 8);
            schema.addField("b", 1, 0, 16);
            QString err;
            bool valid = schema.validateSchema(&err);
            CHECK(valid, "简单schema验证通过");
        }
    }

    // ================================================
    // 11. 错误处理
    // ================================================
    {
        g_stats.startCase("11. 错误处理");

        ProtocolSchema schema;
        QString err;

        // 11.1 空字段名
        schema.addField("", 0, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(!err.isEmpty(), "空字段名: 有错误");

        // 11.2 负数 startByte
        err.clear();
        schema.addField("bad", -1, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(!err.isEmpty(), "负数startByte: 有错误");

        // 11.3 超出范围 startBit
        err.clear();
        schema.addField("bad", 0, 8, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(!err.isEmpty(), "startBit=8: 有错误");

        // 11.4 bitLength 超出范围
        err.clear();
        schema.addField("bad", 0, 0, 65, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(!err.isEmpty(), "bitLength=65: 有错误");

        // 11.5 重复字段名
        err.clear();
        schema.addField("dup", 0, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        err.clear();
        schema.addField("dup", 1, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);
        CHECK(!err.isEmpty(), "重复字段名: 有错误");

        // 11.6 空数据解析
        schema.clear();
        schema.addField("f1", 0, 0, 8);
        QByteArray emptyData;
        err.clear();
        QJsonObject r = schema.parse(emptyData, &err);
        CHECK(!err.isEmpty(), "空数据解析: 有错误");

        // 11.7 不足数据解析
        schema.clear();
        schema.addField("f1", 0, 0, 16);
        QByteArray shortData = QByteArray::fromHex("42"); // 只有1字节
        err.clear();
        r = schema.parse(shortData, &err);
        CHECK(!err.isEmpty(), "不足数据解析: 有错误");

        // 11.8 打包缺失值
        schema.clear();
        schema.addField("f1", 0, 0, 8);
        QJsonObject emptyValues;
        QByteArray packed;
        err.clear();
        bool ok = schema.pack(emptyValues, packed, &err);
        CHECK(!ok, "缺失值打包: 返回失败");
        CHECK(packed.isEmpty(), "缺失值打包: 返回空");
        CHECK(!err.isEmpty(), "缺失值打包: 有错误");
    }

    // ================================================
    // 12. 跨字节位域
    // ================================================
    {
        g_stats.startCase("12. 跨字节位域");

        // 12.1 跨2字节的10位域
        // Physical模式: bit3为LSB, bit12为MSB
        // 0x1F=00011111: bit3=1, bit4=1, bit5=1, bit6=1, bit7=0
        // 0xFF=11111111: bit8=1, bit9=1, bit10=1, bit11=1, bit12=1
        // 结果: 1111100011(二进制) = 995
        {
            ProtocolSchema schema;
            schema.addField("cross", 0, 3, 10, UInt,
                            LittleEndian, Physical);
            QByteArray data = QByteArray::fromHex("1FFF");
            QJsonObject result = schema.parse(data);
            qDebug() << "跨字节10位:" << result["cross"].toDouble();
            CHECK(result["cross"].toDouble() == 995.0, "跨字节10位=995");
        }

        // 12.2 非对齐位域
        {
            ProtocolSchema schema;
            schema.addField("bit3_5", 0, 3, 3, UInt,
                            LittleEndian, Physical);
            schema.addField("bit6_7", 0, 6, 2, UInt,
                            LittleEndian, Physical);
            // 0xF4 = 11110100
            // Physical模式: bit3=0, bit4=1, bit5=1 → 110(二进制)=6
            //               bit6=1, bit7=1 → 11(二进制)=3
            QByteArray data = QByteArray::fromHex("F4");
            QJsonObject result = schema.parse(data);
            CHECK(result["bit3_5"].toDouble() == 6.0, "bit3-5 (0,1,1)=6");
            CHECK(result["bit6_7"].toDouble() == 3.0, "bit6-7 (1,1)=3");
        }
    }

    // ================================================
    // 13. 链式调用
    // ================================================
    {
        g_stats.startCase("13. 链式调用");

        ProtocolSchema schema;
        QString err;

        schema.addField("a", 0, 0, 8, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err)
              .addField("b", 1, 0, 16, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err)
              .addField("c", 3, 0, 32, UInt,
                        LittleEndian, Physical,
                        false, 1.0, 0.0, &err);

        CHECK(err.isEmpty(), "链式调用: 无错误");

        // 打包 (使用显式类型转换，避免unsigned int歧义)
        QJsonObject values;
        values["a"] = QJsonValue(static_cast<int>(0x00AA));       // 170
        values["b"] = QJsonValue(static_cast<int>(0x1234));       // 4660
        values["c"] = QJsonValue(static_cast<qlonglong>(0xDEADBEEF)); // 3735928559
        QByteArray packed = schema.packToArray(values, &err);
        CHECK(!packed.isEmpty(), "链式打包: 非空");
        CHECK(packed.size() == 7, QString("链式打包: 长度=7 (实际=%1)").arg(packed.size()));

        // 解析验证
        QJsonObject result = schema.parse(packed, &err);
        CHECK(result["a"].toDouble() == 170.0, "链式往返: a正确");
        CHECK(result["b"].toDouble() == 4660.0, "链式往返: b正确");
        CHECK(result["c"].toDouble() == 3735928559.0, "链式往返: c正确");
    }

    // ================================================
    // 14. 真实协议场景
    // ================================================
    {
        g_stats.startCase("14. 真实协议场景");

        // 模拟传感器数据包
        // Byte 0:   同步字节 (0xAA = 170)
        // Byte 1:   传感器ID
        // Byte 2-3: 温度 (有符号16位小端, 系数0.1, 偏移0)
        // Byte 4-5: 气压 (无符号16位小端, 系数1, 偏移0)
        // Byte 6:   状态标志 (bit0=就绪, bit1=错误)
        ProtocolSchema schema;
        schema.addField("sync", 0, 0, 8, UInt);
        schema.addField("sensor_id", 1, 0, 8, UInt);
        schema.addField("temperature", 2, 0, 16, Int,
                        LittleEndian, Physical,
                        true, 0.1, 0.0);
        schema.addField("pressure", 4, 0, 16, UInt,
                        LittleEndian, Physical);
        schema.addField("status_ready", 6, 0, 1, UInt,
                        LittleEndian, Physical);
        schema.addField("status_error", 6, 1, 1, UInt,
                        LittleEndian, Physical);

        // 构建测试包: sync=0xAA, id=5, temp=25.6°C (raw=256=0x0100),
        //            pressure=1013 (0x03F5), ready=1, error=0
        // 数据: AA 05 00 01 F5 03 01
        QByteArray packet = QByteArray::fromHex("AA05 0001 F503 01");
        QString err;
        QJsonObject result = schema.parse(packet, &err);
        CHECK(err.isEmpty(), "传感器协议: 解析无错误");
        CHECK(result["sync"].toDouble() == 170.0, "传感器: sync=0xAA=170");
        CHECK(result["sensor_id"].toDouble() == 5.0, "传感器: id=5");
        double temp = result["temperature"].toDouble();
        CHECK(qAbs(temp - 25.6) < 0.1, QString("传感器: temp=25.6(实际=%1)").arg(temp));
        CHECK(result["pressure"].toDouble() == 1013.0, "传感器: pressure=1013");
        CHECK(result["status_ready"].toDouble() == 1.0, "传感器: ready=1");
        CHECK(result["status_error"].toDouble() == 0.0, "传感器: error=0");

        // 打包往返 (使用显式类型转换避免歧义)
        QJsonObject tx;
        tx["sync"] = QJsonValue(static_cast<int>(0x00AA));
        tx["sensor_id"] = QJsonValue(5);
        tx["temperature"] = QJsonValue(25.6);
        tx["pressure"] = QJsonValue(1013);
        tx["status_ready"] = QJsonValue(1);
        tx["status_error"] = QJsonValue(0);
        QByteArray txPacket = schema.packToArray(tx, &err);
        CHECK(!txPacket.isEmpty(), "传感器: 打包非空");

        // 再解析验证
        QJsonObject rx = schema.parse(txPacket, &err);
        CHECK(qAbs(rx["temperature"].toDouble() - 25.6) < 0.1,
              "传感器: 打包往返温度一致");
        CHECK(rx["pressure"].toDouble() == 1013.0,
              "传感器: 打包往返气压一致");
    }

    // ================================================
    // 15. 性能简单测试
    // ================================================
    {
        g_stats.startCase("15. 性能简单测试");

        ProtocolSchema schema;
        for (int i = 0; i < 200; i++) {
            schema.addField(QString("f_%1").arg(i), i, 0, 8, UInt);
        }

        QByteArray data(200, '\x2A');
        QElapsedTimer timer;
        timer.start();
        QJsonObject result = schema.parse(data);
        qint64 elapsed = timer.elapsed();
        qDebug() << "200字段解析耗时:" << elapsed << "ms";
        CHECK(result.contains("f_0"), "性能测试: f_0存在");
        CHECK(result.contains("f_199"), "性能测试: f_199存在");
    }

    // ================================================
    // 输出测试汇总
    // ================================================
    g_stats.summary();

    return g_stats.failed > 0 ? 1 : 0;
}
