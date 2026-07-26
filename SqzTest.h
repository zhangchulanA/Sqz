#ifndef SQZTEST_H
#define SQZTEST_H

#include <QWidget>

namespace Ui {
class SqzTest;
}

class SqzTest : public QWidget
{
    Q_OBJECT

public:
    explicit SqzTest(QWidget *parent = nullptr);
    ~SqzTest();


private:
    Ui::SqzTest *ui;
    signals:
};

#endif // SQZTEST_H
