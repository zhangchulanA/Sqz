#include "SqzViewOps.h"
#include "SqzApplication.h"

namespace Sqz {

// ========== 通用单例操作 ==========
void SqzViewOps::OpenView(const QString& className) {
    SqzApp->OpenView(className);
}

void SqzViewOps::CloseView(const QString& className) {
    SqzApp->CloseView(className);
}

void SqzViewOps::CloseViewLater(const QString& className) {
    SqzApp->CloseViewLater(className);
}

void SqzViewOps::RestartView(const QString& className) {
    SqzApp->RestartView(className);
}

bool SqzViewOps::HasView(const QString& className) {
    return SqzApp->HasView(className);
}

// ========== 视图显隐/位置操作 ==========
void SqzViewOps::HideView(const QString& className) {
    SqzApp->HideView(className);  // SqzApplication 内部根据类型分发
}

void SqzViewOps::ShowView(const QString& className) {
    SqzApp->ShowView(className);
}

void SqzViewOps::ToggleView(const QString& className) {
    SqzApp->ToggleView(className);
}

bool SqzViewOps::IsViewVisible(const QString& className) {
    return SqzApp->IsViewVisible(className);
}

void SqzViewOps::SetViewTopMost(const QString& className, bool topMost) {
    SqzApp->SetViewTopMost(className, topMost);
}

void SqzViewOps::ResizeView(const QString& className, int w, int h) {
    SqzApp->ResizeView(className, w, h);
}

void SqzViewOps::MoveView(const QString& className, int x, int y) {
    SqzApp->MoveView(className, x, y);
}

// ========== 快捷操作 ==========
void SqzViewOps::OpenThis(const QString& className) {
    OpenView(className);
}

void SqzViewOps::CloseThis(const QString& className) {
    CloseView(className);
}

void SqzViewOps::HideThis(const QString& className) {
    HideView(className);
}

void SqzViewOps::ShowThis(const QString& className) {
    ShowView(className);
}

} // namespace Sqz
