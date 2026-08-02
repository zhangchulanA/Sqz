#ifndef BUS_SENDERS_H
#define BUS_SENDERS_H

// ============================================================
//  bus_senders.h — SqzBus 测试用辅助类
//  独立头文件：qmake 自动 moc，bus_main.cpp 无需 include .moc 文件
// ============================================================

#include <QObject>
#include <QString>
#include <QVariant>

/// 用于模板 Receive 测试的对象（含多种成员函数签名）
class BusTarget : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // 测试结果记录
    QString m_lastStr;
    int     m_lastInt = -1;
    double  m_lastDouble = -1.0;
    bool    m_lastBool = false;
    int     m_noArgCallCount = 0;

public slots:
    // 无参成员函数
    void onNoArg() { ++m_noArgCallCount; }

    // QString 参数成员函数
    void onString(const QString& s) { m_lastStr = s; }

    // int 参数成员函数
    void onInt(int v) { m_lastInt = v; }

    // double 参数成员函数
    void onDouble(double v) { m_lastDouble = v; }

    // bool 参数成员函数
    void onBool(bool v) { m_lastBool = v; }
};

/// 跨线程测试用 Worker
class BusWorker : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    int m_receivedCount = 0;
    QVariant m_lastData;

public slots:
    /// 工作线程入口：向总线发送消息
    void doWork()
    {
        // 在工作线程中通过总线发送消息
        // 测试跨线程 Send 的线程安全性
    }

    /// 接收消息的槽
    void onReceived(const QVariant& data)
    {
        ++m_receivedCount;
        m_lastData = data;
    }
};

#endif // BUS_SENDERS_H
