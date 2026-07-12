// SqzQuick.cpp
#include "SqzQuick.h"

#include <QQmlComponent>
namespace Sqz {
SqzQuick::SqzQuick(QObject* parent) : QObject(parent) {}
SqzQuick::~SqzQuick() {

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

void SqzQuick::initializeView()
{
    if (m_initialized) return;

    QQmlEngine* engine = SqzHub::Instance().qmlEngine();
    if (!engine) {
        logwarn << "QML engine not available!";
        return;
    }

    QQmlComponent component(engine, QUrl(qmlSource()));
    if (component.isError()) {
        logwarn << "Failed to load QML:" << component.errors();
        return;
    }

    QObject* obj = component.create();
    if (!obj) {
        logwarn << "Failed to create QML object!";
        return;
    }

    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        logwarn << "QML root is not a QQuickWindow!";
        delete obj;
        return;
    }

    // 设置 C++ 所有权，防止 QML 引擎自动销毁
    QQmlEngine::setObjectOwnership(m_window, QQmlEngine::CppOwnership);

    m_initialized = true;

    onInit();
}
}
