// SqzMainWindow.cpp
#include "SqzMainWindow.h"
#include "SqzViewOps.h"   // 视图操作统一实现（与 SqzWidget 共享同一份转发逻辑）
namespace Sqz {
SqzMainWindow::SqzMainWindow(QWidget *parent) : QMainWindow(parent) {

}
SqzMainWindow::~SqzMainWindow() {}

// ---------- 通用单例操作（转发至 SqzViewOps 统一实现） ----------
void SqzMainWindow::OpenView(const QString& className)             { SqzViewOps::OpenView(className); }
void SqzMainWindow::CloseView(const QString& className)           { SqzViewOps::CloseView(className); }
void SqzMainWindow::CloseViewLater(const QString& className)      { SqzViewOps::CloseViewLater(className); }
void SqzMainWindow::RestartView(const QString& className)         { SqzViewOps::RestartView(className); }
bool SqzMainWindow::HasView(const QString& className) const       { return SqzViewOps::HasView(className); }

// ---------- 界面专属操作 ----------
void SqzMainWindow::HideView(const QString& className)             { SqzViewOps::HideView(className); }
void SqzMainWindow::ShowView(const QString &className)             { SqzViewOps::ShowView(className); }
void SqzMainWindow::ToggleView(const QString& className)            { SqzViewOps::ToggleView(className); }
bool SqzMainWindow::IsViewVisible(const QString& className) const  { return SqzViewOps::IsViewVisible(className); }
void SqzMainWindow::SetViewTopMost(const QString& className, bool topMost) { SqzViewOps::SetViewTopMost(className, topMost); }
void SqzMainWindow::ResizeView(const QString& className, int w, int h)     { SqzViewOps::ResizeView(className, w, h); }
void SqzMainWindow::MoveView(const QString& className, int x, int y)       { SqzViewOps::MoveView(className, x, y); }

// ---------- 快捷操作（操作自身） ----------
void SqzMainWindow::OpenThis()  { SqzViewOps::OpenThis(className()); }
void SqzMainWindow::CloseThis() { SqzViewOps::CloseThis(className()); }
void SqzMainWindow::HideThis()  { SqzViewOps::HideThis(className()); }
void SqzMainWindow::ShowThis()   { SqzViewOps::ShowThis(className()); }
}
