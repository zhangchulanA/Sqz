#ifndef EA_SENDERS_H
#define EA_SENDERS_H

// ============================================================
//  ea_senders.h — EventAggregator 测试用信号源类
//
//  独立头文件设计：qmake 会自动对本头文件运行 moc，
//  生成 moc_ea_senders.cpp 并编译链接。
//  ea_main.cpp 只需 #include 本文件，无需 #include 任何 .moc 文件。
// ============================================================

#include <QObject>
#include <QString>

// ============================================================
//  SenderA：覆盖三种信号签名（引用 / 值 / 多参数）
//  用于测试 EventAggregator 对不同参数类型的兼容性
// ============================================================
class SenderA : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // 便捷触发方法（外部调用）
    void emitText(const QString& s)  { emit textChanged(s); }
    void emitValue(int v)            { emit valueChanged(v); }
    void emitMulti(int v, const QString& s) { emit multiArg(v, s); }

signals:
    void textChanged(const QString&);   // 引用类型信号
    void valueChanged(int);             // 值类型信号
    void multiArg(int, const QString&); // 多参数信号
};

// ============================================================
//  Worker：跨线程测试用，在工作线程 emit progress 信号
// ============================================================
class Worker : public QObject
{
    Q_OBJECT
signals:
    void progress(int p);

public slots:
    // 工作线程内连发 3 次 progress 信号
    void runWork()
    {
        for (int i = 1; i <= 3; ++i)
            emit progress(i * 10);
    }
};

#endif // EA_SENDERS_H
