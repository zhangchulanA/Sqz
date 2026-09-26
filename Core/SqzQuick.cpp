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
    QQmlContext* subCtx = new QQmlContext(engine->rootContext(),this);
    subCtx->setContextProperty("View", this);

    QObject* obj = component.create(subCtx);
    if (!obj) {
        logwarn << "Failed to create QML object!" << component.errors();
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

    m_initialized = true;

    onInit();

    return true;
}
}
