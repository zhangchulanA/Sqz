# Sqz 项目开发规范文档

> **版本**：1.0.0  
> **适用项目**：基于 Sqz 框架的 Qt 5.12 / C++17 应用项目  
> **最后更新**：2026-08-04

---

## 目录

1. [概述](#1-概述)
2. [目录结构与 Pri 模块体系](#2-目录结构与-Pri-模块体系)
3. [命名规范](#3-命名规范)
4. [文件组织结构](#4-文件组织结构)
5. [代码风格](#5-代码风格)
6. [注释规范](#6-注释规范)
7. [模块间调用规范](#7-模块间调用规范)
8. [SqzState 使用规范](#8-sqzstate-使用规范)
9. [SqzBus 使用规范](#9-sqzbus-使用规范)
10. [视图开发规范](#10-视图开发规范)
11. [服务开发规范](#11-服务开发规范)
12. [配置文件规范](#12-配置文件规范)
13. [错误处理与日志规范](#13-错误处理与日志规范)
14. [扩展模块规范（Network / Database / Plugins）](#14-扩展模块规范Network--Database--Plugins)

---

## 1. 概述

### 1.1 文档目的

本规范旨在为基于 Sqz 框架的项目开发提供统一的代码编写标准，确保团队成员在命名、文件组织、代码风格、模块协作等方面保持一致，提升代码可读性、可维护性和可扩展性。

### 1.2 核心原则

| 原则 | 说明 |
|---|---|
| **一致性** | 全项目遵循同一套规范，不允许个人风格偏差 |
| **可读性优先** | 代码写给读者看，而非仅给编译器看 |
| **模块化解耦** | 各 Pri 层职责单一，通过 SqzState / SqzBus 通信，禁止跨层直接引用 |
| **防御性编程** | 所有外部输入需校验，所有指针需判空 |
| **注释同步** | 代码变更时注释必须同步更新，过时注释比没有注释更糟 |

### 1.3 技术栈约束

- **语言标准**：C++17（`CONFIG += c++17`）
- **Qt 版本**：Qt 5.12（不使用 Qt 6 专属 API）
- **构建系统**：qmake + Pri 模块化
- **架构库**：SqzCore（预编译库链接）

---

## 2. 目录结构与 Pri 模块体系

### 2.1 标准目录结构

```
ProjectName/
├── ProjectName.pro            # 顶层工程（include 各层 Pri + 全局配置）
├── SqzAppConfig.json          # 配置文件
│
├── src/
│   ├── app/                   # 应用入口层
│   │   ├── main.cpp
│   │   └── App.pri
│   ├── common/                # 公共定义层
│   │   ├── AppConstants.h
│   │   ├── AppTypes.h
│   │   └── Common.pri
│   ├── models/                # 数据模型层
│   │   └── Models.pri
│   ├── services/              # 服务层
│   │   └── Services.pri
│   ├── views/                 # 视图层
│   │   └── Views.pri
│   ├── network/               # Network 通信层（按需）
│   │   └── Network.pri
│   ├── database/              # Database 持久化层（按需）
│   │   └── Database.pri
│   └── plugins/               # Plugins 插件层（按需）
│       └── Plugins.pri
│
├── qml/
│   ├── pages/                 # 页面级 QML
│   └── components/            # 可复用 QML 组件
│
├── resources/
│   ├── qml.qrc
│   ├── icons/
│   └── Resources.pri
│
├── config/
├── scripts/
├── tests/
├── docs/
└── README.md
```

### 2.2 各 Pri 模块职责

#### App.pri — 应用入口层

| 项 | 说明 |
|---|---|
| **职责** | 应用程序入口，创建 QApplication + SqzApplication，调用 Init() |
| **内容** | 仅 `main.cpp`，不含业务逻辑 |
| **依赖** | 依赖 SqzApplication（架构库） |
| **规则** | 禁止在此层编写任何业务代码 |

#### Common.pri — 公共定义层

| 项 | 说明 |
|---|---|
| **职责** | 全局常量（消息名、State key）、公共类型（枚举、结构体）、工具函数声明 |
| **内容** | `AppConstants.h`、`AppTypes.h`（纯头文件，无 .cpp） |
| **依赖** | 无（最底层，被所有层依赖） |
| **规则** | 此层是全项目"契约层"，修改需评审影响范围 |

#### Models.pri — 数据模型层

| 项 | 说明 |
|---|---|
| **职责** | 业务实体数据结构（DTO / Entity / Value Object），数据转换逻辑 |
| **内容** | 数据结构头文件 + 可选 .cpp（复杂转换逻辑） |
| **依赖** | Common |
| **规则** | 不含业务逻辑，不持有运行时状态，纯数据容器 |

#### Services.pri — 服务层

| 项 | 说明 |
|---|---|
| **职责** | 后台业务逻辑（无界面），继承 SqzService |
| **内容** | 服务类 .h/.cpp，每个服务一个文件对 |
| **依赖** | Common、Models |
| **规则** | 通过 SqzState/SqzBus 与视图层通信，禁止直接引用视图类 |

#### Views.pri — 视图层

| 项 | 说明 |
|---|---|
| **职责** | 界面展示与用户交互，继承 SqzWidget / SqzQuick / SqzMainWindow |
| **内容** | 视图类 .h/.cpp，每个视图一个文件对 |
| **依赖** | Common、Models、Resources（QML） |
| **规则** | 通过 SqzState::Watch 监听数据，通过 SqzBus 发送命令，禁止直接引用服务类 |

#### Resources.pri — 资源层

| 项 | 说明 |
|---|---|
| **职责** | 管理 .qrc 资源文件（QML、图标、图片、翻译） |
| **内容** | .qrc 文件 + 资源目录 |
| **依赖** | 无（独立层） |
| **规则** | 所有 QML 文件必须通过 .qrc 注册，禁止运行时文件路径加载 |

### 2.3 顶层 .pro 文件规范

顶层 .pro 只做三件事：全局 QT 模块配置、架构库链接、include 各层 Pri：

```pro
# ProjectName.pro
QT += core gui widgets qml quick quickwidgets
TEMPLATE = app
TARGET = ProjectName
CONFIG += c++17

# 架构库
INCLUDEPATH += /usr/include/Sqz
LIBS += -L/usr/lib/Sqz -lSqzCore

# 版本号
DEFINES += APP_PRO_VERSION=\\\"1.0.0\\\"

# 模块化构建
include(src/app/App.pri)
include(src/common/Common.pri)
include(src/models/Models.pri)
include(src/services/Services.pri)
include(src/views/Views.pri)
include(resources/Resources.pri)
```

### 2.4 Pri 文件编写规范

每个 Pri 文件统一格式：

```pro
# Xxx.pri - 层级名称
# 管理该层的源文件列表

HEADERS += \
    $$PWD/YourClass.h

SOURCES += \
    $$PWD/YourClass.cpp

# 新增文件时在此追加，保持字母排序
```

**规则**：
- 使用 `$$PWD` 引用当前目录（Pri 相对路径）
- 文件列表每行一个，反斜杠续行
- 新增文件追加到末尾，保持同类文件字母排序
- Pri 文件名首字母大写：`App.pri`、`Common.pri`、`Network.pri`

---

## 3. 命名规范

### 3.1 类命名

| 类型 | 规则 | 示例 |
|---|---|---|
| 业务类 | PascalCase，语义完整 | `MainView`、`DataService`、`HttpClient` |
| 数据结构 | PascalCase，名词 | `SensorData`、`UserProfile` |
| 枚举 | PascalCase | `ViewType`、`ErrorCode` |
| 枚举值 | PascalCase | `ViewType::Widget`、`ErrorCode::NotFound` |

### 3.2 函数命名

| 类型 | 规则 | 示例 |
|---|---|---|
| 普通函数 | camelCase，动词开头 | `openView()`、`loadConfig()` |
| 槽函数 | camelCase，on 前缀 | `onButtonClicked()`、`onTimeout()` |
| 信号 | camelCase，名词/过去式 | `dataChanged()`、`connectionLost()` |
| 属性 Getter | camelCase，名词或 is/get 前缀 | `temperature()`、`isVisible()` |
| 属性 Setter | camelCase，set 前缀 | `setTemperature()` |
| 回调/lambda | camelCase | `onDataReceived` |

### 3.3 变量命名

| 类型 | 规则 | 示例 |
|---|---|---|
| 成员变量 | m_ 前缀 + camelCase | `m_timer`、`m_updateIntervalMs` |
| 静态成员 | s_ 前缀 + camelCase | `s_instance` |
| 全局变量 | g_ 前缀 + camelCase（尽量避免） | `g_configPath` |
| 局部变量 | camelCase，无前缀 | `intervalMs`、`tempLabel` |
| 常量 | camelCase 或 UPPER_SNAKE | `constexpr auto MaxRetry = 3;` |
| 布尔变量 | is/has/can 前缀 | `isConnected`、`hasError` |
| 指针变量 | 与普通变量同规则，p 前缀不强制 | `m_timer`、`btnSave` |

### 3.4 文件命名

| 类型 | 规则 | 示例 |
|---|---|---|
| 类文件 | PascalCase，与类名一致 | `MainView.h`、`MainView.cpp` |
| Pri 文件 | PascalCase + .pri 后缀 | `App.pri`、`Network.pri` |
| QML 文件 | PascalCase | `Dashboard.qml`、`SettingsPanel.qml` |
| 资源文件 | 小写 + .qrc 后缀 | `qml.qrc`、`icons.qrc` |
| 配置文件 | PascalCase + .json | `SqzAppConfig.json` |
| 脚本文件 | 小写 + .sh | `build.sh`、`run.sh` |

### 3.5 命名空间

```cpp
// 项目业务代码统一使用 namespace App
namespace App {
    class MainView : public Sqz::SqzWidget { ... };
}

// 框架代码使用 namespace Sqz（已有，不修改）
namespace Sqz {
    class SqzHub { ... };
}
```

**规则**：
- 业务代码统一放 `namespace App`
- 常量放子命名空间 `App::Msg`、`App::State`
- 禁止使用 `using namespace` 在头文件中

### 3.6 常量命名

```cpp
// AppConstants.h
namespace App {
namespace Msg {
    // 消息名：小写点分格式（消息总线 key，保持与 JSON 配置风格一致）
    inline constexpr auto SENSOR_UPDATED = "app.sensor.updated";
    inline constexpr auto CONFIG_UPDATE_INTERVAL = "app.config.updateInterval";
}
namespace State {
    // State key：小写点分格式
    inline constexpr auto SENSOR_TEMP = "sensor.temp";
}
}
```

---

## 4. 文件组织结构

### 4.1 头文件规范

```cpp
// ============== 文件头注释 ==============
// MainView.h - 主窗口视图（一句话描述文件用途）
#ifndef MAINVIEW_H          // 头文件保护符：全大写 + _H
#define MAINVIEW_H

// ============== include 区 ==============
// 1. Qt 标准头（按字母序）
#include <QLabel>
#include <QPushButton>

// 2. 架构库头
#include "SqzWidget.h"

// 3. 项目内部头
#include "AppConstants.h"

// ============== 命名空间 ==============
namespace App {

// ============== 类定义 ==============
class MainView : public Sqz::SqzWidget
{
    Q_OBJECT

public:
    // 构造/析构
    explicit MainView(QWidget* parent = nullptr);
    ~MainView() = default;

    // 公开方法
    QString className() const override;

protected:
    // 生命周期回调
    void onInit() override;
    void onClose() override;

private slots:
    // 槽函数
    void onButtonClicked();

private:
    // 成员变量
    QLabel* m_tempLabel = nullptr;
};

} // namespace App

#endif // MAINVIEW_H
```

### 4.2 源文件规范

```cpp
// ============== 文件头注释 ==============
// MainView.cpp - 主窗口实现
#include "MainView.h"           // 对应头文件放第一

// Qt 头
#include <QVBoxLayout>
#include <QTimer>

// 架构库头
#include "SqzState.h"
#include "SqzApplication.h"

// 项目内部头
#include "AppConstants.h"

// ============== 命名空间 ==============
namespace App {

// ============== 构造函数 ==============
MainView::MainView(QWidget* parent) : SqzWidget(parent) {}

// ============== 生命周期 ==============
void MainView::onInit()
{
    // ...
}

// ============== 槽函数 ==============
void MainView::onButtonClicked()
{
    // ...
}

} // namespace App

// ============== 注册宏（必须在 namespace 外）==============
SQZ_REG_NOARG(App::MainView)
```

### 4.3 include 顺序规则

头文件和源文件的 include 统一按以下顺序排列，每组之间空行分隔：

1. 对应自己的头文件（仅 .cpp）
2. Qt 标准头（`<QLabel>`）
3. 架构库头（`"SqzWidget.h"`）
4. 项目内部头（`"AppConstants.h"`）
5. 第三方库头（如有）
6. C/C++ 标准库（`<vector>`、`<memory>`）

---

## 5. 代码风格

### 5.1 缩进与格式

- **缩进**：4 个空格，禁止 Tab
- **行宽**：不超过 120 字符
- **大括号**：K&R 风格（左括号不换行）
- **分号**：不留空格

```cpp
// ✅ 正确
void Foo::bar(int value) {
    if (value > 0) {
        doSomething();
    }
}

// ❌ 错误（Tab 缩进、左括号换行）
void Foo::bar(int value)
{
    if (value > 0)
    {
        doSomething();
    }
}
```

### 5.2 指针与引用

- 指针 `*` 紧贴变量名：`int* ptr`、`void foo(int* value)`
- 引用 `&` 紧贴变量名：`const QString& name`
- const 修饰指针：`const QObject* obj`（指向 const）或 `QObject* const`（指针本身 const）

### 5.3 auto 使用

- **允许**：迭代器、`qobject_cast` 返回值、lambda
- **禁止**：基本类型（int/double/bool）、公开 API 返回类型

```cpp
// ✅ 正确
auto* widget = qobject_cast<QWidget*>(obj);
for (auto it = map.begin(); it != map.end(); ++it) {
    // ...
}

// ❌ 错误
auto count = list.size();   // int 还是 unsigned？不明确
```

### 5.4 lambda 使用

```cpp
// 捕获列表：优先按值捕获 [this]、[=]，谨慎使用 [&] 捕获局部变量引用
SqzState::Instance()->Watch(this, State::SENSOR_TEMP,
    [this](const QVariant& v) {       // 捕获 this（生命周期由 receiver 保证）
        m_label->setText(v.toString());
    }, true);
```

**规则**：
- lambda 捕获 `this` 时，必须通过 `SqzState::Watch(this, ...)` 或 `SqzBus::Receive(this, ...)` 绑定 receiver
- 禁止捕获局部变量的引用 `[&]`（超出作用域后悬挂）
- lambda 体超过 5 行时，提取为成员函数

### 5.5 初始化

- 成员变量使用类内初始化（`= nullptr`、`= 0`）
- 构造函数初始化列表与声明顺序一致

```cpp
class Foo {
    QWidget* m_widget = nullptr;   // 类内初始化
    int m_count = 0;
};
```

---

## 6. 注释规范

### 6.1 文件头注释

每个 `.h` / `.cpp` 文件首行必须有注释：

```cpp
// MainView.h - 主窗口视图（一句话描述文件用途）
```

### 6.2 类注释

类定义前必须有注释，说明职责和用法：

```cpp
// 主窗口：展示温度数据 + 操作按钮
// 配置 IsMain=true，关闭触发退出
// 配置 Props: { "WindowTitle": "主界面" }
class MainView : public Sqz::SqzWidget {
    // ...
};
```

### 6.3 函数注释

**每个函数前必须有注释**（用户规则要求）。公开函数用详细注释，私有函数用简短注释：

```cpp
public:
    // 打开指定视图（不存在则创建，存在则激活）
    // className: 视图类名（含命名空间，如 "App::SettingsView"）
    void openView(const QString& className);

private:
    // 处理按钮点击事件
    void onButtonClicked();
```

**复杂函数**需补充参数说明和返回值说明：

```cpp
// 从 JSON 加载配置
// path: 配置文件路径
// 返回: true=加载成功, false=文件不存在或解析失败
bool loadConfig(const QString& path);
```

### 6.4 行内注释

- 仅在逻辑复杂或非显而易见处添加
- 解释"为什么"而非"做了什么"

```cpp
// D3 时序限制：onInit 时 Props 尚未通过 ApplyProps 设置
// 用 0ms 延迟读取，确保 ApplyProps 执行后再读属性
QTimer::singleShot(0, this, [this]() {
    setWindowTitle(property("WindowTitle").toString());
});
```

### 6.5 TODO / FIXME / HACK 标记

```cpp
// TODO: 添加断线重连逻辑（v1.1 计划实现）
// FIXME: 此处存在内存泄漏，QTimer 未释放（临时方案）
// HACK: 绕过 Qt 5.12 的 QML 上下文 bug，Qt 5.15 修复后移除
```

---

## 7. 模块间调用规范

### 7.1 层间调用规则

```
允许的调用方向：
  app → common / services / views（通过框架）
  services → common / models
  views → common / models / resources
  Network → common / models
  Database → common / models

禁止的调用方向：
  services → views（直接引用）  ❌ 必须通过 SqzState / SqzBus
  views → services（直接引用）  ❌ 必须通过 SqzState / SqzBus
  common → 任何业务层           ❌ common 是最底层
```

### 7.2 接口定义规范

#### 服务对外接口

服务通过 `SqzState` 暴露数据，通过 `SqzBus` 接收命令：

```cpp
// DataService.h
class DataService : public Sqz::SqzService {
protected:
    void onInit() override {
        // 暴露初始数据到 SqzState
        SqzState::Instance()->Set(State::SENSOR_TEMP, 25.0, "DataService");

        // 监听命令消息
        SqzBus::Receive(this, Msg::CONFIG_UPDATE_INTERVAL,
            [this](const QVariant& data) {
                int interval = data.toInt();
                // 处理命令...
            });
    }
};
```

#### 视图对外接口

视图通过 `OpenView` / `CloseView` 等基类方法操作其他视图，通过 `SqzBus` 发送命令：

```cpp
// MainView.cpp
void MainView::onOpenSettings() {
    // 通过框架打开设置窗口（不直接 new SettingsView）
    OpenView("App::SettingsView");
}

void MainView::onSave() {
    // 通过消息总线通知服务（不直接调用服务方法）
    SqzBus::Send(Msg::CONFIG_UPDATE_INTERVAL, 2000);
}
```

### 7.3 参数传递规范

| 场景 | 传递方式 | 说明 |
|---|---|---|
| 字符串参数 | `const QString&` | 避免 QString 拷贝 |
| 数据容器 | `const QVariantList&` / `const QVariantMap&` | Qt 惯例 |
| 大对象返回 | `const T&` 或 `T` | 小对象按值，大对象按引用 |
| 指针参数 | `QObject*`（裸指针） | Qt 对象用裸指针，不用智能指针 |
| 回调函数 | `std::function<void(const QVariant&)>` | 统一用 std::function |

### 7.4 返回值处理

```cpp
// 查询类：返回值或默认值
QVariant SqzState::Value(const QString& key, const QVariant& defaultValue = QVariant()) const;

// 判断类：返回 bool
bool SqzHub::IsExist(const QString& className);

// 创建类：返回指针（nullptr 表示失败）
QWidget* SqzHub::CreateWidget(const QString& className);
```

**规则**：
- 返回指针时，调用方必须判空
- 返回 bool 时，true=成功，false=失败
- 返回 QVariant 时，调用方需用 `isValid()` 或 `canConvert<T>()` 检查

### 7.5 错误处理机制

#### 错误处理分级

| 级别 | 处理方式 | 日志宏 | 示例 |
|---|---|---|---|
| **致命** | 中止启动 / 崩溃退出 | `logerror` | 配置加载失败、主窗口创建失败 |
| **可恢复** | warn + 跳过/回退 | `logwarn` | 单个视图创建失败、数据解析失败 |
| **正常流程** | info 日志 | `loginfo` | 服务启动、配置加载完成 |
| **调试信息** | debug 日志 | `logdebug` | 状态变化、方法调用追踪 |

#### 错误处理代码模式

```cpp
// 模式1：创建失败处理
QWidget* widget = SqzHub::Instance().CreateWidget(className);
if (!widget) {
    logwarn << "[MainView] 创建视图失败:" << className;
    return;   // 跳过，不崩溃
}

// 模式2：类型转换失败处理
QObject* obj = SqzHub::Instance().GetQuickObject(className);
auto* service = qobject_cast<DataService*>(obj);
if (!service) {
    logwarn << "[MainView] 类型转换失败，期望 DataService:" << className;
    return;
}

// 模式3：配置校验失败
int interval = property("UpdateIntervalMs").toInt();
if (interval <= 0) {
    logwarn << "[DataService] UpdateIntervalMs 无效:" << interval << "，使用默认值 1000";
    interval = 1000;   // 回退到默认值
}
```

**规则**：
- 禁止忽略错误返回值（不允许 `CreateWidget(...);` 不检查）
- 禁止用异常处理常规错误流（Qt 不鼓励异常）
- 致命错误用 `logerror` + `return false` / `exit(1)`
- 可恢复错误用 `logwarn` + 回退默认值

---

## 8. SqzState 使用规范

### 8.1 数据写入

```cpp
// 写入数据（forceUpdate=false 时值未变不通知，省流量）
SqzState::Instance()->Set(State::SENSOR_TEMP, 36.5, "DataService", false);

// 强制更新（即使值未变也通知）
SqzState::Instance()->Set(State::SENSOR_TEMP, 36.5, "DataService", true);
```

**规则**：
- `source` 参数必须填写数据来源（如类名），便于调试
- 高频更新数据用 `forceUpdate=false`（默认）去重
- 低频关键数据可用 `forceUpdate=true` 强制刷新

### 8.2 数据监控

```cpp
// 推荐用法：带 receiver 绑定（生命周期安全 + 跨线程安全）
SqzState::Instance()->Watch(this, State::SENSOR_TEMP,
    [this](const QVariant& v) {
        m_label->setText(QString::number(v.toDouble()));
    }, true);   // sendCurrent=true：注册时立即获取当前值
```

**规则**：
- **必须**带 `receiver` 参数（第一个参数），确保 receiver 销毁时自动清理
- **必须**用 `sendCurrent=true` 获取初始值（避免空状态闪烁）
- 禁止用无 receiver 的旧版 `Watch(key, callback)`（无线程安全保证）
- lambda 中操作 UI 时，框架自动跨线程投递到 receiver 线程（S1 修复）

### 8.3 数据读取

```cpp
// 读取值（带默认值）
double temp = SqzState::Instance()->Value(State::SENSOR_TEMP, 0.0).toDouble();

// 检查是否存在
if (SqzState::Instance()->Has(State::SENSOR_TEMP)) {
    // ...
}
```

---

## 9. SqzBus 使用规范

### 9.1 发送消息

```cpp
// 无参消息（事件通知）
SqzBus::Send(Msg::SENSOR_UPDATED);

// 带数据消息
SqzBus::Send(Msg::CONFIG_UPDATE_INTERVAL, 2000);
SqzBus::Send(Msg::USER_LOGIN, QVariantMap{{"name", "张三"}, {"role", "admin"}});
```

### 9.2 接收消息

```cpp
// 注册监听（带 receiver 绑定，自动生命周期管理）
SqzBus::Receive(this, Msg::SENSOR_UPDATED,
    [this]() {
        // 收到更新通知
        refreshData();
    });

// 带数据的监听
SqzBus::Receive(this, Msg::CONFIG_UPDATE_INTERVAL,
    [this](const QVariant& data) {
        int interval = data.toInt();
        // 处理...
    });

// 一次性监听（收到一次后自动注销）
quint64 id = SqzBus::ReceiveOnce(this, Msg::INIT_DONE,
    [this]() {
        // 只执行一次
    });
```

### 9.3 消息命名规范

在 `AppConstants.h` 中集中定义：

```cpp
namespace App::Msg {
    // 格式：模块名.动作名（全小写点分）
    inline constexpr auto SENSOR_UPDATED = "app.sensor.updated";
    inline constexpr auto CONFIG_UPDATE_INTERVAL = "app.config.updateInterval";
    inline constexpr auto USER_LOGIN = "app.user.login";
    inline constexpr auto USER_LOGOUT = "app.user.logout";
}
```

**规则**：
- 消息名格式：`app.模块.动作`（全小写）
- 禁止在代码中硬编码消息字符串，必须用常量
- 新增消息名后同步更新 `AppConstants.h`

---

## 10. 视图开发规范

### 10.1 视图类标准模板

```cpp
// XxxView.h - 视图用途描述
#ifndef XXXVIEW_H
#define XXXVIEW_H

#include "SqzWidget.h"   // 或 SqzQuick / SqzMainWindow

namespace App {

// 视图类描述
class XxxView : public Sqz::SqzWidget
{
    Q_OBJECT
    // 如有 QML 暴露属性：
    // Q_PROPERTY(double value READ value NOTIFY valueChanged)

public:
    explicit XxxView(QWidget* parent = nullptr);

    // 必须实现：返回注册类名（与配置 ClassName 一致）
    QString className() const override { return "App::XxxView"; }

protected:
    // 生命周期回调
    void onInit() override;
    void onClose() override;

private slots:
    // UI 事件槽
    void onButtonClicked();

private:
    // 成员变量
    QLabel* m_label = nullptr;
};

} // namespace App

#endif // XXXVIEW_H
```

### 10.2 onInit 规范

```cpp
void XxxView::onInit()
{
    // 1. Props 读取（D3 时序限制：必须延迟读取）
    QTimer::singleShot(0, this, [this]() {
        setWindowTitle(property("WindowTitle").toString());
    });

    // 2. 构建 UI 布局
    auto* layout = new QVBoxLayout(this);
    // ... 添加控件 ...

    // 3. 注册数据监听
    SqzState::Instance()->Watch(this, State::SOME_KEY,
        [this](const QVariant& v) {
            // 更新 UI
        }, true);

    // 4. 注册消息监听（如有）
    SqzBus::Receive(this, Msg::SOME_MESSAGE,
        [this](const QVariant& data) {
            // 处理消息
        });

    // 5. 窗口尺寸
    resize(400, 300);
}
```

### 10.3 注册宏规范

每个视图类的 .cpp 末尾必须有注册宏：

```cpp
// XxxView.cpp 末尾（必须在 namespace App {} 之外）
SQZ_REG_NOARG(App::XxxView)
```

**规则**：
- 注册宏必须在 `namespace` 之外
- 类名含命名空间：`SQZ_REG_NOARG(App::XxxView)`
- 配置文件的 `ClassName` 必须与注册名完全一致

### 10.4 QML 视图规范（SqzQuick）

```cpp
class DashboardView : public Sqz::SqzQuick
{
    Q_OBJECT
    // Q_PROPERTY 必须紧跟 Q_OBJECT 之后
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)

    // ...
};
```

QML 中通过 `This` 上下文访问 C++ 属性：

```qml
// Dashboard.qml
Text {
    text: This.temperature.toFixed(1) + "°C"
}
```

**规则**：
- `Q_PROPERTY` 必须在 `Q_OBJECT` 之后（Bug #15 修复要求）
- 属性 setter 中用 `qFuzzyCompare` 去重，避免无谓刷新
- QML 文件放 `qml/pages/`（页面级）或 `qml/components/`（可复用组件）

---

## 11. 服务开发规范

### 11.1 服务类标准模板

```cpp
// XxxService.h - 服务用途描述
#ifndef XXXSERVICE_H
#define XXXSERVICE_H

#include "SqzService.h"
#include <QTimer>

namespace App {

// 服务类描述
class XxxService : public Sqz::SqzService
{
    Q_OBJECT
public:
    explicit XxxService(QObject* parent = nullptr);

    QString className() const override { return "App::XxxService"; }

protected:
    void onInit() override;
    void onClose() override;

private slots:
    void onTimeout();

private:
    QTimer* m_timer = nullptr;
};

} // namespace App

#endif // XXXSERVICE_H
```

### 11.2 服务间通信

服务之间**禁止直接引用**，必须通过 `SqzState` / `SqzBus`：

```cpp
// ✅ 正确：通过 SqzBus 通知其他服务
SqzBus::Send(Msg::DATA_READY, data);

// ❌ 错误：直接调用其他服务
auto* dbService = SqzHub::Instance().CreateObject("App::DbService");
dbService->query("...");   // 耦合！
```

### 11.3 线程安全

```cpp
void XxxService::onInit()
{
    // QTimer 的 parent 必须设为 this（确保线程亲和性一致）
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &XxxService::onTimeout);
    m_timer->start(1000);

    // 跨线程操作：通过 SqzState / SqzBus（框架自动处理跨线程投递）
    // 禁止直接在子线程操作 UI 相关对象
}
```

---

## 12. 配置文件规范

### 12.1 SqzAppConfig.json 字段规范

```json
{
  "AppMeta": {
    "AppName": "ProjectName",          // 英文标识，不含空格
    "DisplayName": "项目显示名",         // 可中文，用于窗口标题
    "Version": "1.0.0",                // 语义化版本号
    "ThreadPrefix": "",                // 业务类有命名空间时留空
    "ExitDelayMs": 300,                // 退出延迟（毫秒）
    "StrictVersion": false             // 版本严格校验
  },
  "Services": [...],
  "Views": [...]
}
```

### 12.2 Props 属性命名

```json
"Props": {
  "WindowTitle": "主界面",      // PascalCase
  "UpdateIntervalMs": 1000,    // PascalCase + 单位后缀
  "MaxRetryCount": 3           // PascalCase
}
```

**规则**：
- Props 键名用 PascalCase
- 数值类属性带单位后缀（`Ms`、`Seconds`、`Px`）
- 业务类中用 `Q_PROPERTY` 声明对应属性，否则 `setProperty` 静默失败

### 12.3 ClassName 一致性

配置文件中的 `ClassName` 必须与注册宏的类名**完全一致**：

```cpp
// 注册宏
SQZ_REG_NOARG(App::MainView)
```
```json
// 配置文件
"ClassName": "App::MainView"
```

---

## 13. 错误处理与日志规范

### 13.1 日志使用

```cpp
logerror << "[ClassName] 致命错误描述:" << detail;   // 中止运行
logwarn  << "[ClassName] 警告描述:" << detail;       // 可恢复
loginfo  << "[ClassName] 正常流程:" << detail;        // 信息
logdebug << "[ClassName] 调试信息:" << detail;        // 仅 DEBUG
```

**规则**：
- 日志前缀用 `[ClassName]` 标识来源
- 日志内容用中文描述（便于排查）
- 变量值用 `<<` 追加，不用字符串拼接

### 13.2 错误处理检查清单

每个函数完成后，开发者需自检：

- [ ] 所有指针返回值是否判空？
- [ ] 所有 `qobject_cast` 是否检查结果？
- [ ] 所有 `toInt()` / `toDouble()` 是否有默认值？
- [ ] 所有 `property()` 读取是否有默认值？
- [ ] 所有文件操作是否检查返回值？
- [ ] 所有 lambda 捕获是否安全（无悬挂引用）？

---

## 14. 扩展模块规范（Network / Database / Plugins）

### 14.1 Network 通信层

**适用场景**：HTTP API 调用、WebSocket 实时通信、TCP/UDP 自定义协议

**目录结构**：
```
src/network/
├── HttpClient.h/cpp         # HTTP 请求封装
├── WebSocketClient.h/cpp    # WebSocket 客户端
├── ApiDefines.h             # API 端点定义
└── Network.pri
```

**Network.pri**：
```pro
# Network.pri - Network 通信层
QT += network

HEADERS += \
    $$PWD/HttpClient.h \
    $$PWD/WebSocketClient.h \
    $$PWD/ApiDefines.h

SOURCES += \
    $$PWD/HttpClient.cpp \
    $$PWD/WebSocketClient.cpp
```

**调用规范**：
- Network 层继承 `SqzService`，作为后台服务运行
- 通过 `SqzState` 暴露网络状态（`network.connected`）
- 通过 `SqzBus` 接收请求消息（`network.http.get`）、发送响应消息（`network.http.response`）
- 禁止在视图层直接调用 Network 类

### 14.2 Database 持久化层

**适用场景**：SQLite 本地存储、配置持久化、数据缓存

**目录结构**：
```
src/database/
├── DbManager.h/cpp          # 数据库连接管理
├── ConfigStore.h/cpp         # 配置持久化
├── DbSchema.h               # 表结构定义
└── Database.pri
```

**Database.pri**：
```pro
# Database.pri - Database 持久化层
QT += sql

HEADERS += \
    $$PWD/DbManager.h \
    $$PWD/ConfigStore.h \
    $$PWD/DbSchema.h

SOURCES += \
    $$PWD/DbManager.cpp \
    $$PWD/ConfigStore.cpp
```

**调用规范**：
- Database 层继承 `SqzService`，作为后台服务运行
- 对外接口通过 `SqzBus` 消息（`db.query`、`db.insert`）
- 查询结果通过 `SqzState` 推送（`db.result.{table}`）
- 禁止在视图层直接执行 SQL

### 14.3 Plugins 插件层

**适用场景**：运行时动态加载扩展模块

**目录结构**：
```
src/plugins/
├── PluginInterface.h        # 插件接口定义（纯虚类）
├── PluginManager.h/cpp      # 插件加载/管理
└── Plugins.pri
```

**Plugins.pri**：
```pro
# Plugins.pri - Plugins 插件层
HEADERS += \
    $$PWD/PluginInterface.h \
    $$PWD/PluginManager.h

SOURCES += \
    $$PWD/PluginManager.cpp
```

**调用规范**：
- 插件必须实现 `PluginInterface` 纯虚接口
- `PluginManager` 继承 `SqzService`，通过 `SqzBus` 接收加载/卸载命令
- 插件通过 `SqzState` / `SqzBus` 与主程序通信，禁止直接访问内部类
- 插件 .so 文件放在 `plugins/` 目录，运行时扫描加载

### 14.4 扩展模块集成

在顶层 `.pro` 中按需 include：

```pro
# 按需启用扩展模块（注释掉不需要的）
include(src/network/Network.pri)
include(src/database/Database.pri)
include(src/plugins/Plugins.pri)
```

---

## 附录 A：代码审查检查清单

提交代码前，开发者需逐项自检：

### 通用项
- [ ] 文件头注释已添加
- [ ] 每个函数前有注释
- [ ] 复杂逻辑有行内注释
- [ ] include 顺序符合规范
- [ ] 命名符合规范（PascalCase/camelCase）
- [ ] 无 Tab 缩进（4 空格）
- [ ] 行宽不超过 120 字符

### 架构项
- [ ] 注册宏在 namespace 外
- [ ] ClassName 与配置一致
- [ ] Props 用 Q_PROPERTY 声明
- [ ] onInit 中 Props 延迟读取（D3 限制）
- [ ] Watch 带了 receiver 参数
- [ ] 消息名用常量（非 magic string）

### 安全项
- [ ] 指针返回值判空
- [ ] qobject_cast 结果检查
- [ ] 数值转换有默认值
- [ ] lambda 捕获无悬挂引用
- [ ] 无跨层直接引用（services↔views）

---

## 附录 B：Git 提交规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

| type | 说明 |
|---|---|
| feat | 新功能 |
| fix | 修复 bug |
| refactor | 重构 |
| docs | 文档 |
| style | 格式调整 |
| test | 测试 |
| chore | 构建/工具 |

**示例**：
```
feat(views): 添加用户设置视图

- 新增 SettingsView 类，继承 SqzWidget
- 通过 SqzBus 接收频率调整消息
- 配置 AutoStart=false，按需打开

Closes #42
```

---

*本规范随项目演进持续更新，最新版本以 docs/DevelopmentStandards.md 为准。*
