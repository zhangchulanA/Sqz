#include "SqzServiceTest.h"
#include "Logger.h"

SQZ_REG(SqzServiceTest)
SqzServiceTest::SqzServiceTest(QObject *parent) :
    SqzService(parent)
{
    loginfo << "触发构造函数";
}

SqzServiceTest::~SqzServiceTest()
{
    loginfo << "触发析构函数";
}

void SqzServiceTest::onInit()
{
    loginfo << "onInit";
}

void SqzServiceTest::onClose()
{
    loginfo << "onClose";
}

