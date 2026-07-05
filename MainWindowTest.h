#ifndef MAINWINDOWTEST_H
#define MAINWINDOWTEST_H

#include <QMainWindow>
#include "SqzMainWindow.h"
namespace Ui {
class MainWindowTest;
}

class MainWindowTest : public SqzMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowTest(QWidget *parent = nullptr);
    ~MainWindowTest();
protected:

    // 必须实现：返回类名（必须与注册宏一致）
    QString className() const override { return "LoginWindow"; }


private slots:
    void on_pushButton_2_clicked();

private:
    Ui::MainWindowTest *ui;
};

#endif // MAINWINDOWTEST_H
