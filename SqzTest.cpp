#include "SqzTest.h"
#include "ui_SqzTest.h"
//#include "SqzHub.h"
#include "SqzClassReg.h"
using namespace Sqz;

SqzTest::SqzTest(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SqzTest)
{
    ui->setupUi(this);
}

SqzTest::~SqzTest()
{
    delete ui;
}
SQZ_REG_NOARG(SqzTest);
