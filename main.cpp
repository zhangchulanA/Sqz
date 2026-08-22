#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include "ProtocolSchema.h"
#include "SqzApplication.h"
#include "MenuBar.h"
#include "MainWindow.h"
#include <Utils/QHotkey/QHotkey>
#include "KeyManager.h"
using namespace Sqz;



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Logger::instance().init(".","log");
    SqzApplication sq;
//        sq.Init();
    sq.LogRegClass();

//    MainWindow win;
//    win.show();

    auto* km = KeyManager::instance();

       // ---------- 1. 绑定按键 ----------
       // 首次绑定
       km->bind(KeyCode::F1, []() {
           qDebug() << "F1 被按下 - 打开帮助";
       });

       // 同一个接口，自动变成重绑定
       km->bind(KeyCode::F1, []() {
           qDebug() << "F1 现在执行新的功能";
       });

       // ---------- 4. 触摸屏触发 ----------
       // 触摸屏按钮点击时，模拟按键
       km->trigger(KeyCode::Enter);


       // ---------- 5. 批量绑定 ----------
       QMap<KeyCode, KeyAction::Callback> bindings;
       bindings[KeyCode::F3] = []() { qDebug() << "F3"; };
       bindings[KeyCode::F4] = []() { qDebug() << "F4"; };
       bindings[KeyCode::F5] = []() { qDebug() << "F5"; };
       km->bindBatch(bindings);

       // ---------- 6. 场景管理 ----------
       // 保存主界面场景
       km->saveContext("main");

       // 切换到编辑场景
       km->bind(KeyCode::F1, []() { qDebug() << "编辑模式：保存"; });
       km->bind(KeyCode::F2, []() { qDebug() << "编辑模式：撤销"; });
       km->saveContext("edit");

       // 切换回主界面
       km->loadContext("main");

       // ---------- 7. 临时禁用按键 ----------
       km->setEnabled(KeyCode::F1, false);  // 禁用 F1
       km->setEnabled(KeyCode::F1, true);   // 恢复 F1

       // ---------- 8. 查询状态 ----------
       qDebug() << "F1 是否已绑定：" << km->isBound(KeyCode::F1);
       qDebug() << "F1 是否启用：" << km->isEnabled(KeyCode::F1);
       qDebug() << "F1 显示名称：" << km->getKeyDisplayName(KeyCode::F1);
       qDebug() << "当前场景：" << km->currentContext();
       qDebug() << "已绑定按键数：" << km->boundCount();

    return app.exec();
}
