// SqzMainWindow.cpp
#include "SqzMainWindow.h"
#include "SqzWidgetOps.h"   // 视图操作统一实现（与 SqzWidget 共享同一份转发逻辑）
namespace Sqz {
SqzMainWindow::SqzMainWindow(QWidget *parent) : QMainWindow(parent) {

}
SqzMainWindow::~SqzMainWindow() {}

// ---------- 通用单例操作（转发至 SqzWidgetOps 统一实现） ----------
void SqzMainWindow::OpenView(const QString& className)             { SqzWidgetOps::OpenView(className); }
void SqzMainWindow::CloseView(const QString& className)           { SqzWidgetOps::CloseView(className); }
void SqzMainWindow::CloseViewLater(const QString& className)      { SqzWidgetOps::CloseViewLater(className); }
void SqzMainWindow::RestartView(const QString& className)         { SqzWidgetOps::RestartView(className); }
bool SqzMainWindow::HasView(const QString& className) const       { return SqzWidgetOps::HasView(className); }

// ---------- 界面专属操作 ----------
void SqzMainWindow::HideView(const QString& className)             { SqzWidgetOps::HideView(className); }
void SqzMainWindow::ShowView(const QString &className)             { SqzWidgetOps::ShowView(className); }
void SqzMainWindow::ToggleView(const QString& className)            { SqzWidgetOps::ToggleView(className); }
bool SqzMainWindow::IsViewVisible(const QString& className) const  { return SqzWidgetOps::IsViewVisible(className); }
void SqzMainWindow::SetViewTopMost(const QString& className, bool topMost) { SqzWidgetOps::SetViewTopMost(className, topMost); }
void SqzMainWindow::ResizeView(const QString& className, int w, int h)     { SqzWidgetOps::ResizeView(className, w, h); }
void SqzMainWindow::MoveView(const QString& className, int x, int y)       { SqzWidgetOps::MoveView(className, x, y); }

// ---------- 快捷操作（操作自身） ----------
void SqzMainWindow::OpenThis()  { SqzWidgetOps::OpenThis(className()); }
void SqzMainWindow::CloseThis() { SqzWidgetOps::CloseThis(className()); }
void SqzMainWindow::HideThis()  { SqzWidgetOps::HideThis(className()); }
void SqzMainWindow::ShowThis()   { SqzWidgetOps::ShowThis(className()); }
}
