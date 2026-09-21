#ifndef SQZSERVICETEST_H
#define SQZSERVICETEST_H
#include <SqzService.h>

using namespace Sqz;
class SqzServiceTest : public SqzService
{
    Q_OBJECT
public:
    SqzServiceTest(QObject *parent = nullptr);
    ~SqzServiceTest();
protected:
     void onInit() override;
     void onClose() override;
};

#endif // SQZSERVICETEST_H
