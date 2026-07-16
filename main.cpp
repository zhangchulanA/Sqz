
#include <QApplication>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QSpinBox>
#include <QTextEdit>
#include "Translator.h"
#include "SqzHub.h"
#include "ThreadPool.h"
#include "Logger.h"
#include "TimerUtils.h"

#include "CustomSearchBox.h"
#include "FlexData.h"
#include "ProtocolSchema.h"
#include "ChainBranch.h"
#include <FluentCard.h>
#include "MsgBox.h"
#include "SuperTableAll.h"
#include "UiUtils.h"
#include "RadioLink.h"
#include "SqzState.h"
#include <QAbstractNativeEventFilter>
#include <SqzApplication.h>
#include <DataTransporter.h>
#include <ShortcutManager.h>
#include "SERIALIZE.h"
using namespace Net;
using namespace Sqz;

// ============ 嵌套结构体 ============
struct Address {
    QString city;
    QString street;
    int number;

    SERIALIZE(city, street, number)
};

struct Contact {
    QString phone;
    QString email;

    SERIALIZE(phone, email)
};

// ============ 复杂结构体 ============
struct Person {
    QString name;
    int age;
    double height;
    bool isStudent;
    Address address;          // 嵌套结构体
    QList<Contact> contacts;  // Qt 容器
    QVector<QString> tags;    // Qt 容器
    std::vector<int> scores;  // STL 容器
    QMap<QString, int> stats; // Qt Map
    QHash<QString, double> metrics; // Qt Hash

    SERIALIZE(name, age, height, isStudent, address, contacts, tags, scores, stats, metrics)

};

int main() {
    // ============ 构建复杂对象 ============
    Person p1;
    p1.name = "张三";
    p1.age = 25;
    p1.height = 175.5;
    p1.isStudent = true;

    p1.address.city = "北京";
    p1.address.street = "长安街";
    p1.address.number = 1;

    Contact c1{"13800138000", "zhangsan@email.com"};
    Contact c2{"13900139000", "zhangsan@work.com"};
    p1.contacts.append(c1);
    p1.contacts.append(c2);

    p1.tags = {"程序员", "背包客", "摄影师"};
    p1.scores = {95, 87, 92, 88};

    p1.stats["访问量"] = 1024;
    p1.stats["点赞"] = 256;

    p1.metrics["满意度"] = 4.8;
    p1.metrics["完成度"] = 0.95;

    // ============ 序列化 ============
    QByteArray data = p1.toByteArray();
    qDebug().noquote() << data;

    // 漂亮的 JSON 输出
    QJsonDocument doc = QJsonDocument::fromJson(data);
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);

    // ============ 反序列化 ============
    Person p2;
    if (p2.fromByteArray(data)) {
        qDebug() << "反序列化成功!";
        qDebug() << "姓名:" << p2.name;
        qDebug() << "地址:" << p2.address.city << p2.address.street;
        qDebug() << "联系人数量:" << p2.contacts.size();
        qDebug() << "标签:" << p2.tags;
        qDebug() << "成绩:" << p2.scores.size() << "门";
        qDebug() << "统计数据:" << p2.stats;
    }

    return 0;
}
