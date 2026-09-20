#include "form.h"
#include "ui_form.h"

SQZ_REG(Form)
Form::Form(QWidget *parent) :
    SqzWidget(parent),
    ui(new Ui::Form)
{
    ui->setupUi(this);
    loginfo << "触发构造函数";

}

Form::~Form()
{
    loginfo << "触发析构函数";
    delete ui;
}

void Form::onInit()
{
    loginfo << "onInit";
}

void Form::onClose()
{
    loginfo << "onClose";
}

