#ifndef FORM_H
#define FORM_H

#include <SqzWidget.h>

using namespace Sqz;
namespace Ui {
class Form;
}

class Form : public SqzWidget
{
    Q_OBJECT

public:
    explicit Form(QWidget *parent = nullptr);
    ~Form();

protected:
    //构造完成后执行
    void onInit() override;
    //析构触发前执行
    void onClose() override;
private:
    Ui::Form *ui;
};

#endif // FORM_H
