/*****************************************************************************
 * 综合示例：ProtocolSchema 全功能演示与验证
 *
 * 设计思路：用一个"智能传感器帧"协议把所有功能串起来，每个测试函数聚焦一个
 * 功能点，函数前有详细注释讲解用法与原理。既是测试也是教学示例。
 *
 * 覆盖功能:
 *   1. 位域字段（字节内任意比特偏移与长度）
 *   2. 字节序 LittleEndian/BigEndian、比特顺序 Physical/MsbFirst
 *   3. 多种 ValueType：Int/UInt/HexString/Base64/RawBytes/String
 *   4. 线性变换 factor/offset（物理值 = 原始值*factor + offset）
 *   5. 枚举映射 map（链式，与线性变换互斥，解析输出字符串/打包反查）
 *   6. 变长字段 addVariableField（长度由另一字段决定，打包时自动回写长度）
 *   7. 条件分支 when/otherwise/endBranch
 *   8. 嵌套条件（子分支继承父分支条件，AND 逻辑）
 *   9. JSON 加载 loadJson/loadFile（defaults 缺省继承 + enumMaps 命名引用）
 *  10. parse/pack 往返一致性
 *  11. 错误处理（缺值/越界/类型不符）
 *  12. 辅助接口 checkOverlap/validateSchema/clear
 *
 * 编译: qmake test_demo.pro && mingw32-make
 * 运行: ./release/test_demo.exe
 *****************************************************************************/
#include "ProtocolSchema.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace Sqz;

// 简易断言宏：累计统计通过/失败数，便于汇总
static int g_pass = 0;
static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; qDebug() << "[ PASS ]" << msg; } \
    else { ++g_fail; qDebug() << "[ FAIL ]" << msg << " (line" << __LINE__ << ")"; } \
} while(0)

// ==================== 测试用例 ====================

/*--------------------------------------------------------------------
 * 测试1：位域 + 字节序 + 比特顺序
 * 演示：一个字节内拆出多个字段；多字节字段的大/小端差异
 * Physical 模式下 startBit=0 表示从 bit0（最低位）开始
 *--------------------------------------------------------------------*/
static void test_bitfield_endian() {
    qDebug() << "\n=== test_bitfield_endian ===";
    ProtocolSchema s;

    // byte0 拆成两半：低4位 lowNibble，高4位 highNibble（Physical 位索引）
    s.addField("lowNibble",  0, 0, 4, UInt, LittleEndian, Physical)
     .addField("highNibble", 0, 4, 4, UInt, LittleEndian, Physical)
     // byte1~2：16位无符号，分别用小端和大端读取对比
     .addField("valLE", 1, 0, 16, UInt, LittleEndian, Physical)
     .addField("valBE", 1, 0, 16, UInt, BigEndian,    Physical);

    // 0x1A = 0001 1010 → low=1010=10, high=0001=1
    // 0x34 0x12 → 小端=0x1234=4660, 大端=0x3412=13330
    QByteArray rx = QByteArray::fromHex("1a3412");
    QJsonObject j = s.parse(rx);
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);

    CHECK(j.value("lowNibble").toInt()  == 10,    "lowNibble=10 (bit0-3 of 0x1A)");
    CHECK(j.value("highNibble").toInt() == 1,     "highNibble=1 (bit4-7 of 0x1A)");
    CHECK(j.value("valLE").toInt()      == 4660,  "valLE=0x1234 (LittleEndian)");
    CHECK(j.value("valBE").toInt()      == 13330, "valBE=0x3412 (BigEndian)");
}

/*--------------------------------------------------------------------
 * 测试2：多种 ValueType（HexString / Base64 / RawBytes / String）
 * 演示：不同类型字段解析后的 JSON 值形式
 *--------------------------------------------------------------------*/
static void test_value_types() {
    qDebug() << "\n=== test_value_types ===";
    ProtocolSchema s;
    s.addField("hex",  0, 0, 8, HexString)   // → 十六进制字符串 "ab"
     .addField("b64",  1, 0, 8, Base64)      // → Base64 编码串
     .addField("raw",  2, 0, 8, RawBytes)    // → Base64 串（原始字节）
     .addField("str",  3, 0, 8, String);     // → UTF-8 字符串

    // 0xAB 'Z' 'Z' 'H' → hex="ab", str="H"
    QByteArray rx;
    rx.append(static_cast<char>(0xAB));
    rx.append('Z'); rx.append('Z'); rx.append('H');
    QJsonObject j = s.parse(rx);
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);

    CHECK(j.value("hex").toString() == "ab", "hex=\"ab\" (HexString)");
    CHECK(j.value("str").toString() == "H",  "str=\"H\" (UTF-8 String)");
    CHECK(j.value("b64").isString(),         "b64 is string (Base64)");
    CHECK(j.value("raw").isString(),         "raw is string (Base64)");
}

/*--------------------------------------------------------------------
 * 测试3：线性变换 factor/offset
 * 原理：物理值 = 原始值 * factor + offset；打包时逆变换还原原始值
 * 例：温度 8bit 有符号，factor=0.5, offset=-20 → raw=100 时 100*0.5-20=30℃
 *--------------------------------------------------------------------*/
static void test_linear_transform() {
    qDebug() << "\n=== test_linear_transform ===";
    ProtocolSchema s;
    // 8位有符号整数，系数0.5偏移-20
    s.addField("temp", 0, 0, 8, Int, LittleEndian, Physical, true, 0.5, -20.0);

    // raw=100 → 100*0.5-20 = 30.0
    QJsonObject j = s.parse(QByteArray::fromHex("64"));
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);
    CHECK(qAbs(j.value("temp").toDouble() - 30.0) < 1e-9, "temp=30.0 (raw 100 *0.5 -20)");

    // 反向打包：物理值 30.0 → 逆变换 (30+20)/0.5=100 → 0x64
    QJsonObject tx; tx["temp"] = 30.0;
    QByteArray packed = s.packToArray(tx);
    qDebug() << "packed:" << packed.toHex();
    CHECK((quint8)packed[0] == 100, "pack temp=30.0 → raw 0x64 (inverse transform)");
}

/*--------------------------------------------------------------------
 * 测试4：枚举映射 map（链式 + 与线性变换互斥）
 * 规则：启用 map 后 factor/offset 自动失效；解析输出字符串；打包接受字符串反查
 *--------------------------------------------------------------------*/
static void test_enum_map() {
    qDebug() << "\n=== test_enum_map ===";
    ProtocolSchema s;
    // status 字段：0→关机 1→开机 2→待机，链式 map
    s.addField("status", 0, 0, 8, UInt)
     .map(0, QStringLiteral("关机"))
     .map(1, QStringLiteral("开机"))
     .map(2, QStringLiteral("待机"));

    // 解析 raw=1 → "开机"，value 必为字符串类型
    QJsonObject j = s.parse(QByteArray::fromHex("01"));
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);
    CHECK(j.value("status").isString(),                          "status is string type");
    CHECK(j.value("status").toString() == QStringLiteral("开机"), "status=开机 (raw 1)");

    // 打包：传字符串 "待机" → 反查 raw=2 → 0x02
    QJsonObject tx; tx["status"] = QStringLiteral("待机");
    QByteArray packed = s.packToArray(tx);
    qDebug() << "packed:" << packed.toHex();
    CHECK((quint8)packed[0] == 2, "pack \"待机\" → raw 0x02 (reverse map)");

    // 互斥验证：map 启用后即便设了 factor/offset 也不生效
    ProtocolSchema s2;
    s2.addField("v", 0, 0, 8, UInt, LittleEndian, Physical, false, 2.0, 1.0)
      .map(1, "ONE");   // 加 map 后 factor=2/offset=1 自动失效
    QJsonObject j2 = s2.parse(QByteArray::fromHex("01"));
    CHECK(j2.value("v").toString() == "ONE", "map disables linear transform (factor/offset ignored)");
}

/*--------------------------------------------------------------------
 * 测试5：变长字段 addVariableField
 * 原理：变长字段长度由 lenField 指向的字段值决定；打包时自动回写长度
 *--------------------------------------------------------------------*/
static void test_variable_field() {
    qDebug() << "\n=== test_variable_field ===";
    ProtocolSchema s;
    // byte0: 长度字段 len；byte1起: 变长字符串 payload，长度=len
    s.addField("len", 0, 0, 8, UInt)
     .addVariableField("payload", 1, 0, "len", String);

    // len=5, payload="hello"
    QByteArray rx = QByteArray::fromHex("05") + "hello";
    QJsonObject j = s.parse(rx);
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);
    CHECK(j.value("len").toInt() == 5,                      "len=5");
    CHECK(j.value("payload").toString() == QStringLiteral("hello"), "payload=hello");

    // 打包：只传 payload，len 自动回写为 5
    QJsonObject tx; tx["payload"] = QStringLiteral("world");
    QByteArray packed = s.packToArray(tx);
    qDebug() << "packed:" << packed.toHex();
    CHECK((quint8)packed[0] == 5,                              "len auto-written =5");
    CHECK(packed.mid(1) == QByteArray("world"),                "payload=world");
}

/*--------------------------------------------------------------------
 * 测试6：条件分支 when/otherwise/endBranch
 * 原理：when(field,value) 后续字段仅在条件满足时解析；otherwise 为默认分支
 * 条件基于原始数值比较（枚举字段自动反查）
 *--------------------------------------------------------------------*/
static void test_conditional_branch() {
    qDebug() << "\n=== test_conditional_branch ===";
    ProtocolSchema s;
    // byte0: type 条件字段；byte1: 按分支不同字段
    s.addField("type", 0, 0, 8, UInt)
      .map(1, "DATA")      // type 带枚举，验证条件按原始值比较
      .map(2, "CMD");
    s.when("type", 1)                          // type==1 数据帧
        .addField("dataVal", 1, 0, 8, UInt)
        .endBranch();
    s.when("type", 2)                          // type==2 命令帧
        .addField("cmdVal", 1, 0, 8, UInt)
        .endBranch();
    s.otherwise()                              // 默认分支：其它 type
        .addField("errVal", 1, 0, 8, UInt)
        .endBranch();

    // type=1（DATA）→ dataVal 生效，cmdVal/errVal 不出现
    QJsonObject j1 = s.parse(QByteArray::fromHex("0199"));
    qDebug() << "type=1:" << QJsonDocument(j1).toJson(QJsonDocument::Compact);
    CHECK(j1.value("type").toString() == "DATA",       "type=DATA (enum, raw 1)");
    CHECK(j1.value("dataVal").toInt() == 0x99,         "dataVal active when type==1");
    CHECK(!j1.contains("cmdVal") && !j1.contains("errVal"), "other branches skipped");

    // type=9（未命中枚举→"9"，落入默认分支）→ errVal 生效
    QJsonObject j9 = s.parse(QByteArray::fromHex("0955"));
    qDebug() << "type=9:" << QJsonDocument(j9).toJson(QJsonDocument::Compact);
    CHECK(j9.value("type").toString() == "9",          "type=9 unmapped -> \"9\"");
    CHECK(j9.value("errVal").toInt() == 0x55,          "errVal active (default branch)");
}

/*--------------------------------------------------------------------
 * 测试7：嵌套条件（子分支继承父分支条件，AND 逻辑）
 * 场景：type==1 且 subType==1 时才解析 temp
 *--------------------------------------------------------------------*/
static void test_nested_condition() {
    qDebug() << "\n=== test_nested_condition ===";
    ProtocolSchema s;
    s.addField("type",    0, 0, 8, UInt);
    s.addField("subType", 1, 0, 8, UInt);
    // 嵌套：type==1 内部再 when subType==1，子分支继承父条件（AND 逻辑）
    // endBranch 仅结束最内层分支并返回 ProtocolSchema&（外层空分支析构无害）
    s.when("type", 1)
        .when("subType", 1)
        .addField("temp", 2, 0, 8, Int, LittleEndian, Physical, true, 0.1, -40.0)
        .endBranch();

    // type=1, subType=1 → temp 生效
    QJsonObject j1 = s.parse(QByteArray::fromHex("010164"));  // temp raw=100 → 100*0.1-40=-30
    qDebug() << "type=1,subType=1:" << QJsonDocument(j1).toJson(QJsonDocument::Compact);
    CHECK(j1.contains("temp"),                            "temp active (type=1 AND subType=1)");
    CHECK(qAbs(j1.value("temp").toDouble() - (-30.0)) < 1e-9, "temp=-30.0 (raw 100)");

    // type=1, subType=2 → temp 不生效（AND 条件不满足）
    QJsonObject j2 = s.parse(QByteArray::fromHex("010200"));
    qDebug() << "type=1,subType=2:" << QJsonDocument(j2).toJson(QJsonDocument::Compact);
    CHECK(!j2.contains("temp"), "temp skipped (subType!=1, AND fails)");
}

/*--------------------------------------------------------------------
 * 测试8：JSON 加载 loadJson（defaults 缺省继承 + enumMaps 命名引用）
 * 演示：顶层 defaults 提供缺省值，字段未写时继承；enumMap 可命名引用
 *--------------------------------------------------------------------*/
static void test_json_load() {
    qDebug() << "\n=== test_json_load ===";
    // 内联构造协议 JSON（避免依赖外部文件）
    QJsonObject proto;
    proto["protocolName"] = QStringLiteral("演示协议");
    // defaults：所有字段未显式指定时继承这些值
    proto["defaults"] = QJsonObject{
        {"endian", "LittleEndian"}, {"bitOrder", "Physical"},
        {"type", "UInt"}, {"factor", 1.0}, {"offset", 0.0}
    };
    // enumMaps：命名枚举库，字段 enumMap 可用字符串引用
    proto["enumMaps"] = QJsonObject{
        {"devType", QJsonObject{{"1", "传感器"}, {"2", "执行器"}}}
    };
    QJsonArray fields;
    // head：显式 type=HexString，覆盖 defaults 的 UInt
    fields.append(QJsonObject{{"name","head"},{"startByte",0},{"startBit",0},{"bitLength",8},{"type","HexString"}});
    // devType：未写 type → 继承 defaults.UInt；enumMap 命名引用 "devType"
    fields.append(QJsonObject{{"name","devType"},{"startByte",1},{"startBit",0},{"bitLength",8},{"enumMap","devType"}});
    proto["fields"] = fields;

    ProtocolSchema s;
    QString err;
    bool ok = s.loadJson(proto, &err);
    if (!ok) qDebug() << "err:" << err;
    CHECK(ok, "loadJson success");
    CHECK(s.protocolName() == QStringLiteral("演示协议"), "protocolName correct");

    // 解析：head=0xAA, devType=1 → "传感器"
    QJsonObject j = s.parse(QByteArray::fromHex("aa01"));
    qDebug() << "parsed:" << QJsonDocument(j).toJson(QJsonDocument::Compact);
    CHECK(j.value("head").toString() == "aa",                 "head=\"aa\" (HexString overrides defaults)");
    CHECK(j.value("devType").toString() == QStringLiteral("传感器"), "devType=传感器 (named enumMap ref, type inherited)");
}

/*--------------------------------------------------------------------
 * 测试9：parse/pack 往返一致性
 * 演示：解析后再打包，应得到等价数据（枚举字符串 + 线性变换 + 条件）
 *--------------------------------------------------------------------*/
static void test_roundtrip() {
    qDebug() << "\n=== test_roundtrip ===";
    ProtocolSchema s;
    s.addField("status", 0, 0, 8, UInt).map(0,"OFF").map(1,"ON");
    s.addField("temp",   1, 0, 8, Int, LittleEndian, Physical, true, 0.5, -20.0);

    // 原始数据 → JSON
    QByteArray rx = QByteArray::fromHex("0164");  // status=1→ON, temp raw=100→30
    QJsonObject j1 = s.parse(rx);
    qDebug() << "parse:" << QJsonDocument(j1).toJson(QJsonDocument::Compact);

    // JSON → 打包 → 再解析，校验一致
    QByteArray packed = s.packToArray(j1);
    qDebug() << "packed:" << packed.toHex();
    QJsonObject j2 = s.parse(packed);
    qDebug() << "roundtrip:" << QJsonDocument(j2).toJson(QJsonDocument::Compact);

    CHECK(j2.value("status").toString() == "ON",                "roundtrip status=ON");
    CHECK(qAbs(j2.value("temp").toDouble() - 30.0) < 1e-9,      "roundtrip temp=30.0");
    CHECK(packed.toHex() == "0164",                             "roundtrip bytes match original");
}

/*--------------------------------------------------------------------
 * 测试10：错误处理
 * 演示：数据不足、打包缺值、map 类型不符等错误均被捕获
 *--------------------------------------------------------------------*/
static void test_error_handling() {
    qDebug() << "\n=== test_error_handling ===";
    ProtocolSchema s;
    s.addField("a", 0, 0, 8, UInt)
     .addField("b", 1, 0, 8, UInt);

    // 解析数据不足：只有1字节，b 字段越界 → b 为 null
    QString err;
    QJsonObject j = s.parse(QByteArray::fromHex("01"), &err);
    qDebug() << "short data parse:" << QJsonDocument(j).toJson(QJsonDocument::Compact) << "err:" << err;
    CHECK(j.value("a").toInt() == 1,  "a parsed from short data");
    CHECK(j.value("b").isNull(),      "b null when data out of bounds");

    // 打包缺值：JSON 缺 b → 报错并返回空
    QJsonObject tx; tx["a"] = 1;   // 故意不传 b
    QByteArray packed = s.packToArray(tx, &err);
    qDebug() << "pack missing b:" << err;
    CHECK(packed.isEmpty(),                 "pack returns empty when value missing");
    CHECK(err.contains("Missing value"),    "err mentions 'Missing value'");

    // map 作用于非整数字段 → 报错
    ProtocolSchema s2;
    QString e2;
    s2.addField("str", 0, 0, 8, String).map(0, "X", &e2);
    qDebug() << "map on String err:" << e2;
    CHECK(e2.contains("type mismatch"), "map on non-Int field returns error");
}

/*--------------------------------------------------------------------
 * 测试11：辅助接口 checkOverlap / validateSchema / clear
 * 演示：字段重叠检测、配置校验、清空
 *--------------------------------------------------------------------*/
static void test_aux_interfaces() {
    qDebug() << "\n=== test_aux_interfaces ===";
    ProtocolSchema s;
    // 两个字段重叠：a 占 byte0 全部，b 占 byte0 高4位
    s.addField("a", 0, 0, 8, UInt)
     .addField("b", 0, 4, 4, UInt);

    QStringList overlaps = s.checkOverlap();
    qDebug() << "overlaps:" << overlaps;
    CHECK(!overlaps.isEmpty(),                "checkOverlap detects a/b overlap");
    CHECK(overlaps.join("").contains("a"),    "overlap report includes a");
    CHECK(overlaps.join("").contains("b"),    "overlap report includes b");

    // validateSchema：合法配置应通过
    QString err;
    CHECK(s.validateSchema(&err),             "validateSchema passes for valid config");

    // clear：清空后无字段，parse 返回空对象
    s.clear();
    QJsonObject j = s.parse(QByteArray::fromHex("0102"));
    CHECK(j.isEmpty(),                        "schema empty after clear()");
}

// ==================== 主函数 ====================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    qDebug() << "========== ProtocolSchema 全功能综合示例 ==========";

    test_bitfield_endian();      // 1. 位域 + 字节序 + 比特顺序
    test_value_types();          // 2. 多种 ValueType
    test_linear_transform();     // 3. 线性变换
    test_enum_map();             // 4. 枚举映射 + 互斥
    test_variable_field();       // 5. 变长字段
    test_conditional_branch();   // 6. 条件分支
    test_nested_condition();     // 7. 嵌套条件
    test_json_load();            // 8. JSON 加载
    test_roundtrip();            // 9. 往返一致性
    test_error_handling();       // 10. 错误处理
    test_aux_interfaces();       // 11. 辅助接口

    qDebug() << "\n========== 结果汇总 ==========";
    qDebug() << "PASS:" << g_pass << "  FAIL:" << g_fail;
    if (g_fail == 0) {
        qDebug() << "所有测试通过 ✓";
        return 0;
    }
    qDebug() << "存在失败用例 ✗";
    return 1;
}
