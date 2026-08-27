// SqzService.cpp
#include "SqzService.h"
#include "SqzApplication.h"
#include <QApplication>

namespace Sqz {

SqzService::SqzService(QObject* parent) : QObject(parent)
    , m_workThread(nullptr)
    , m_running(false)
{

}

SqzService::~SqzService()
{
    //基类析构函数已经调用了 stopThread()，
    //子类析构函数中不应该再调用 stopWorkThread() 或 stopThread()。
    //如果需要主动停止，应该在 onClose() 中处理。
    //确保线程在对象销毁前停止
    stopThread();
}

// ---------- 通用单例操作 ----------
void SqzService::OpenService(const QString& className) {
    SqzApp->OpenService(className);
}

void SqzService::CloseService(const QString& className) {
    SqzApp->CloseService(className);
}

void SqzService::CloseServiceLater(const QString& className) {
    SqzApp->CloseServiceLater(className);
}

void SqzService::RestartService(const QString& className) {
    SqzApp->RestartService(className);
}

bool SqzService::HasService(const QString& className) const {
    return  SqzApp->HasService(className);
}

// ---------- 快捷操作 ----------
void SqzService::OpenThis() { OpenService(className());}

void SqzService::CloseThis() { CloseService(className());}

// ---------- 线程控制接口（供子类调用） ----------
void SqzService::startWorkThread()
{
    if (m_workThread) return;
    startThread();
}

void SqzService::stopWorkThread()
{
    if (!m_workThread) return;
    stopThread();
}

bool SqzService::isWorkThreadRunning() const
{
    return m_running;
}

// ---------- 内部槽函数 ----------
void SqzService::onStartWork()
{
    m_running = true;
    doWork();
    m_running = false;
    emit workFinished();
}

void SqzService::onStopWork()
{
    stopWork();
}

// ---------- 线程管理（内部实现） ----------
void SqzService::startThread()
{
    if (m_workThread) return;

    m_workThread = new QThread(this);
    this->moveToThread(m_workThread);

    //线程启动 -> 执行业务
    connect(m_workThread, &QThread::started, this, &SqzService::onStartWork);
    //业务完成 -> 退出线程
    connect(this, &SqzService::workFinished, m_workThread, &QThread::quit);
    //线程退出 -> 清理状态
    connect(m_workThread, &QThread::finished, this, [this]() {
        m_running = false;
    });

    m_running = true;
    m_workThread->start();
}

void SqzService::stopThread()
{
    if (!m_workThread) return;

    //请求停止业务（通过信号触发 onStopWork 在子线程执行）
    //注意：此处直接 emit workFinished() 会触发 onStopWork，但 workFinished 也连接了 quit()
    //为了避免冲突，我们直接请求停止
    emit workFinished();

    //等待线程退出
    m_workThread->quit();
    m_workThread->wait();

    //移回主线程，确保对象在正确的线程中销毁
    this->moveToThread(QApplication::instance()->thread());

    delete m_workThread;
    m_workThread = nullptr;

    m_running = false;
}

}
