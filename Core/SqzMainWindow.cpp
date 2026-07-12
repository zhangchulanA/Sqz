// SqzMainWindow.cpp
#include "SqzMainWindow.h"
namespace Sqz {
SqzMainWindow::SqzMainWindow(QWidget *parent) : QMainWindow(parent) {

}
SqzMainWindow::~SqzMainWindow() {}

// ---------- 通用单例操作 ----------
void SqzMainWindow::OpenView(const QString& className) {
    SqzHub::Instance().CreateWidget(className);
}

void SqzMainWindow::CloseView(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}

void SqzMainWindow::CloseViewLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}

void SqzMainWindow::RestartView(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}

bool SqzMainWindow::HasView(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}

// ---------- 界面专属操作 ----------
void SqzMainWindow::HideView(const QString& className) {
    SqzHub::Instance().HideWidget(className);
}

void SqzMainWindow::ShowView(const QString &className)
{
    SqzHub::Instance().ShowWidget(className);
}

void SqzMainWindow::ToggleView(const QString& className) {
    SqzHub::Instance().ToggleWidget(className);
}

bool SqzMainWindow::IsViewVisible(const QString& className) const {
    return SqzHub::Instance().IsWidgetVisible(className);
}

void SqzMainWindow::SetViewTopMost(const QString& className, bool topMost) {
    SqzHub::Instance().SetWidgetTop(className, topMost);
}

void SqzMainWindow::ResizeView(const QString& className, int w, int h) {
    SqzHub::Instance().SetWidgetSize(className, w, h);
}

void SqzMainWindow::MoveView(const QString& className, int x, int y) {
    SqzHub::Instance().SetWidgetPos(className, x, y);
}

// ---------- 快捷操作 ----------
void SqzMainWindow::OpenThis() { OpenView(className()); }
void SqzMainWindow::CloseThis() { CloseView(className()); }
void SqzMainWindow::HideThis() { HideView(className()); }
void SqzMainWindow::ShowThis()  { ShowView(className()); }
}
