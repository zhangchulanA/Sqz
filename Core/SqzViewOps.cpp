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


} // namespace Sqz
