// SqzWidget.cpp
#include "SqzWidget.h"
#include "SqzWidgetOps.h"   // 视图操作统一实现（消除与 SqzMainWindow 的逐行重复）
namespace Sqz {
SqzWidget::SqzWidget(QWidget* parent) : QWidget(parent) {

}
SqzWidget::~SqzWidget() {}

// ---------- 通用单例操作（转发至 SqzWidgetOps 统一实现） ----------
void SqzWidget::OpenView(const QString& className)             { SqzWidgetOps::OpenView(className); }
void SqzWidget::CloseView(const QString& className)           { SqzWidgetOps::CloseView(className); }
void SqzWidget::CloseViewLater(const QString& className)      { SqzWidgetOps::CloseViewLater(className); }
void SqzWidget::RestartView(const QString& className)         { SqzWidgetOps::RestartView(className); }
bool SqzWidget::HasView(const QString& className) const        { return SqzWidgetOps::HasView(className); }

// ---------- 界面专属操作 ----------
void SqzWidget::HideView(const QString& className)             { SqzWidgetOps::HideView(className); }
void SqzWidget::ShowView(const QString& className)             { SqzWidgetOps::ShowView(className); }
void SqzWidget::ToggleView(const QString& className)            { SqzWidgetOps::ToggleView(className); }
bool SqzWidget::IsViewVisible(const QString& className) const  { return SqzWidgetOps::IsViewVisible(className); }
void SqzWidget::SetViewTopMost(const QString& className, bool topMost) { SqzWidgetOps::SetViewTopMost(className, topMost); }
void SqzWidget::ResizeView(const QString& className, int w, int h)     { SqzWidgetOps::ResizeView(className, w, h); }
void SqzWidget::MoveView(const QString& className, int x, int y)      { SqzWidgetOps::MoveView(className, x, y); }

// ---------- 快捷操作（操作自身） ----------
void SqzWidget::OpenThis()  { SqzWidgetOps::OpenThis(className()); }
void SqzWidget::CloseThis() { SqzWidgetOps::CloseThis(className()); }
void SqzWidget::HideThis()  { SqzWidgetOps::HideThis(className()); }
void SqzWidget::ShowThis()   { SqzWidgetOps::ShowThis(className()); }
}
