#ifndef SQZTEST_H
#define SQZTEST_H

#include <QWidget>
#include <SqzWidget.h>
namespace Ui {
class SqzTest;
}
using namespace Sqz;

class SqzTest : public SqzWidget
{
    Q_OBJECT

public:
    explicit SqzTest(QWidget *parent = nullptr);
    ~SqzTest();

protected:
    virtual QString className() const{
        return  "SqzTest";
    }
    void onInit();
private slots:
    void on_pushButton_clicked();

private:
    Ui::SqzTest *ui;
};

#endif // SQZTEST_H
