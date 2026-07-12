// SqzService.cpp
#include "SqzService.h"
namespace Sqz {
SqzService::SqzService(QObject* parent) : QObject(parent) {

}
SqzService::~SqzService() {}

// ---------- 通用单例操作 ----------
void SqzService::OpenService(const QString& className) {
    SqzHub::Instance().CreateObject(className);
}

void SqzService::CloseService(const QString& className) {
    SqzHub::Instance().CloseObj(className);
}

void SqzService::CloseServiceLater(const QString& className) {
    SqzHub::Instance().CloseObjLater(className);
}

void SqzService::RestartService(const QString& className) {
    SqzHub::Instance().ResetObj(className);
}

bool SqzService::HasService(const QString& className) const {
    return SqzHub::Instance().IsExist(className);
}

// ---------- 快捷操作 ----------
void SqzService::OpenThis() { OpenService(className());}

void SqzService::CloseThis() { CloseService(className());}
}
