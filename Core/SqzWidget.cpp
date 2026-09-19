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

}
