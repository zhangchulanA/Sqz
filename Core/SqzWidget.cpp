// SqzWidget.cpp
#include "SqzWidget.h"
namespace Sqz {
SqzWidget::SqzWidget(QWidget* parent) : QWidget(parent) {

}
SqzWidget::~SqzWidget() {}

// ---------- 通用单例操作 ----------
void SqzWidget::OpenView(const QString& className) {
    SqzHub::Instance().CreateWidget(className);
}

void SqzWidget::CloseView(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}

void SqzWidget::CloseViewLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}

void SqzWidget::RestartView(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}

bool SqzWidget::HasView(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}

// ---------- 界面专属操作 ----------
void SqzWidget::HideView(const QString& className) {
    SqzHub::Instance().HideWidget(className);
}

void SqzWidget::ShowView(const QString &className)
{
    SqzHub::Instance().ShowWidget(className);
}

void SqzWidget::ToggleView(const QString& className) {
    SqzHub::Instance().ToggleWidget(className);
}

bool SqzWidget::IsViewVisible(const QString& className) const {
    return SqzHub::Instance().IsWidgetVisible(className);
}

void SqzWidget::SetViewTopMost(const QString& className, bool topMost) {
    SqzHub::Instance().SetWidgetTop(className, topMost);
}

void SqzWidget::ResizeView(const QString& className, int w, int h) {
    SqzHub::Instance().SetWidgetSize(className, w, h);
}

void SqzWidget::MoveView(const QString& className, int x, int y) {
    SqzHub::Instance().SetWidgetPos(className, x, y);
}

// ---------- 快捷操作 ----------
void SqzWidget::OpenThis() { OpenView(className()); }
void SqzWidget::CloseThis() { CloseView(className()); }
void SqzWidget::HideThis() { HideView(className()); }
void SqzWidget::ShowThis()  { ShowView(className()); }
}
