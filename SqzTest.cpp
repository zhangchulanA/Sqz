#include "SqzTest.h"
#include "ui_SqzTest.h"
#include "SqzHub.h"
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
SQZOBJECT_NOARG(SqzTest);
