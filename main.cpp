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
#include "DBManager.h"
#include "TestWindow.h"
#include "SuperListAll.h"
#include "NavGlobalTest.h"
#include "NavGlobalView.h"
#include "RouteWidget.h"

using namespace Sqz;

void KeyTest()
{
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
}

int BDTest()
{
    // ==================== 1. 配置数据库 ====================
       Configuration config;
       config.setType("QSQLITE")
              .setDatabaseName("test.db")
              .setConnectionName("main_db");

       // 获取默认实例并配置
       DBManager& db = DBManager::instance();
       db.configure(config);

       if (!db.lastError().isEmpty()) {
           qCritical() << "配置失败:" << db.lastError();
           return -1;
       }
       qDebug() << "数据库配置成功";

       // ==================== 2. 创建表 ====================
       QMap<QString, QString> columns;
       columns["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
       columns["name"] = "TEXT NOT NULL";
       columns["age"] = "INTEGER";
       columns["email"] = "TEXT UNIQUE";
       columns["created_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";

       if (db.createTable("users", columns)) {
           qDebug() << "创建 users 表成功";
       } else {
           qCritical() << "创建表失败:" << db.lastError();
           return -1;
       }

       // ==================== 3. 插入数据 ====================
       QVariantMap user;
       user["name"] = "张三";
       user["age"] = 25;
       user["email"] = "zhangsan@example.com";

       if (db.insertRecord("users", user)) {
           qDebug() << "插入用户成功";
       } else {
           qCritical() << "插入失败:" << db.lastError();
       }

       // 批量插入
       QVector<QVariantMap> users;
       for (int i = 0; i < 5; ++i) {
           QVariantMap u;
           u["name"] = QString("用户_%1").arg(i + 1);
           u["age"] = 20 + i;
           u["email"] = QString("user%1@example.com").arg(i + 1);
           users.append(u);
       }
       int inserted = db.insertBatch("users", users);
       qDebug() << "批量插入" << inserted << "条记录";

       // ==================== 4. 查询数据 ====================
       // 4.1 查询所有
       auto allUsers = db.selectRecords("users");
       qDebug() << "所有用户数:" << allUsers.size();
       for (const auto& u : allUsers) {
           qDebug() << "ID:" << u["id"].toInt()
                    << "姓名:" << u["name"].toString()
                    << "年龄:" << u["age"].toInt();
       }

       // 4.2 条件查询
       auto results = db.selectRecords("users", {"name", "age"}, {{"age", 25}});
       if (!results.isEmpty()) {
           qDebug() << "年龄为25的用户:"
                    << results.first()["name"].toString();
       }

       // 4.3 灵活条件查询 (>=, LIKE)
       QVector<Condition> conds;
       conds << Condition("age", 22, Condition::GreaterOrEqual)
             << Condition("name", "%用户%", Condition::Like);
       auto filterResults = db.selectRecords("users", {"*"}, conds, "age DESC");
       qDebug() << "年龄>=22且姓名包含'用户'的用户数:" << filterResults.size();

       // ==================== 5. 更新数据 ====================
       QVariantMap updates;
       updates["age"] = 26;
       if (db.updateRecords("users", updates, {{"name", "张三"}})) {
           qDebug() << "更新用户年龄成功";
       }

       // ==================== 6. 删除数据 ====================
       // 6.1 删除指定用户
       if (db.deleteRecords("users", {{"name", "用户_5"}})) {
           qDebug() << "删除用户成功";
       }

       // 6.2 使用灵活条件删除（年龄小于20的）
       QVector<Condition> delConds;
       delConds << Condition("age", 20, Condition::Less);
       if (db.deleteRecords("users", delConds)) {
           qDebug() << "删除年龄<20的用户成功";
       }

       // ==================== 7. 计数与存在检查 ====================
       int count = db.countRecords("users");
       qDebug() << "当前用户总数:" << count;

       bool exists = db.recordExists("users", {{"name", "张三"}});
       qDebug() << "用户'张三'是否存在:" << exists;

       // ==================== 8. 事务操作 ====================
       bool txSuccess = db.executeTransaction([&](QSqlDatabase& conn) {
           // 在事务中执行多个操作
           QSqlQuery q(conn);
           if (!q.exec("INSERT INTO users (name, age) VALUES ('事务用户', 30)")) {
               qWarning() << "事务中插入失败:" << q.lastError().text();
               return false;
           }
           if (!q.exec("UPDATE users SET age = age + 1 WHERE name = '张三'")) {
               qWarning() << "事务中更新失败:" << q.lastError().text();
               return false;
           }
           return true;  // 全部成功，提交事务
       });

       qDebug() << "事务执行结果:" << (txSuccess ? "成功提交" : "回滚");

       // ==================== 9. JSON 互转 ====================
       // 9.1 查询结果转 JSON
       auto jsonData = db.selectRecords("users", {"id", "name", "age"});
       QJsonArray jsonArray = db.toJson(jsonData);
       QByteArray jsonStr = QJsonDocument(jsonArray).toJson(QJsonDocument::Indented);
//        QJsonObject obj = QJsonDocument(jsonArray).object();
       qDebug() << "JSON数据:\n" << QString::fromUtf8(jsonStr);

       // 9.2 从 JSON 插入
       QJsonObject newUser;
       newUser["name"] = "JSON用户";
       newUser["age"] = 28;
       newUser["email"] = "json@example.com";
       if (db.insertRecordFromJson("users", newUser)) {
           qDebug() << "从JSON插入用户成功";
       }

       // ==================== 10. 分页查询 ====================
       auto pageData = db.selectPage("users", 1, 3, {"id", "name", "age"},
                                     {}, "id DESC");
       int total = pageData["total"].toInt();
       auto pageRows = pageData["rows"].value<QVector<QVariantMap>>();
       qDebug() << "分页: 总数=" << total << ", 当前页记录数=" << pageRows.size();

       // ==================== 11. 健康检查 ====================
       if (db.ping()) {
           qDebug() << "所有数据库连接正常";
       } else {
           qWarning() << "数据库连接异常:" << db.lastError();
       }

       // 开启自动重连
       db.setAutoReconnect(true);
       qDebug() << "自动重连已开启:" << db.autoReconnect();

       // ==================== 12. 错误处理示例 ====================
       // 尝试更新不存在的用户
       if (!db.updateRecords("users", {{"age", 99}}, {{"id", 99999}})) {
           qWarning() << "更新失败（预期）:" << db.lastError();
       }

       // ==================== 13. 多实例示例 ====================
       // 获取另一个实例
       Configuration logConfig;
       logConfig.setType("QSQLITE")
                .setDatabaseName("logs.db");

       DBManager& logDb = DBManager::instance("logs");
       logDb.configure(logConfig);

       // 创建日志表
       QMap<QString, QString> logColumns;
       logColumns["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
       logColumns["message"] = "TEXT";
       logColumns["created_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";
       logDb.createTable("logs", logColumns);

       // 插入日志
       logDb.insertRecord("logs", {{"message", "应用程序启动"}});
       qDebug() << "日志已写入独立数据库实例";

       // ==================== 14. 清理资源 ====================
       // 程序退出前销毁所有实例（可选，进程退出时会自动清理）
       DBManager::destroyAll();

       qDebug() << "示例程序执行完毕";

}

void LogTest(){
    // 普通日志 - 不会显示在控制台（因为 enableConsole=false）
    logdebug << "This won't be seen";      // 等级过滤，也不写入文件
    loginfo  << "Normal info - file only"; // 写入文件，不显示控制台
    logwarn  << "Normal warn - file only"; // 写入文件，不显示控制台
    logerror << "Normal error - file only";// 写入文件，不显示控制台

    // 强制日志 - 忽略所有开关，同时输出到文件和控制台
    fdebug << "Force debug - shows everywhere!";  // 蓝色
    finfo  << "System initialized successfully";  // 绿色
    fwarn  << "Memory usage: " << 85 << "%";      // 黄色
    ferror << "Database connection failed!";      // 红色

    // 强制日志立即刷新到文件，即使 FLUSH_INTERVAL=64 也不影响
    finfo << "Critical transaction completed";
}


void SuperListAllTest()
{
    SuperListWidget* list = new SuperListWidget();

    TableColumnConfig col;
    col.name = "status";
    col.title = "状态";
    col.type = TableCellType::StateTag;
    list->setHeaders({col});   // 单列配置

    TableRowData r1, r2;
    r1.set("status", "正常");
    r2.set("status", "失败");
    list->addRows({r1, r2});

    list->filterColumn("status", "正常");  // 筛选

    QObject::connect(list, &SuperListWidget::rowClickedIndex,
            [](const TableRowData& data, int idx){
        qDebug() << "点击:" << data.get("status").toString();
    });

list->show();

}
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Logger::instance().init("./log","log_");
    SqzApplication sq;
//        sq.Init();
    sq.LogRegClass();

//    MainWindow win;
//    win.show();
//LogTest();
//   BDTest();
//    TestWindow window;
//    window.setWindowTitle("SuperTableWidget 测试示例");
//    window.resize(800, 600);
//    window.show();
//    NavGlobalTest test;
//    test.show();
//    NavGlobalView v;
//    v.setMinimumSize(200,200);
//    v.show();

    RouteWidget r;
    r.show();

    return app.exec();
}
