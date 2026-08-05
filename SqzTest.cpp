#include "SqzTest.h"
#include "ui_SqzTest.h"
//#include "SqzHub.h"
#include "SqzClassReg.h"


SqzTest::SqzTest(QWidget *parent) :
    SqzWidget(parent),
    ui(new Ui::SqzTest)
{
    ui->setupUi(this);

}

SqzTest::~SqzTest()
{
    logerror << property("sss2")<<property("sss3");
    delete ui;
}

void SqzTest::onInit()
{

    logerror << property("sss2")<<property("sss3")<<property("sss4");
}

SQZ_REG_NOARG(SqzTest);
