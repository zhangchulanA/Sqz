#include <QtTest>
#include "FlexData.h"
#include <QThread>
#include <QAtomicInt>
#include <QList>
#include <QHash>
#include <QByteArray>
#include <QString>

class FlexDataTest : public QObject
{
    Q_OBJECT

private slots:
    // ========== 1. 基础类型与转换 ==========
    void testBasicTypes();
    void testSafeTypeConversions();

    // ========== 2. 核心修复：隐式共享（写时复制） ==========
    void testImplicitSharing_MapModify();
    void testImplicitSharing_ArrayModify();
    void testImplicitSharing_PathSet();
    void testImplicitSharing_Remove();

    // ========== 3. 路径访问与转义修复 ==========
    void testPath_BasicReadWrite();
    void testPath_EscapeSlash();       // 覆盖 QRegExp → QRegularExpression 转义修复
    void testPath_AutoCreateNodes();
    void testPath_RemovePath();
    void testPath_ArrayIndex();

    // ========== 4. Schema 校验修复 ==========
    void testSchema_ShortForm();        // 覆盖简写模式校验对象错误修复
    void testSchema_FullForm();
    void testSchema_ApplyDefaults();

    // ========== 5. 序列化修复 ==========
    void testJson_IntTypePreservation(); // 覆盖 JSON 整数还原修复
    void testJson_TopLevelValue();       // 覆盖顶层基础类型序列化修复
    void testXml_TypePreservation();     // 覆盖 XML type 属性保留修复
    void testIni_Utf8AndStructure();     // 覆盖 INI 编码与 Windows 独占修复
    void testBinary_Consistency();       // 覆盖 QDataStream 固定版本

    // ========== 6. 合并、差量与补丁 ==========
    void testMerge_Recursive();
    void testDiffAndPatch_Consistency();

    // ========== 7. 深拷贝与快照 ==========
    void testClone_DeepCopy();

    // ========== 8. 拷贝构造/赋值修复（QMutex 导致的删除问题）==========
    void testCopyConstructor_Valid();
    void testAssignmentOperator_Valid();
    void testContainer_Storage();        // 存入 QList/QHash 验证构造可用性

    // ========== 9. 线程安全（路径缓存锁）==========
    void testThreadSafe_ConcurrentRead();

    // ========== 10. 边界与异常场景 ==========
    void testBoundary_NegativeIndex();
    void testBoundary_NullOperations();
    void testDeserialization_InvalidInput();
};

// ================================================================
// 1. 基础类型与转换
// ================================================================
void FlexDataTest::testBasicTypes()
{
    QVERIFY(FlexData().isNull());
    QVERIFY(FlexData(true).isBool());
    QVERIFY(FlexData(42).isInt());
    QVERIFY(FlexData(3.14).isDouble());
    QVERIFY(FlexData("hello").isString());
    QVERIFY(FlexData(QByteArray("bin")).isByteArray());
    QVERIFY(FlexData(QDateTime::currentDateTime()).isDateTime());
    QVERIFY(FlexData(QUuid::createUuid()).isUuid());
    QVERIFY(FlexData(QHash<QString, FlexData>()).isMap());
    QVERIFY(FlexData(QList<FlexData>()).isArray());
}

void FlexDataTest::testSafeTypeConversions()
{
    QCOMPARE(FlexData(true).toInt(), 1);
    QCOMPARE(FlexData(0).toBool(), false);
    QCOMPARE(FlexData("123").toInt(), 123);
    QCOMPARE(FlexData("3.14").toDouble(), 3.14);
    QCOMPARE(FlexData(456).toString(), QString("456"));
    QCOMPARE(FlexData().toInt(999), 999); // 默认值生效
}

// ================================================================
// 2. 隐式共享（写时复制）核心修复验证
// ================================================================
void FlexDataTest::testImplicitSharing_MapModify()
{
    FlexData original;
    original["name"] = "origin";

    FlexData copy = original;
    copy["name"] = "modified";

    // 核心验证：修改副本不影响原对象
    QCOMPARE(original["name"].toString(), QString("origin"));
    QCOMPARE(copy["name"].toString(), QString("modified"));
}

void FlexDataTest::testImplicitSharing_ArrayModify()
{
    FlexData original;
    original.append(1);
    original.append(2);

    FlexData copy = original;
    copy.append(3);

    QCOMPARE(original.arraySize(), 2);
    QCOMPARE(copy.arraySize(), 3);
}

void FlexDataTest::testImplicitSharing_PathSet()
{
    FlexData original;
    original.set("/user/age", 20);

    FlexData copy = original;
    copy.set("/user/age", 30);

    QCOMPARE(original.get("/user/age").toInt(), 20);
    QCOMPARE(copy.get("/user/age").toInt(), 30);
}

void FlexDataTest::testImplicitSharing_Remove()
{
    FlexData original;
    original["a"] = 1;
    original["b"] = 2;

    FlexData copy = original;
    copy.remove("a");

    QVERIFY(original.contains("a"));
    QVERIFY(!copy.contains("a"));
}

// ================================================================
// 3. 路径访问与转义修复
// ================================================================
void FlexDataTest::testPath_BasicReadWrite()
{
    FlexData data;
    data.set("/server/config/port", 8080);
    data.set("/server/config/host", "127.0.0.1");

    QCOMPARE(data.get("/server/config/port").toInt(), 8080);
    QCOMPARE(data.get("/server/config/host").toString(), QString("127.0.0.1"));
    QVERIFY(data.has("/server/config/port"));
    QVERIFY(!data.has("/server/config/none"));
}

void FlexDataTest::testPath_EscapeSlash()
{
    FlexData data;
    // 键名包含斜杠，使用转义符
    data.set("a\\/b", 123);

    QVERIFY(data.contains("a/b"));
    QCOMPARE(data["a/b"].toInt(), 123);
    QCOMPARE(data.get("a\\/b").toInt(), 123);

    // 删除转义路径
    data.removePath("a\\/b");
    QVERIFY(!data.contains("a/b"));
}

void FlexDataTest::testPath_AutoCreateNodes()
{
    FlexData data;
    data.set("/a/b/c/d", "deep");
    QVERIFY(data.isMap());
    QVERIFY(data["a"]["b"]["c"].contains("d"));
    QCOMPARE(data["a"]["b"]["c"]["d"].toString(), QString("deep"));
}

void FlexDataTest::testPath_RemovePath()
{
    FlexData data;
    data.set("/user/name", "test");
    data.set("/user/age", 20);

    data.removePath("/user/age");
    QVERIFY(!data.has("/user/age"));
    QVERIFY(data.has("/user/name"));
}

void FlexDataTest::testPath_ArrayIndex()
{
    FlexData data;
    data.set("/list/0", "first");
    data.set("/list/1", "second");

    QCOMPARE(data.get("/list/0").toString(), QString("first"));
    QCOMPARE(data.get("/list/1").toString(), QString("second"));
    QCOMPARE(data["list"].arraySize(), 2);
}

// ================================================================
// 4. Schema 校验修复
// ================================================================
void FlexDataTest::testSchema_ShortForm()
{
    FlexData data;
    data["name"] = "张三";
    data["age"] = "not_number"; // 字符串，不符合 int 类型

    FlexData schema;
    schema["name"] = "string";
    schema["age"] = "int";

    QStringList errors;
    bool valid = data.validate(schema, &errors);

    QVERIFY(!valid);
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains("age"));
}

void FlexDataTest::testSchema_FullForm()
{
    FlexData data;
    data["level"] = 150;

    FlexData schema;
    FlexData rule;
    rule["type"] = "int";
    rule["required"] = true;
    rule["min"] = 0;
    rule["max"] = 100;
    schema["level"] = rule;

    QStringList errors;
    bool valid = data.validate(schema, &errors);
    QVERIFY(!valid);
    QVERIFY(errors.first().contains("out of range"));
}

void FlexDataTest::testSchema_ApplyDefaults()
{
    FlexData data;
    data["name"] = "test";

    FlexData schema;
    FlexData rule;
    rule["type"] = "int";
    rule["default"] = 0;
    schema["count"] = rule;

    FlexData result = data.applyDefaults(schema);
    QVERIFY(result.contains("count"));
    QCOMPARE(result["count"].toInt(), 0);
}

// ================================================================
// 5. 序列化修复
// ================================================================
void FlexDataTest::testJson_IntTypePreservation()
{
    FlexData original;
    original["id"] = 100;
    original["price"] = 99.9;

    QByteArray json = original.toJson(true);
    FlexData restored;
    QVERIFY(restored.fromJson(json));

    QVERIFY(restored["id"].isInt());
    QVERIFY(restored["price"].isDouble());
    QCOMPARE(restored["id"].toInt(), 100);
}

void FlexDataTest::testJson_TopLevelValue()
{
    FlexData original(12345);
    QByteArray json = original.toJson();
    QVERIFY(!json.isEmpty());

    FlexData restored;
    QVERIFY(restored.fromJson(json));
    QCOMPARE(restored.toInt(), 12345);
}

void FlexDataTest::testXml_TypePreservation()
{
    FlexData original;
    original["enabled"] = true;
    original["count"] = 42;
    original["ratio"] = 0.85;

    QByteArray xml = original.toXml("root");
    FlexData restored;
    QVERIFY(restored.fromXml(xml));

    QVERIFY(restored["enabled"].isBool());
    QVERIFY(restored["count"].isInt());
    QVERIFY(restored["ratio"].isDouble());
    QCOMPARE(restored["count"].toInt(), 42);
    QCOMPARE(restored["enabled"].toBool(), true);
}

void FlexDataTest::testIni_Utf8AndStructure()
{
    FlexData original;
    FlexData server;
    server["port"] = 8080;
    server["host"] = "本地地址"; // 中文验证
    original["server"] = server;

    QString ini = original.toIni();
    QVERIFY(!ini.isEmpty());

    FlexData restored;
    QVERIFY(restored.fromIni(ini));
    QCOMPARE(restored["server"]["port"].toInt(), 8080);
    QCOMPARE(restored["server"]["host"].toString(), QString("本地地址"));
}

void FlexDataTest::testBinary_Consistency()
{
    FlexData original;
    original.set("/user/name", "test");
    original.set("/user/age", 25);
    original["tags"].append("cpp");
    original["tags"].append("qt");

    QByteArray bin = original.serialize();
    FlexData restored;
    QVERIFY(restored.deserialize(bin));

    QVERIFY(original == restored);
}

// ================================================================
// 6. 合并、差量与补丁
// ================================================================
void FlexDataTest::testMerge_Recursive()
{
    FlexData base;
    base.set("/a/b", 1);
    base.set("/a/c", 2);

    FlexData other;
    other.set("/a/c", 99);
    other.set("/a/d", 3);

    FlexData merged = base.merge(other);
    QCOMPARE(merged.get("/a/b").toInt(), 1);
    QCOMPARE(merged.get("/a/c").toInt(), 99);
    QCOMPARE(merged.get("/a/d").toInt(), 3);
}

void FlexDataTest::testDiffAndPatch_Consistency()
{
    FlexData origin;
    origin.set("/user/name", "old");
    origin.set("/user/age", 20);

    FlexData target;
    target.set("/user/name", "new");
    target.set("/user/city", "beijing");

    FlexData patch = origin.diff(target);
    QVERIFY(patch.isArray());
    QVERIFY(patch.arraySize() > 0);

    FlexData result = origin.patch(patch);
    QVERIFY(result == target);
}

// ================================================================
// 7. 深拷贝与快照
// ================================================================
void FlexDataTest::testClone_DeepCopy()
{
    FlexData original;
    original["value"] = 100;

    FlexData cloned = original.clone();
    original["value"] = 200;

    QCOMPARE(cloned["value"].toInt(), 100);
}

// ================================================================
// 8. 拷贝构造/赋值修复
// ================================================================
void FlexDataTest::testCopyConstructor_Valid()
{
    FlexData a;
    a["key"] = "value";
    FlexData b(a);

    QCOMPARE(b["key"].toString(), QString("value"));
    b["key"] = "new";
    QCOMPARE(a["key"].toString(), QString("value"));
}

void FlexDataTest::testAssignmentOperator_Valid()
{
    FlexData a;
    a["key"] = "value";
    FlexData b;
    b = a;

    QCOMPARE(b["key"].toString(), QString("value"));
    b["key"] = "new";
    QCOMPARE(a["key"].toString(), QString("value"));
}

void FlexDataTest::testContainer_Storage()
{
    // 验证可正常存入 Qt 容器（间接验证拷贝构造可用）
    QList<FlexData> list;
    list.append(FlexData(1));
    list.append(FlexData("test"));
    QCOMPARE(list.size(), 2);

    QHash<QString, FlexData> hash;
    hash["k1"] = FlexData(true);
    QVERIFY(hash["k1"].isBool());
}

// ================================================================
// 9. 线程安全（路径缓存锁）
// ================================================================
void FlexDataTest::testThreadSafe_ConcurrentRead()
{
    FlexData data;
    data.set("/a/b/c", 123);
    data.set("/x/y/z", "test_str");

    const int threadCount = 10;
    const int iterations = 2000;
    QList<QThread*> threads;
    QAtomicInt errorCount(0);

    for (int i = 0; i < threadCount; ++i) {
        QThread* t = QThread::create([&](){
            for (int j = 0; j < iterations; ++j) {
                FlexData v1 = data.get("/a/b/c");
                FlexData v2 = data.get("/x/y/z");
                if (v1.toInt() != 123 || v2.toString() != "test_str") {
                    errorCount.ref();
                }
            }
        });
        threads.append(t);
        t->start();
    }

    for (QThread* t : threads) {
        t->wait();
        delete t;
    }

    QCOMPARE(errorCount.load() , 0);
}

// ================================================================
// 10. 边界与异常场景
// ================================================================
void FlexDataTest::testBoundary_NegativeIndex()
{
    FlexData arr;
    arr.append("a");
    // 负索引自动修正为 0（Debug 下会触发断言，Release 下行为为 0）
#ifndef QT_NO_DEBUG
    QEXPECT_FAIL("", "Debug 模式下负索引触发断言", Continue);
#endif
    QCOMPARE(arr[-1].toString(), QString("a"));
}

void FlexDataTest::testBoundary_NullOperations()
{
    FlexData nullData;
    QCOMPARE(nullData.toInt(99), 99);
    QCOMPARE(nullData.toString("empty"), QString("empty"));
    QVERIFY(nullData.keys().isEmpty());
    QCOMPARE(nullData.arraySize(), 0);
}

void FlexDataTest::testDeserialization_InvalidInput()
{
    FlexData data;
    QVERIFY(!data.fromJson("{invalid json}"));
    QVERIFY(!data.fromXml("<not>valid</xml"));
    QVERIFY(!data.deserialize(QByteArray("garbage")));
}



#include "tst_flexdata.moc"
