// SqzQuick.cpp
#include "SqzQuick.h"

#include <QQmlComponent>
#include <QQmlContext>
#include "SqzViewOps.h"
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
    SqzViewOps::OpenView(className);
}
void SqzQuick::CloseView(const QString& className) {
    SqzViewOps::CloseView(className);
}
void SqzQuick::CloseViewLater(const QString& className) {
    SqzViewOps::CloseViewLater(className);
}
void SqzQuick::RestartView(const QString& className) {
    SqzViewOps::RestartView(className);
}
bool SqzQuick::HasView(const QString& className) const {
    return SqzViewOps::HasView(className);
}

// ---------- 窗口专属操作 ----------
void SqzQuick::HideView(const QString& className) {
    SqzViewOps::HideView(className);
}

void SqzQuick::ShowView(const QString& className) {
    SqzViewOps::ShowView(className);
}

void SqzQuick::ToggleView(const QString& className) {
    SqzViewOps::ToggleView(className);
}

bool SqzQuick::IsViewVisible(const QString& className) const {
    return SqzViewOps::IsViewVisible(className);
}

void SqzQuick::SetViewTopMost(const QString& className, bool topMost) {
    SqzViewOps::SetViewTopMost(className, topMost);
}

void SqzQuick::ResizeView(const QString& className, int w, int h) {
    SqzViewOps::ResizeView(className, w, h);
}

void SqzQuick::MoveView(const QString& className, int x, int y) {
    SqzViewOps::MoveView(className, x, y);
}

// ---------- 快捷操作 ----------
void SqzQuick::OpenThis() {  SqzViewOps::OpenThis(className());}
void SqzQuick::CloseThis() {  SqzViewOps::CloseThis(className()); }
void SqzQuick::HideThis() { SqzViewOps::HideThis(className()); }
void SqzQuick::ShowThis()  {   SqzViewOps::ShowThis(className()); }


bool SqzQuick::init()
{
    if (m_initialized) return true;

    QQmlEngine* engine = SqzApp->hub().qmlEngine();
    if (!engine) {
        logwarn << "QML engine not available!";
        return false;
    }

    QQmlComponent component(engine, QUrl(m_qmlSourcePath));
    if (component.isError()) {
        logwarn << "Failed to load QML:" << component.errors();
        return false;
    }

    // 创建独立子上下文隔离 "This"（修复 Bug #7：多视图共享 rootContext 导致 "This" 互相覆盖）
    QQmlContext* subCtx = new QQmlContext(engine->rootContext());
    subCtx->setContextProperty("This", this);

    QObject* obj = component.create(subCtx);
    if (!obj) {
        logwarn << "Failed to create QML object!";
        delete subCtx;
        return false;
    }

    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        logwarn << "QML root is not a QQuickWindow!";
        delete obj;
        delete subCtx;
        return false;
    }

    // 设置 C++ 所有权，防止 QML 引擎自动销毁
    QQmlEngine::setObjectOwnership(m_window, QQmlEngine::CppOwnership);

    // 子上下文生命周期绑定到 m_window（随窗口销毁，避免泄漏）
    subCtx->setParent(m_window);

    m_initialized = true;

    onInit();

    return true;
}
}
