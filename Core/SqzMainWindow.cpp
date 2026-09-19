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

}

