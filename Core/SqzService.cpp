// SqzService.cpp
#include "SqzService.h"
#include "SqzApplication.h"
namespace Sqz {
SqzService::SqzService(QObject* parent) : QObject(parent) {

}
SqzService::~SqzService() {}

// ---------- 通用单例操作 ----------
void SqzService::OpenService(const QString& className) {
    SqzApp->OpenService(className);
}

void SqzService::CloseService(const QString& className) {
    SqzApp->CloseService(className);
}

void SqzService::CloseServiceLater(const QString& className) {
    SqzApp->CloseServiceLater(className);
}

void SqzService::RestartService(const QString& className) {
    SqzApp->RestartService(className);
}

bool SqzService::HasService(const QString& className) const {
    return  SqzApp->HasService(className);
}

// ---------- 快捷操作 ----------
void SqzService::OpenThis() { OpenService(className());}

void SqzService::CloseThis() { CloseService(className());}
}
