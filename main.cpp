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
#include "SERIALIZE.h"
#include "TestModel.h"
#include "SqzModel.h"
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



// 1. 标准枚举（从 0 开始）
enum Weekday {
    Monday = 0,
    Tuesday = 1,
    Wednesday = 2,
    Thursday = 3,
    Friday = 4,
    Saturday = 5,
    Sunday = 6
};
SERIALIZE_ENUM(Weekday)  // ✅ 一行搞定

// 2. 自定义值的枚举
enum Priority {
    Low = 1,
            Medium = 5,
            High = 10,
            Critical = 20
};
SERIALIZE_ENUM(Priority)  // ✅ 一行搞定

// 3. 不连续的枚举
enum StatusCode {
    Success = 200,
            NotFound = 404,
            ServerError = 500,
            Timeout = 504
};
SERIALIZE_ENUM(StatusCode)  // ✅ 一行搞定

// 4. enum class（强枚举）
enum class UserRole {
    Guest = 0,
            User = 1,
            Moderator = 2,
            Admin = 3,
            SuperAdmin = 4
};
SERIALIZE_ENUM(UserRole)  // ✅ 一行搞定

// 5. 负值枚举
enum Temperature {
    Cold = -10,
            Cool = 0,
            Warm = 25,
            Hot = 40
};
SERIALIZE_ENUM(Temperature)  // ✅ 一行搞定

// ==================== 使用 SERIALIZE 宏的结构体 ====================

// 示例 1: 包含多个枚举的结构体
struct Task {
    int id;
    QString title;
    Priority priority;
    StatusCode status;
    Weekday dueDay;

    SERIALIZE(id, title, priority, status, dueDay)  // ✅ 所有枚举都工作
};

// 示例 2: 包含 enum class 的结构体
struct User {
    int userId;
    QString username;
    UserRole role;
    bool isActive;

    SERIALIZE(userId, username, role, isActive)  // ✅ enum class 也工作
};

// 示例 3: 包含枚举容器的结构体
struct Schedule {
    QString name;
    QList<Weekday> workingDays;
    QVector<Priority> taskPriorities;
    QMap<Weekday, QString> dailyTasks;

    SERIALIZE(name, workingDays, taskPriorities, dailyTasks)  // ✅ 容器中的枚举也工作
};

// 示例 4: 嵌套结构体
struct Team {
    QString teamName;
    QList<User> members;
    UserRole defaultRole;

    SERIALIZE(teamName, members, defaultRole)  // ✅ 嵌套也工作
};

// 示例 5: 带默认值的结构体（避免未初始化）
struct SafeTask {
    int id = 0;
    QString title = "Untitled";
    Priority priority = Priority::Low;  // 默认值
    StatusCode status = StatusCode::Success;  // 默认值
    Weekday dueDay = Weekday::Monday;  // 默认值

    SERIALIZE(id, title, priority, status, dueDay)
};

// ==================== 辅助函数 ====================
void printJson(const QByteArray& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json);
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
}

void printSeparator() {
    qDebug() << "\n" << QString(60, '=') << "\n";
}

void ENumTest(){
    // ==================== 测试 1: 基础枚举序列化 ====================
        printSeparator();
        qDebug() << "测试 1: 基础枚举序列化";

        Task task;
        task.id = 1001;
        task.title = "完成项目报告";
        task.priority = Priority::High;
        task.status = StatusCode::Success;
        task.dueDay = Weekday::Friday;

        QByteArray json1 = task.toByteArray();
        printJson(json1);

        // 反序列化
        Task task2;
        bool ok = task2.fromByteArray(json1);
        qDebug() << "反序列化成功:" << ok;
        qDebug() << "Task ID:" << task2.id;
        qDebug() << "标题:" << task2.title;
        qDebug() << "优先级值:" << task2.priority;  // 输出 10
        qDebug() << "状态码:" << task2.status;       // 输出 200
        qDebug() << "到期日:" << task2.dueDay;       // 输出 4 (Friday)

        // ==================== 测试 2: enum class ====================
        printSeparator();
        qDebug() << "测试 2: enum class (UserRole)";

        User user;
        user.userId = 2001;
        user.username = "张三";
        user.role = UserRole::Admin;
        user.isActive = true;

        QByteArray json2 = user.toByteArray();
        printJson(json2);

        User user2;
        user2.fromByteArray(json2);
        qDebug() << "用户名:" << user2.username;
        qDebug() << "角色值:" << static_cast<int>(user2.role);  // 输出 3
        qDebug() << "活跃状态:" << user2.isActive;

        // ==================== 测试 3: 容器中的枚举 ====================
        printSeparator();
        qDebug() << "测试 3: 容器中的枚举";

        Schedule schedule;
        schedule.name = "开发组工作安排";
        schedule.workingDays = {Weekday::Monday, Weekday::Tuesday, Weekday::Wednesday,
                                Weekday::Thursday, Weekday::Friday};
        schedule.taskPriorities = {Priority::High, Priority::Medium, Priority::Critical};
        schedule.dailyTasks[Weekday::Monday] = "晨会";
        schedule.dailyTasks[Weekday::Wednesday] = "代码审查";
        schedule.dailyTasks[Weekday::Friday] = "项目总结";

        QByteArray json3 = schedule.toByteArray();
        printJson(json3);

        Schedule schedule2;
        schedule2.fromByteArray(json3);
        qDebug() << "工作日数量:" << schedule2.workingDays.size();
        qDebug() << "优先级数量:" << schedule2.taskPriorities.size();
        qDebug() << "周一日程:" << schedule2.dailyTasks[Weekday::Monday];

        // ==================== 测试 4: 嵌套结构体 ====================
        printSeparator();
        qDebug() << "测试 4: 嵌套结构体";

        Team team;
        team.teamName = "核心开发组";
        team.defaultRole = UserRole::Moderator;

        User member1;
        member1.userId = 3001;
        member1.username = "李四";
        member1.role = UserRole::User;
        member1.isActive = true;
        team.members.append(member1);

        User member2;
        member2.userId = 3002;
        member2.username = "王五";
        member2.role = UserRole::Admin;
        member2.isActive = true;
        team.members.append(member2);

        QByteArray json4 = team.toByteArray();
        printJson(json4);

        Team team2;
        team2.fromByteArray(json4);
        qDebug() << "团队名称:" << team2.teamName;
        qDebug() << "成员数量:" << team2.members.size();
        qDebug() << "默认角色值:" << static_cast<int>(team2.defaultRole);

        // ==================== 测试 5: 默认值保护 ====================
        printSeparator();
        qDebug() << "测试 5: 默认值保护（避免未初始化）";

        SafeTask safeTask;
        qDebug() << "默认优先级:" << safeTask.priority;  // 输出 1 (Low)
        qDebug() << "默认状态码:" << safeTask.status;    // 输出 200 (Success)
        qDebug() << "默认到期日:" << safeTask.dueDay;    // 输出 0 (Monday)

        // 从缺少字段的 JSON 反序列化
        QByteArray partialJson = "{\"id\":5001,\"title\":\"紧急修复\"}";  // 缺少枚举字段
        SafeTask safeTask2;
        safeTask2.fromByteArray(partialJson);
        qDebug() << "反序列化后优先级（保持默认）:" << safeTask2.priority;  // 仍为 1
        qDebug() << "反序列化后状态码（保持默认）:" << safeTask2.status;    // 仍为 200

        // ==================== 测试 6: 错误数据处理 ====================
        printSeparator();
        qDebug() << "测试 6: 错误数据处理";

        // 6a: 超出范围的枚举值（SERIALIZE_ENUM 不检查范围，会接受）
        QByteArray badJson1 = "{\"id\":6001,\"title\":\"测试\",\"priority\":999,\"status\":200,\"dueDay\":0}";
        Task task3;
        ok = task3.fromByteArray(badJson1);
        qDebug() << "超出范围的枚举值（999）反序列化:" << (ok ? "成功" : "失败");
        qDebug() << "优先级被设置为:" << task3.priority;  // 输出 999（无效值！）
        qDebug() << "注意: SERIALIZE_ENUM 不检查范围，所以 999 被接受";

        // 6b: 类型错误（字符串而不是数字）
        QByteArray badJson2 = "{\"id\":6002,\"title\":\"测试\",\"priority\":\"high\",\"status\":200,\"dueDay\":0}";
        Task task4;
        ok = task4.fromByteArray(badJson2);
        qDebug() << "类型错误（字符串）反序列化:" << (ok ? "成功" : "失败");  // 应该失败

        // 6c: 手动验证枚举值
        qDebug() << "\n建议: 反序列化后手动验证枚举值";
        bool isValidPriority = (task3.priority == Priority::Low ||
                               task3.priority == Priority::Medium ||
                               task3.priority == Priority::High ||
                               task3.priority == Priority::Critical);
        qDebug() << "task3 的优先级是否有效:" << isValidPriority;  // false

        // ==================== 测试 7: 负值枚举 ====================
        printSeparator();
        qDebug() << "测试 7: 负值枚举";

        struct Weather {
            QString city;
            Temperature temp;
            SERIALIZE(city, temp)
        };

        Weather weather;
        weather.city = "北京";
        weather.temp = Temperature::Cold;

        QByteArray json5 = weather.toByteArray();
        printJson(json5);

        Weather weather2;
        weather2.fromByteArray(json5);
        qDebug() << "城市:" << weather2.city;
        qDebug() << "温度值:" << weather2.temp;  // 输出 -10

        // ==================== 测试 8: 复杂的 Map 结构 ====================
        printSeparator();
        qDebug() << "测试 8: 复杂的 Map 结构";

        struct Project {
            QString name;
            QMap<Weekday, Priority> dailyPriority;
            SERIALIZE(name, dailyPriority)
        };

        Project project;
        project.name = "项目X";
        project.dailyPriority[Weekday::Monday] = Priority::Low;
        project.dailyPriority[Weekday::Wednesday] = Priority::High;
        project.dailyPriority[Weekday::Friday] = Priority::Critical;

        QByteArray json6 = project.toByteArray();
        printJson(json6);

        Project project2;
        project2.fromByteArray(json6);
        qDebug() << "项目:" << project2.name;
        qDebug() << "周三优先级:" << project2.dailyPriority[Weekday::Wednesday];  // 输出 10

        // ==================== 总结 ====================
        printSeparator();
        qDebug() << "========== 测试总结 ==========";
        qDebug() << "✅ SERIALIZE_ENUM 适用于所有枚举类型";
        qDebug() << "✅ 支持标准枚举、enum class、负值、不连续值";
        qDebug() << "✅ 支持容器中的枚举（QList, QVector, QMap 等）";
        qDebug() << "✅ 支持嵌套结构体";
        qDebug() << "✅ 配合默认值可避免未初始化";
        qDebug() << "⚠️  SERIALIZE_ENUM 不检查范围，需要手动验证";
        qDebug() << "==========================================\n";
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

void modelTest(){
    UserModel user;
        user. setId(10086);
        user.setUserName("李四");
        user.setCreateTime(1788888888888LL);
        user.setIsVip(true);
        user.setNote("这是一条备注信息");

        // 二进制数据示例
        QByteArray bin = "hello binary data";
        user.setAvatarBin(bin);

        // 嵌套对象赋值：拿到引用直接操作子模型字段
        user.address().setCity("北京市");
        user.address().setStreet("未来科学城路");
        user.address().setHouseNumber(88);
        user.address().setZipCode(12);

        // ========== 2. 序列化为 QJsonObject ==========
        QJsonObject jsonObj = user.toJson();
        qDebug() << "===== toJson 输出 =====";
        qDebug() << jsonObj;

        // ========== 3. 保存到本地json文件 ==========
        bool saveOk = user.saveToFile("./user_data.json");
        qDebug() << "保存文件结果:" << saveOk;

        // ========== 4. 新建空模型，从文件加载恢复数据 ==========
        UserModel userLoad;
        bool loadOk = userLoad.loadFromFile("./user_data.json");
        qDebug() << "加载文件结果:" << loadOk;

        // 读取加载出来的数据
        qDebug() << "\n===== 读取加载后的数据 =====";
        qDebug() << "id:" << userLoad.Id();
        qDebug() << "userName:" << userLoad.UserName();
        qDebug() << "isVip:" << userLoad.IsVip();
        qDebug() << "note(std::string):" << QString::fromStdString(userLoad.Note());
        qDebug() << "avatarBin:" << userLoad.AvatarBin();

        // 读取嵌套模型
        qDebug() << "地址-城市:" << userLoad.address().City();
        qDebug() << "地址-街道:" << userLoad.address().Street();
        qDebug() << "地址-门牌号:" << userLoad.address().HouseNumber();

        // ========== 5. 从QJsonObject反序列化 ==========
        UserModel userFromJson;
        userFromJson.fromJson(jsonObj);
        qDebug() << "\nfromJson 恢复用户名：" << userFromJson.UserName();
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

    //    RouteWidget r;
    //    r.show();
//     ENumTest();
modelTest();


    return app.exec();
}
