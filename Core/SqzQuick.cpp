// SqzQuick.cpp
#include "SqzQuick.h"

#include <QQmlComponent>
#include <QQmlContext>
namespace Sqz {
SqzQuick::SqzQuick(QObject* parent) : QObject(parent) {}
// 析构：释放 QQuickWindow（initializeView 中设为 CppOwnership，引擎不会自动回收）
SqzQuick::~SqzQuick() {
    if (m_window) {
        delete m_window;
        m_window = nullptr;
    }
}

// ---------- 通用单例操作 ----------
void SqzQuick::OpenView(const QString& className) {
    SqzHub::Instance().CreateQuick(className);
}
void SqzQuick::CloseView(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}
void SqzQuick::CloseViewLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}
void SqzQuick::RestartView(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}
bool SqzQuick::HasView(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}

// ---------- 窗口专属操作 ----------
void SqzQuick::HideView(const QString& className) {
    SqzHub::Instance().HideQuick(className);
}

void SqzQuick::ShowView(const QString& className) {
    SqzHub::Instance().ShowQuick(className);
}

void SqzQuick::ToggleView(const QString& className) {
    SqzHub::Instance().ToggleQuick(className);
}

bool SqzQuick::IsViewVisible(const QString& className) const {
    return SqzHub::Instance().IsQuickVisible(className);
}

void SqzQuick::SetViewTopMost(const QString& className, bool topMost) {
    SqzHub::Instance().SetQuickTop(className, topMost);
}

void SqzQuick::ResizeView(const QString& className, int w, int h) {
    SqzHub::Instance().SetQuickSize(className, w, h);
}

void SqzQuick::MoveView(const QString& className, int x, int y) {
    SqzHub::Instance().SetQuickPos(className, x, y);
}

// ---------- 快捷操作 ----------
void SqzQuick::OpenThis() { OpenView(className()); }
void SqzQuick::CloseThis() { CloseView(className()); }
void SqzQuick::HideThis() { HideView(className()); }
void SqzQuick::ShowThis()  { ShowView(className()); }

void SqzQuick::initializeView(const QString& qmisource)
{
    if (m_initialized) return;

    QQmlEngine* engine = SqzHub::Instance().qmlEngine();
    if (!engine) {
        logwarn << "QML engine not available!";
        return;
    }

    QQmlComponent component(engine, QUrl(qmisource));
    if (component.isError()) {
        logwarn << "Failed to load QML:" << component.errors();
        return;
    }

    // 创建独立子上下文隔离 "This"（修复 Bug #7：多视图共享 rootContext 导致 "This" 互相覆盖）
    QQmlContext* subCtx = new QQmlContext(engine->rootContext());
    subCtx->setContextProperty("This", this);

    QObject* obj = component.create(subCtx);
    if (!obj) {
        logwarn << "Failed to create QML object!";
        delete subCtx;
        return;
    }

    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        logwarn << "QML root is not a QQuickWindow!";
        delete obj;
        delete subCtx;
        return;
    }

    // 设置 C++ 所有权，防止 QML 引擎自动销毁
    QQmlEngine::setObjectOwnership(m_window, QQmlEngine::CppOwnership);

    // 子上下文生命周期绑定到 m_window（随窗口销毁，避免泄漏）
    subCtx->setParent(m_window);

    m_initialized = true;

    onInit();
}
}
