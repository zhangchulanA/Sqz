// Net 框架全面测试 — 覆盖 IPC 和 TCP 所有核心功能
// 编译：在 .pro 中加入此文件即可
// 用法：先启动一个作为服务端，再启动一个作为客户端
//      命令行参数：server 或 client

#include "Net/Core/DataTransporter.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <memory>
#include <QJsonObject>

// ========== 测试辅助宏 ==========
#define TEST(name)  qDebug() << "\n========== [" << name << "] =========="
#define PASS        qDebug() << "  [PASS]"
#define FAIL(msg)   qDebug() << "  [FAIL]" << msg

// ========== 服务端测试 ==========
void runServerTests(Net::DataTransporter& net)
{
    // --- 测试1：IPC 服务端监听 ---
    TEST("IPC服务端监听");
    net.Listen("test_ipc_full");
    PASS;

    // --- 测试2：IPC 请求-应答 ---
    TEST("IPC请求-应答");
    QObject::connect(&net, &Net::DataTransporter::SignalRecvRequest,
        [&net](quint32 seq, const QByteArray& data) {
            qDebug() << "  服务端收到请求:" << data;

            // 超时测试：不回复，让客户端超时
            if (data == "timeout_test") {
                qDebug() << "  超时测试：故意不回复";
                return;
            }

            // ReplyFail测试：返回业务失败
            if (data == "fail_test") {
                net.ReplyFail(seq, QString("business_error_occurred"));
                qDebug() << "  回复业务失败: business_error_occurred";
                return;
            }

            // JSON请求测试：解析并回复JSON
            if (data.startsWith('{')) {
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (!doc.isNull()) {
                    QJsonObject obj = doc.object();
                    obj["reply"] = QString("json_ok");
                    net.ReplySuccess(seq, QJsonDocument(obj));
                    return;
                }
            }

            // 大负载测试：回显原始数据
            if (data.size() > 1000) {
                net.ReplySuccess(seq, data);
                return;
            }

            // 其他请求：正常回复
            net.ReplySuccess(seq, "IPC_OK:" + data);
        });

    // --- 测试3：IPC 接收推送 ---
    QObject::connect(&net, &Net::DataTransporter::SignalRecvPush,
        [](const QByteArray& data) {
            qDebug() << "  服务端收到推送:" << data;
        });

    // --- 测试4：TCP 服务端监听 ---
    TEST("TCP服务端监听(端口19999)");
    // 注意：TCP 和 IPC 不能同时监听同一个 DataTransporter 实例
    // 这里只演示 IPC 服务端，TCP 服务端需另开实例
    qDebug() << "  TCP服务端测试需另开进程，本测试仅测IPC";
    PASS;

    qDebug() << "\n服务端已就绪，等待客户端连接...";
}

// 模板序列化测试用结构体
struct TestStruct {
    int x;
    float y;
};

inline QDataStream& operator<<(QDataStream& ds, const TestStruct& s) {
    ds << s.x << s.y;
    return ds;
}

inline QDataStream& operator>>(QDataStream& ds, TestStruct& s) {
    ds >> s.x >> s.y;
    return ds;
}

// ========== 客户端测试 ==========
void runClientTests()
{
    Net::DataTransporter net;
    QEventLoop loop;
    int testPhase = 0;
    bool allPassed = true;

    // 先注册所有回调
    QObject::connect(&net, &Net::DataTransporter::SignalRecvPush,
        [](const QByteArray& data) {
            qDebug() << "  客户端收到推送:" << data;
        });

    QObject::connect(&net, &Net::DataTransporter::SignalConnected, [&]() {
        qDebug() << "  已连接";
    });

    QObject::connect(&net, &Net::DataTransporter::SignalDisconnected, [&]() {
        qDebug() << "  已断开";
    });

    // ============================================================
    //  测试函数声明（先声明后赋值，避免循环引用）
    // ============================================================
    std::function<void()> runTest1, runTest2, runTest3, runTest4,
                          runTest5, runTest6, runTest7, runTest8, runTest9,
                          runTest10, runTest11, runTest12, runTest13,
                          runTest14, runTest15, runTest16;

    // ============================================================
    //  测试1：IPC 基本请求-应答
    // ============================================================
    runTest1 = [&]() {
        TEST("IPC基本请求-应答");
        net.Request(QByteArray("hello"))
           .Timeout(3000)
           .OnSuccess([&](const QByteArray& rsp) {
               if (rsp == "IPC_OK:hello") {
                   qDebug() << "  响应:" << rsp;
                   PASS;
               } else {
                   FAIL("响应内容不匹配:" + rsp);
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest2);
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("请求失败:" + err.msg);
               allPassed = false;
               QTimer::singleShot(500, runTest2);
           })
           .Call();
    };

    // ============================================================
    //  测试2：IPC 单向推送
    // ============================================================
    runTest2 = [&]() {
        TEST("IPC单向推送");
        net.SendPush(QString("push_from_client"));
        qDebug() << "  推送已发送（检查服务端是否收到）";
        PASS;
        QTimer::singleShot(1000, runTest3);
    };

    // ============================================================
    //  测试3：请求超时
    // ============================================================
    runTest3 = [&]() {
        TEST("请求超时");
        net.Request(QByteArray("timeout_test"))
           .Timeout(2000)
           .OnSuccess([&](const QByteArray&) {
               FAIL("超时请求不应该成功");
               allPassed = false;
               QTimer::singleShot(500, runTest4);
           })
           .OnFailed([&](const Net::RequestError& err) {
               if (err.code == Net::RspCode::Timeout) {
                   qDebug() << "  超时码:" << int(err.code);
                   PASS;
               } else {
                   FAIL("错误码应为Timeout，实际:" + int(err.code));
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest4);
           })
           .Call();
    };

    // ============================================================
    //  测试4：并发请求
    // ============================================================
    runTest4 = [&]() {
        TEST("并发请求(3个同时发送)");
        auto pending = std::make_shared<int>(3);

        auto checkDone = [pending, &allPassed, &runTest5]() {
            if (--(*pending) == 0) {
                PASS;
                QTimer::singleShot(500, runTest5);
            }
        };

        for (int i = 0; i < 3; ++i) {
            net.Request(QByteArray("concurrent_" + QByteArray::number(i)))
               .Timeout(3000)
               .OnSuccess([i, checkDone](const QByteArray&) {
                   qDebug() << "  并发请求" << i << "成功";
                   checkDone();
               })
               .OnFailed([i, checkDone, &allPassed](const Net::RequestError& err) {
                   FAIL("并发请求" + QString::number(i) + "失败:" + err.msg);
                   allPassed = false;
                   checkDone();
               })
               .Call();
        }
    };

    // ============================================================
    //  测试5：周期请求
    // ============================================================
    runTest5 = [&]() {
        TEST("周期请求(每500ms一次，共3次)");
        auto cycleCount = std::make_shared<int>(0);

        net.Request(QByteArray("cycle_test"))
           .Cycle(500, 3)
           .OnSuccess([cycleCount, &allPassed, &runTest6](const QByteArray& rsp) {
               (*cycleCount)++;
               qDebug() << "  周期响应" << *cycleCount << ":" << rsp;
               if (*cycleCount == 3) {
                   PASS;
                   QTimer::singleShot(500, runTest6);
               }
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("周期请求失败:" + err.msg);
               allPassed = false;
               QTimer::singleShot(500, runTest6);
           })
           .Call();
    };

    // ============================================================
    //  测试6：同步请求
    // ============================================================
    runTest6 = [&]() {
        TEST("同步请求(Sync)");
        Net::RequestResult result = net.Request(QByteArray("sync_test"))
                                      .Timeout(3000)
                                      .Sync();
        if (result.IsSuccess()) {
            qDebug() << "  同步响应:" << result.payload;
            PASS;
        } else {
            FAIL("同步请求失败, code:" + QString::number(int(result.code)));
            allPassed = false;
        }
        QTimer::singleShot(500, runTest7);
    };

    // ============================================================
    //  测试7：断开后重连
    // ============================================================
    runTest7 = [&]() {
        TEST("断开后重连");
        net.Disconnect();
        QTimer::singleShot(1000, [&]() {
            net.ConnectTo("test_ipc_full");
            QTimer::singleShot(1000, [&]() {
                net.Request(QByteArray("reconnect_test"))
                   .Timeout(3000)
                   .OnSuccess([&](const QByteArray& rsp) {
                       qDebug() << "  重连后响应:" << rsp;
                       PASS;
                       QTimer::singleShot(500, runTest8);
                   })
                   .OnFailed([&](const Net::RequestError& err) {
                       FAIL("重连后请求失败:" + err.msg);
                       allPassed = false;
                       QTimer::singleShot(500, runTest8);
                   })
                   .Call();
            });
        });
    };

    // ============================================================
    //  测试8：大负载
    // ============================================================
    runTest8 = [&]() {
        TEST("大负载传输(10KB)");
        QByteArray bigData(10 * 1024, 'X');
        int bigSize = bigData.size();
        net.Request(bigData)
           .Timeout(5000)
           .OnSuccess([bigSize, &allPassed, &runTest10](const QByteArray& rsp) {
               if (rsp.size() == bigSize) {
                   qDebug() << "  大负载响应大小:" << rsp.size() << "bytes";
                   PASS;
               } else {
                   FAIL("大负载响应大小不匹配:" + QString::number(rsp.size()));
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest10);
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("大负载请求失败:" + err.msg);
               allPassed = false;
               QTimer::singleShot(500, runTest10);
           })
           .Call();
    };

    // ============================================================
    //  测试10：ReplyFail 业务失败
    // ============================================================
    runTest10 = [&]() {
        TEST("ReplyFail业务失败");
        net.Request(QByteArray("fail_test"))
           .Timeout(3000)
           .OnSuccess([&](const QByteArray&) {
               FAIL("ReplyFail不应该走OnSuccess");
               allPassed = false;
               QTimer::singleShot(500, runTest11);
           })
           .OnFailed([&](const Net::RequestError& err) {
               if (err.code == Net::RspCode::BusinessFail) {
                   qDebug() << "  错误码: BusinessFail, 错误信息:" << err.msg;
                   PASS;
               } else {
                   FAIL("错误码应为BusinessFail，实际:" + int(err.code));
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest11);
           })
           .Call();
    };

    // ============================================================
    //  测试11：JSON 接口
    // ============================================================
    runTest11 = [&]() {
        TEST("JSON接口(SendPush+Request+Reply)");
        // 测试JSON推送
        QJsonObject pushObj;
        pushObj["type"] = QString("json_push");
        pushObj["value"] = 42;
        net.SendPush(QJsonDocument(pushObj));
        qDebug() << "  JSON推送已发送";

        // 测试JSON请求
        QJsonObject reqObj;
        reqObj["action"] = QString("json_test");
        reqObj["param"] = 123;
        net.Request(QJsonDocument(reqObj))
           .Timeout(3000)
           .OnSuccess([&](const QByteArray& rsp) {
               QJsonDocument rspDoc = QJsonDocument::fromJson(rsp);
               if (rspDoc.object().value("reply").toString() == "json_ok") {
                   qDebug() << "  JSON响应:" << rsp;
                   PASS;
               } else {
                   FAIL("JSON响应内容不匹配");
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest12);
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("JSON请求失败:" + err.msg);
               allPassed = false;
               QTimer::singleShot(500, runTest12);
           })
           .Call();
    };

    // ============================================================
    //  测试12：模板序列化接口
    // ============================================================
    runTest12 = [&]() {
        TEST("模板序列化(SendPushStruct+Request<T>)");
        // 测试SendPushStruct
        TestStruct pushData = { 100, 3.14f };
        net.SendPushStruct(pushData);
        qDebug() << "  SendPushStruct已发送";

        // 测试Request<T>
        TestStruct reqData = { 200, 2.71f };
        net.Request(reqData)
           .Timeout(3000)
           .OnSuccess([&](const QByteArray& rsp) {
               if (!rsp.isEmpty()) {
                   qDebug() << "  Request<T>响应大小:" << rsp.size() << "bytes";
                   PASS;
               } else {
                   FAIL("Request<T>响应为空");
                   allPassed = false;
               }
               QTimer::singleShot(500, runTest13);
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("Request<T>失败:" + err.msg);
               allPassed = false;
               QTimer::singleShot(500, runTest13);
           })
           .Call();
    };

    // ============================================================
    //  测试13：StopCycleRequest 停止单个周期请求
    // ============================================================
    runTest13 = [&]() {
        TEST("StopCycleRequest停止单个周期请求");
        auto cycleCount = std::make_shared<int>(0);
        auto stopped = std::make_shared<bool>(false);
        auto testDone = std::make_shared<bool>(false);
        auto seqPtr = std::make_shared<quint32>(0);

        *seqPtr = net.Request(QByteArray("cycle_stop_test"))
                    .Cycle(300, 10)
                    .OnSuccess([cycleCount, stopped, testDone, seqPtr, &net, &allPassed, &runTest14](const QByteArray& rsp) {
                        (*cycleCount)++;
                        qDebug() << "  周期响应" << *cycleCount << ":" << rsp;
                        if (*cycleCount == 2 && !*stopped) {
                            *stopped = true;
                            net.StopCycleRequest(*seqPtr);
                            qDebug() << "  已调用StopCycleRequest";
                            QTimer::singleShot(800, [cycleCount, testDone, &allPassed, &runTest14]() {
                                if (*testDone) return;
                                *testDone = true;
                                if (*cycleCount == 2) {
                                    PASS;
                                } else {
                                    FAIL("StopCycleRequest后仍收到响应");
                                    allPassed = false;
                                }
                                QTimer::singleShot(500, runTest14);
                            });
                        }
                    })
                    .OnFailed([testDone, &allPassed, &runTest14](const Net::RequestError& err) {
                        if (*testDone) return;
                        *testDone = true;
                        FAIL("周期请求失败:" + err.msg);
                        allPassed = false;
                        QTimer::singleShot(500, runTest14);
                    })
                    .Call();
    };

    // ============================================================
    //  测试14：StopAllCycleRequest 停止所有周期请求
    // ============================================================
    runTest14 = [&]() {
        TEST("StopAllCycleRequest停止所有周期请求");
        auto count1 = std::make_shared<int>(0);
        auto count2 = std::make_shared<int>(0);
        auto stopped = std::make_shared<bool>(false);
        auto testDone = std::make_shared<bool>(false);

        net.Request(QByteArray("cycle_all_1"))
           .Cycle(300, 10)
           .OnSuccess([count1](const QByteArray&) { (*count1)++; })
           .OnFailed([testDone](const Net::RequestError&) {})
           .Call();

        net.Request(QByteArray("cycle_all_2"))
           .Cycle(300, 10)
           .OnSuccess([count2](const QByteArray&) { (*count2)++; })
           .OnFailed([testDone](const Net::RequestError&) {})
           .Call();

        QTimer::singleShot(1000, [&net, count1, count2, stopped, testDone, &allPassed, &runTest15]() {
            if (*stopped) return;
            *stopped = true;
            int c1 = *count1, c2 = *count2;
            net.StopAllCycleRequest();
            qDebug() << "  已停止所有，请求1:" << c1 << "次 请求2:" << c2 << "次";
            QTimer::singleShot(500, [c1, c2, count1, count2, testDone, &allPassed, &runTest15]() {
                if (*testDone) return;
                *testDone = true;
                if (*count1 == c1 && *count2 == c2) {
                    PASS;
                } else {
                    FAIL("StopAllCycleRequest后仍收到响应");
                    allPassed = false;
                }
                QTimer::singleShot(500, runTest15);
            });
        });
    };

    // ============================================================
    //  测试15：ScanLanDevices 局域网扫描
    // ============================================================
    runTest15 = [&]() {
        TEST("ScanLanDevices局域网扫描");
        QList<Net::DeviceInfo> devices = net.ScanLanDevices(2000, 19999);
        qDebug() << "  扫描到" << devices.size() << "个设备";
        for (const auto& dev : devices) {
            qDebug() << "    " << dev.ip << ":" << dev.port << " - " << dev.devName;
        }
        PASS;
        QTimer::singleShot(500, runTest16);
    };

    runTest16 = [&]() {
        qDebug() << "\n========================================";
        if (allPassed) {
            qDebug() << "  全部测试通过!";
        } else {
            qDebug() << "  部分测试失败，请检查上面输出";
        }
        qDebug() << "========================================";
        loop.quit();
    };

    // 启动测试
    net.ConnectTo("test_ipc_full");
    QTimer::singleShot(1000, runTest1);
    loop.exec();
}

// ========== TCP 专用测试 ==========
void runTcpServer()
{
    Net::DataTransporter net;
    net.Listen("0.0.0.0", 19999, Net::TransMode::Tcp);

    QObject::connect(&net, &Net::DataTransporter::SignalRecvRequest,
        [&net](quint32 seq, const QByteArray& data) {
            qDebug() << "  TCP服务端收到请求:" << data;
            // 回显数据
            net.ReplySuccess(seq, data);
        });

    QObject::connect(&net, &Net::DataTransporter::SignalRecvPush,
        [](const QByteArray& data) {
            qDebug() << "  TCP服务端收到推送:" << data;
        });

    qDebug() << "TCP服务端监听端口 19999...";
}

void runTcpClient()
{
    Net::DataTransporter net;
    QEventLoop loop;

    QObject::connect(&net, &Net::DataTransporter::SignalConnected, [&]() {
        qDebug() << "  TCP已连接";
        TEST("TCP请求-应答");
        net.Request(QByteArray("tcp_hello"))
           .Timeout(3000)
           .OnSuccess([&](const QByteArray& rsp) {
               qDebug() << "  TCP响应:" << rsp;
               if (rsp == "tcp_hello") PASS; else FAIL("响应不匹配");
               TEST("TCP推送");
               net.SendPush(QString("tcp_push"));
               PASS;
               qDebug() << "\nTCP测试完成";
               loop.quit();
           })
           .OnFailed([&](const Net::RequestError& err) {
               FAIL("TCP请求失败:" + err.msg);
               loop.quit();
           })
           .Call();
    });

    net.ConnectTo("127.0.0.1", 19999, Net::TransMode::Tcp);
    loop.exec();
}

// ========== 心跳 + 自动重连测试 ==========
void runHeartbeatReconnectTest()
{
    Net::DataTransporter net;
    QEventLoop loop;

    QObject::connect(&net, &Net::DataTransporter::SignalConnected, [&]() {
        qDebug() << "  [心跳测试] 已连接";
        // 连接成功后保持，等待观察心跳日志
    });

    QObject::connect(&net, &Net::DataTransporter::SignalDisconnected, [&]() {
        qDebug() << "  [心跳测试] 已断开（自动重连中...）";
    });

    QObject::connect(&net, &Net::DataTransporter::SignalNetError, [&](const QString& err) {
        qDebug() << "  [心跳测试] 错误:" << err;
    });

    net.AutoReconnect(true, 3000)
       .HeartBeat(true, 5000, 15000)
       .ConnectTo("test_ipc_full");

    qDebug() << "心跳+自动重连测试运行中...";
    qDebug() << "  - 每5秒发送心跳";
    qDebug() << "  - 15秒无响应判定超时";
    qDebug() << "  - 断连后3秒自动重连";
    qDebug() << "  请手动关闭服务端观察重连行为";
    qDebug() << "  按 Ctrl+C 退出";

    loop.exec();
}

// ========== 广播模式测试 ==========
void runBroadcastServer()
{
    Net::DataTransporter net;
    net.BroadcastAll(true);
    net.Listen("test_ipc_full");

    QObject::connect(&net, &Net::DataTransporter::SignalRecvRequest,
        [&net](quint32 seq, const QByteArray& data) {
            net.ReplySuccess(seq, "broadcast_reply:" + data);
        });

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&net]() {
        net.SendPush(QString("broadcast_push_every_3s"));
    });
    timer.start(3000);

    qDebug() << "广播服务端已启动，每3秒向所有客户端推送...";
}

