// SqzWidget.cpp
#include "SqzWidget.h"
#include "SqzViewOps.h"   // 视图操作统一实现（消除与 SqzMainWindow 的逐行重复）
namespace Sqz {
SqzWidget::SqzWidget(QWidget* parent) : QWidget(parent) {

}
SqzWidget::~SqzWidget() {}

// ---------- 通用单例操作（转发至 SqzViewOps 统一实现） ----------
void SqzWidget::OpenView(const QString& className)             { SqzViewOps::OpenView(className); }
void SqzWidget::CloseView(const QString& className)           { SqzViewOps::CloseView(className); }
void SqzWidget::CloseViewLater(const QString& className)      { SqzViewOps::CloseViewLater(className); }
void SqzWidget::RestartView(const QString& className)         { SqzViewOps::RestartView(className); }
bool SqzWidget::HasView(const QString& className) const        { return SqzViewOps::HasView(className); }

// ---------- 界面专属操作 ----------
void SqzWidget::HideView(const QString& className)             { SqzViewOps::HideView(className); }
void SqzWidget::ShowView(const QString& className)             { SqzViewOps::ShowView(className); }
void SqzWidget::ToggleView(const QString& className)            { SqzViewOps::ToggleView(className); }
bool SqzWidget::IsViewVisible(const QString& className) const  { return SqzViewOps::IsViewVisible(className); }
void SqzWidget::SetViewTopMost(const QString& className, bool topMost) { SqzViewOps::SetViewTopMost(className, topMost); }
void SqzWidget::ResizeView(const QString& className, int w, int h)     { SqzViewOps::ResizeView(className, w, h); }
void SqzWidget::MoveView(const QString& className, int x, int y)      { SqzViewOps::MoveView(className, x, y); }

// ---------- 快捷操作（操作自身） ----------
void SqzWidget::OpenThis()  { SqzViewOps::OpenThis(className()); }
void SqzWidget::CloseThis() { SqzViewOps::CloseThis(className()); }
void SqzWidget::HideThis()  { SqzViewOps::HideThis(className()); }
void SqzWidget::ShowThis()   { SqzViewOps::ShowThis(className()); }
}
