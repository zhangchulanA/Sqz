# ProtocolSchema 使用说明文档

> 二进制协议编解码器 · 支持比特级精确解析 / 打包 / 条件分支 / 枚举映射 / JSON 配置加载
> 框架：Qt 5.12+（C++14） · 命名空间：`Sqz` · 别名：`Sqz::PtlSc`

---

## 目录

1. [文件概述](#1-文件概述)
2. [接口详细说明](#2-接口详细说明)
3. [复杂协议说明](#3-复杂协议说明)
4. [超级变态复杂协议详解](#4-超级变态复杂协议详解)
5. [小协议说明](#5-小协议说明)
6. [JSON 加载与导出示例](#6-json-加载与导出示例)
7. [使用步骤](#7-使用步骤)
8. [注意事项](#8-注意事项)

---

## 1. 文件概述

### 1.1 这是什么

`ProtocolSchema` 是一个**声明式二进制协议编解码器**。你只需把协议的字段布局"描述"出来（每个字段在哪一字节、哪一位、多长、什么类型），它就能自动完成两件事：

- **parse（解析）**：把收到的原始字节流 `QByteArray` → 结构化的 `QJsonObject`
- **pack（打包）**：把 `QJsonObject` → 原始字节流 `QByteArray`

你不再需要手写位移、掩码、字节序转换、符号扩展这些易错的底层代码。

### 1.2 应用场景

| 场景 | 说明 |
|---|---|
| 串口/网络协议解析 | 工业设备、传感器、Modbus 类私有协议 |
| 比特级字段 | 一个字节里塞 4 个 2-bit 状态位、12-bit 数值 |
| 变长帧 | 帧体长度由前面的"长度字段"动态决定 |
| 多分支协议 | 同一协议按"帧类型"走不同字段布局（数据帧/命令帧/心跳帧…） |
| 枚举映射 | 原始数值 ↔ 中文/英文标签字符串 |
| 工程量变换 | 原始值经 `factor*raw + offset` 还原为物理量（温度、电压…） |
| 配置驱动 | 整套协议写进 JSON 文件，换协议只换文件不改代码 |

### 1.3 核心文件

| 文件 | 作用 |
|---|---|
| `ProtocolSchema.h` | 类、枚举、`Field` 结构、`ConditionalBuilder` 全部声明 |
| `ProtocolSchema.cpp` | 全部实现（位操作、解析、打包、JSON 加载、条件求值） |
| `test/protocol.json` | 工业物联网测试协议配置（JSON 驱动范例） |
| `test/main_json.cpp` | JSON 加载功能测试（55 用例） |
| `test/main.cpp` | 编程式 API 测试（37 用例） |

---

## 2. 接口详细说明

### 2.1 全局枚举

```cpp
namespace Sqz {

// 字节序：仅当字段位宽 > 8 时才生效，单字节内忽略
enum Endian { LittleEndian, BigEndian };

// 字节内比特顺序
enum BitOrder {
    MsbFirst,   // 高位优先：bit7 为最高位，startBit=0 表示从字节高位开始（常规网络协议）
    Physical    // 物理位索引：bit0 为最低位，startBit=0 表示从字节最右侧开始，不反转权重
};

// 字段值类型（决定 JSON 中的存储形式）
enum ValueType {
    Int,        // 有符号整数（JSON 存为数字）
    UInt,       // 无符号整数（JSON 存为数字）
    HexString,  // 十六进制字符串，如 "1a2b"
    Base64,     // Base64 编码字符串
    RawBytes,   // 原始字节数组（JSON 中存为 Base64）
    String      // UTF-8 字符串
};

}
```

> **关于 BitOrder 的选择**：`Physical` 适合"bit0 是最低位"的常规小端位域；`MsbFirst` 适合网络协议中"从字节最高位开始读"的场景。两者只影响**字节内的位排列**，不影响多字节序。

### 2.2 Field 结构（字段定义）

每个字段在内部由 `ProtocolSchema::Field` 描述，理解它有助于读懂 JSON 配置：

| 成员 | 类型 | 说明 |
|---|---|---|
| `name` | QString | 字段名（JSON 中的 key） |
| `startByte` | int | 起始字节偏移（0-based） |
| `startBit` | int | 起始位（0~7，含义由 BitOrder 决定） |
| `bitLength` | int | 比特长度，**0 表示变长**（需配合 lenField） |
| `type` | ValueType | 值类型 |
| `endian` | Endian | 字节序（仅 bitLength > 8 有效） |
| `bitOrder` | BitOrder | 比特顺序 |
| `isSigned` | bool | 是否有符号（仅对 Int 有效） |
| `lenField` | QString | 变长字段依赖的长度字段名 |
| `factor` | double | 线性变换系数（默认 1.0） |
| `offset` | double | 线性变换偏移（默认 0.0） |
| `valueMap` | QHash<quint64,QString> | 枚举映射表（原始值→字符串） |
| `hasMapping` | bool | 是否启用枚举映射（与线性变换**互斥**） |
| `conditions` | QList<QPair<QString,QVariant>> | 条件列表（AND 逻辑，空=无条件） |
| `isDefaultBranch` | bool | 是否为默认分支 |
| `defaultBranchField` | QString | 默认分支对应的条件字段名 |

---

### 2.3 addField —— 添加固定长度字段

```cpp
ProtocolSchema& addField(const QString& name, int startByte, int startBit, int bitLength,
                         ValueType type = UInt, Endian endian = LittleEndian,
                         BitOrder bitOrder = Physical, bool isSigned = false,
                         double factor = 1.0, double offset = 0.0,
                         QString* err = nullptr);
```

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| name | QString | 是 | 字段名，不可为空，不可与已有字段重名 |
| startByte | int | 是 | 起始字节偏移，≥0 |
| startBit | int | 是 | 起始位，0~7 |
| bitLength | int | 是 | 比特长度，1~64 |
| type | ValueType | 否 | 默认 UInt |
| endian | Endian | 否 | 默认 LittleEndian，仅 >8 位有效 |
| bitOrder | BitOrder | 否 | 默认 Physical |
| isSigned | bool | 否 | 默认 false，仅对 Int 有效 |
| factor | double | 否 | 默认 1.0 |
| offset | double | 否 | 默认 0.0 |
| err | QString* | 否 | 错误信息输出 |

**返回**：`*this`（支持链式调用）。

**调用示例**：

```cpp
using namespace Sqz;
ProtocolSchema s;
QString err;

// 字节0低4位无符号数，字节1起16位有符号小端温度，系数0.1偏移-40
s.addField("flag",   0, 0, 4,  UInt)
 .addField("temp",   1, 0, 16, Int, LittleEndian, Physical, true, 0.1, -40.0, &err);

QByteArray rx = QByteArray::fromHex("01000001");  // flag=1(低4位), temp=0x0100=256
QJsonObject j = s.parse(rx);
// j = {"flag":1, "temp":-14.4}   （256*0.1-40 = -14.4）
```

**错误码**：`Field name cannot be empty` / `Field X startBit must 0~7` / `Field X bitLength must 1~64` / `Duplicate field name: X`。

---

### 2.4 addVariableField —— 添加变长字段

```cpp
ProtocolSchema& addVariableField(const QString& name, int startByte, int startBit,
                                 const QString& lenField, ValueType type = RawBytes,
                                 Endian endian = LittleEndian, BitOrder bitOrder = MsbFirst,
                                 double factor = 1.0, double offset = 0.0,
                                 QString* err = nullptr);
```

| 参数 | 说明 |
|---|---|
| lenField | **必填**，指向另一个已定义字段，该字段的数值 = 本变长字段的字节数 |
| bitLength | 内部固定为 0（标记为变长） |
| 其它 | 同 addField |

**调用示例**：

```cpp
ProtocolSchema s;
s.addField("len",    0, 0, 8)               // 长度字段：1字节
 .addVariableField("payload", 1, 0, "len", String);  // 变长字符串，长度由 len 决定

QByteArray rx = QByteArray::fromHex("05") + "hello";  // len=5, payload="hello"
QJsonObject j = s.parse(rx);
// j = {"len":5, "payload":"hello"}
```

**打包时**：`pack` 先计算变长字段内容长度，回写到 lenField。若 JSON 里缺 payload 值会报 `Missing value for variable field 'payload'`。

---

### 2.5 map —— 枚举映射（链式）

```cpp
ProtocolSchema& map(quint64 rawValue, const QString& str, QString* err = nullptr);
```

为**最近添加的字段**追加一条 `原始字节值 → 字符串` 的映射。

| 参数 | 说明 |
|---|---|
| rawValue | 原始字节值（解析时从比特流读到的无符号整数） |
| str | 映射目标字符串（解析后写入 JSON 的 value） |

**核心规则（互斥机制）**：

- 一旦调用 `map`，该字段 `hasMapping=true`，`factor`/`offset` **自动重置为 1.0/0.0**，线性变换失效。
- 解析后该字段 JSON value **必为字符串类型**（命中映射输出对应字符串；未命中输出数值字符串如 `"3"`）。
- 打包时接受字符串**自动反查**为原始值写入；也接受数字直接写入。
- 条件比较时统一按**原始数值**比较（见 [§3.3](#33-条件判断基于原始数值)）。

**调用示例**：

```cpp
ProtocolSchema s;
s.addField("status", 0, 0, 8, UInt)
 .map(0, "关机")
 .map(1, "开机")
 .map(2, "待机");

QJsonObject j = s.parse(QByteArray::fromHex("01"));
// j = {"status":"开机"}

QJsonObject tx; tx["status"] = "待机";
QByteArray packed = s.packToArray(tx);   // packed = 0x02
```

**错误码**：`map() called before any addField/addVariableField` / `map() only valid for Int/UInt field, field 'X' type mismatch`。

---

### 2.6 when / otherwise —— 条件分支

```cpp
ConditionalBuilder when(const QString& fieldName, const QVariant& value);
ConditionalBuilder otherwise();
```

| 参数 | 说明 |
|---|---|
| fieldName | 条件字段名（必须已通过 addField 定义） |
| value | 条件期望值（基于原始数值比较） |

`when(field, value)` 表示"当 field 的原始值 == value 时，后续字段生效"。`otherwise()` 是默认分支（所有 when 都不匹配时生效）。

**返回** `ConditionalBuilder`，可在其上继续 `addField` / `addVariableField` / `map` / `when`（嵌套）/ `otherwise` / `endBranch`。

**调用示例**：

```cpp
ProtocolSchema s;
s.addField("type", 0, 0, 8);                    // 条件字段
s.when("type", 1)                               // type==1 数据帧
    .addField("temp", 1, 0, 16, Int, LittleEndian, Physical, true, 0.1, -40.0)
    .endBranch();
s.when("type", 2)                               // type==2 命令帧
    .addField("cmd", 1, 0, 8)
    .endBranch();
s.otherwise()                                   // 默认分支
    .addField("errCode", 1, 0, 8)
    .endBranch();
```

> **endBranch()** 结束当前分支返回 `ProtocolSchema&`，否则后续 addField 会错误地挂在分支内。

---

### 2.7 ConditionalBuilder —— 分支构建器

| 接口 | 说明 |
|---|---|
| `addField(...)` | 向当前分支添加固定字段，参数同 ProtocolSchema::addField |
| `addVariableField(...)` | 向当前分支添加变长字段 |
| `map(rawValue, str)` | 为分支内最近字段追加枚举映射 |
| `when(field, value)` | 在当前分支内**嵌套**子分支（条件 AND 累加） |
| `otherwise()` | 当前分支的默认子分支 |
| `endBranch()` | 结束当前分支，返回上一级（根级返回 ProtocolSchema&） |

嵌套分支会**继承父分支的条件**（AND 逻辑），详见 [§4](#4-超级变态复杂协议详解)。

---

### 2.8 loadJson / loadFile / protocolName —— JSON 配置加载

```cpp
bool loadJson(const QJsonObject& root, QString* err = nullptr);  // 从 JSON 对象加载
bool loadFile(const QString& path, QString* err = nullptr);      // 从 JSON 文件加载
QString protocolName() const;                                      // 获取协议名
```

- `loadJson`：先 `clear()`，再按 `protocolName` → `defaults` → `enumMaps` → `fields` 顺序装载，最后 `validateSchema`。**任意步骤失败均回滚到空 schema**。
- `loadFile`：读文件 → `QJsonDocument::fromJson` 解析 → `loadJson`。
- `protocolName`：返回 JSON 中 `protocolName` 字段（编程式构建时为空）。

**JSON 顶层结构**：

```json
{
  "protocolName": "协议名",
  "defaults":   { ... },   // 可选：标量属性缺省值继承
  "enumMaps":   { ... },   // 可选：命名枚举库
  "fields":     [ ... ]    // 必填：字段数组
}
```

详见 [§6](#6-json-加载与导出示例)。

---

### 2.9 parse / pack / packToArray —— 解析与打包

```cpp
QJsonObject parse(const QByteArray& data, QString* errorMsg = nullptr) const;
bool pack(const QJsonObject& values, QByteArray& out, QString* errorMsg = nullptr) const;
QByteArray packToArray(const QJsonObject& values, QString* errorMsg = nullptr) const;
```

**parse 输出**：`QJsonObject`，key=字段名，value 类型由字段决定（整数=数字、枚举=字符串、HexString=十六进制串、变长=对应类型）。解析失败的字段 value 为 `null`。

**pack 输入**：`QJsonObject`，需包含所有**无条件非默认分支字段**的值；条件字段按当前帧类型提供；变长字段需提供内容（长度自动回写）。

**pack 输出**：`QByteArray`，长度 = 整个 schema 最大字节范围（含未激活条件字段的保留区，填 0）。

**调用示例**：

```cpp
QJsonObject j = schema.parse(rxData);
// 反向
QByteArray tx = schema.packToArray(j);
```

---

### 2.10 checkOverlap / validateSchema / clear

```cpp
QStringList checkOverlap(const QJsonObject& runtimeVarData = {}, QString* err = nullptr) const;
bool validateSchema(QString* errMsg = nullptr) const;
void clear();
```

| 接口 | 说明 |
|---|---|
| `checkOverlap` | 检测字段间比特范围是否重叠，返回重叠字段名列表（空=无重叠）。变长字段需传 runtimeVarData 提供长度字段值 |
| `validateSchema` | 校验配置合法性（当前检测变长字段循环依赖） |
| `clear` | 清空所有字段、协议名、缓存 |

---

## 3. 复杂协议说明

### 3.1 工作原理总览

```
              ┌───────────────┐
  原始字节 ──▶│   parse()     │──▶ QJsonObject（结构化数据）
  QByteArray  │  按字段定义    │
              └───────────────┘

              ┌───────────────┐
  QJsonObject │   pack()      │──▶ 原始字节
  ──────────▶│  按字段定义    │    QByteArray
              └───────────────┘
```

**parse 三遍扫描**（保证依赖顺序）：

1. **第一遍**：所有无条件固定长度字段（基础字段，供后续依赖）
2. **第二遍**：所有无条件变长字段（依赖第一遍的长度字段）
3. **第三遍**：条件字段 + 默认分支（用已解析结果做条件求值，支持嵌套实时判断）

**pack 流程**：

1. 计算所有变长字段内容长度 → 回写长度字段
2. 按整个 schema 最大字节范围分配输出缓冲（填 0）
3. 按比特偏移升序写入字段（防覆盖），条件不满足的字段跳过

### 3.2 协议数据格式规范

#### 字节序（Endian）

- **仅当 bitLength > 8 时生效**，单字节字段忽略 endian。
- `LittleEndian`：低地址存低字节（x86/ARM 默认）。
- `BigEndian`：低地址存高字节（网络序）。

#### 比特顺序（BitOrder）

| 模式 | startBit=0 含义 | 适用 |
|---|---|---|
| Physical | 从字节最右侧 bit0 开始 | 常规小端位域 |
| MsbFirst | 从字节最高位 bit7 开始 | 网络协议高位优先 |

#### 线性变换

```
物理值 = 原始值 × factor + offset
原始值 = (物理值 - offset) / factor   （打包时逆变换）
```

例：温度字段 `factor=0.1, offset=-40`，原始值 256 → `256×0.1-40 = -14.4℃`。

### 3.3 条件判断基于原始数值

**这是可靠性的关键设计**：条件比较统一在**原始数值**层面进行，而非解析后的物理值/字符串。

- 若条件字段含枚举映射（如 status 解析后是 `"开机"`），`matchCondition` 会把上下文值 `"开机"` **反查**回原始值 `1`，再与条件值 `1` 比较。
- 这保证 `when("status", 1)` 即使 status 解析为字符串也能正确匹配。
- 数值类型做宽容比较（int 与 double 视为相等，容差 1e-9）。

### 3.4 交互流程图示

```
解析流程:
  data ──▶ [第一遍: 无条件固定字段] ──▶ [第二遍: 无条件变长字段]
        ──▶ [第三遍: 条件字段(求值)] ──▶ QJsonObject

打包流程:
  JSON ──▶ [计算变长内容+回写长度] ──▶ [按schema最大范围分配缓冲]
       ──▶ [按bit偏移升序写入(条件跳过)] ──▶ QByteArray
```

### 3.5 边界情况处理

| 情况 | 处理 |
|---|---|
| 数据不足 | parse 该字段返回 null，errorMsg 提示 `Read out of data bit bounds` |
| 枚举未命中映射 | 输出原始数值字符串（如 `"3"`），保持 value 为字符串类型 |
| 打包缺字段值 | 报 `Missing value for field 'X'`，返回空 |
| 变长长度超限 | 报 `Variable length X exceeds max limit`（上限 1MB） |
| 字段位宽溢出 | 报 `Mapped raw value X overflow field Y bit width` |
| 空数据 | parse 返回空对象 |

---

## 4. 超级变态复杂协议详解

本节用一个"工业物联网高级数据帧"演示全部高级特性联动：多帧类型分支、嵌套条件、枚举映射、线性变换、变长字段、默认分支同时存在。

### 4.1 协议结构分解图

```
字节偏移:  0     1       2        3-4        5        2..     ...
        ┌──────┬──────┬────────┬─────────┬────────┬──────────────┐
        │ head │ type │subType │  temp   │tempAlarm│  ...         │
        │ 0xAA │ 1~4  │ 1~3    │ 16bit   │ 2bit   │ (按type分支) │
        └──────┴──────┴────────┴─────────┴────────┴──────────────┘
         固定    总开关  仅type=1  仅type=1  仅type=1  分支依赖type
         始终    分支    且subType 且subType 且subType
                         =1       =1       =1
```

**帧类型分支（由 type 字段决定）**：

| type 值 | 帧类型 | 后续字段 |
|---|---|---|
| 1 | 数据帧 | subType → (subType=1:temp+tempAlarm / =2:pressure / =3:humidity) |
| 2 | 命令帧 | cmdId + cmdPayloadLen + cmdPayload(变长) |
| 3 | 心跳帧 | heartbeatCounter + timestamp |
| 4 | 错误帧 | errorCode + errorDetail(变长) |
| 其它 | 默认分支 | unknownPayload(原始字节) |

### 4.2 字段含义与约束条件

以数据帧（type=1, subType=1）为例：

| 字段 | 位置 | 长度 | 类型 | 约束 | 变换 |
|---|---|---|---|---|---|
| head | byte0 | 8bit | HexString | 无条件，始终解析 | 无 |
| type | byte1 | 8bit | UInt | 无条件 | 枚举映射 1→数据帧… |
| subType | byte2 | 8bit | UInt | type==1 | 枚举映射 1→温度… |
| temp | byte3 | 16bit | Int(有符号) | type==1 且 subType==1 | `raw×0.1-40` |
| tempAlarm | byte5 | 8bit | UInt | type==1 且 subType==1 | 枚举映射（互斥线性） |

**关键约束**：
- `temp` 用线性变换 → **不能**同时用 enumMap（互斥）。
- `tempAlarm` 用 enumMap → factor/offset **自动失效**。
- `subType` 的条件 `type==1` 基于原始数值（不是字符串 `"数据帧"`）。

### 4.3 完整协议转换示例

**JSON 配置（节选）**：

```json
{
  "defaults": { "endian":"LittleEndian", "bitOrder":"Physical", "type":"UInt" },
  "enumMaps": {
    "frameType": {"1":"数据帧","2":"命令帧","3":"心跳帧"},
    "alarm":     {"0":"正常","1":"高温预警","2":"高温报警"}
  },
  "fields": [
    {"name":"head","startByte":0,"startBit":0,"bitLength":8,"type":"HexString"},
    {"name":"type","startByte":1,"startBit":0,"bitLength":8,"enumMap":"frameType"},
    {"name":"subType","startByte":2,"startBit":0,"bitLength":8,
     "conditions":[{"field":"type","value":1}], "enumMap":{"1":"温度","2":"压力"}},
    {"name":"temp","startByte":3,"startBit":0,"bitLength":16,"type":"Int","isSigned":true,
     "factor":0.1,"offset":-40.0,
     "conditions":[{"field":"type","value":1},{"field":"subType","value":1}]},
    {"name":"tempAlarm","startByte":5,"startBit":0,"bitLength":8,
     "conditions":[{"field":"type","value":1},{"field":"subType","value":1}],
     "enumMap":"alarm"}
  ]
}
```

**解析**（接收字节 → JSON）：

```cpp
ProtocolSchema s;
s.loadFile("protocol.json");

// 收到: AA 01 01 00 01 01  (head=AA, type=1, subType=1, temp=0x0100=256, tempAlarm=1)
QByteArray rx = QByteArray::fromHex("aa0101000101");
QJsonObject j = s.parse(rx);
// j = {
//   "head":"aa",
//   "type":"数据帧",        ← 命名枚举引用
//   "subType":"温度",       ← 内联枚举
//   "temp":-14.4,           ← 256*0.1-40 线性变换
//   "tempAlarm":"高温预警"   ← 命名枚举（与线性互斥）
// }
```

**打包**（JSON → 字节）：

```cpp
QJsonObject tx;
tx["head"] = "aa";
tx["type"] = "数据帧";      // 字符串自动反查 → 0x01
tx["subType"] = "温度";     // 字符串自动反查 → 0x01
tx["temp"] = -14.4;         // 逆变换 → 256
tx["tempAlarm"] = "高温预警"; // 字符串自动反查 → 0x01

QByteArray packed = s.packToArray(tx);
// packed = aa 01 01 00 01 01  (+ 保留区填0，按schema最大范围)
```

### 4.4 常见问题及解决方案

| 问题 | 原因 | 解决 |
|---|---|---|
| 条件字段解析为字符串后 `when` 不匹配 | 未启用原始数值比较 | 本库已内置 `normalizeToRawValue`，自动反查；确保条件值写**原始数字**（`"value":1` 而非 `"value":"数据帧"`） |
| 字段既有 enumMap 又有 factor | 两者互斥 | 启用 map 后 factor/offset 自动重置；若需线性变换就别加 enumMap |
| pack 后字节数比预期多 | pack 按 schema 最大范围分配 | 这是设计行为（含未激活条件字段保留区填0），非 bug |
| 变长字段报 lenField not defined | JSON 中长度字段定义在变长字段之后 | loadJson 要求 lenField **先于**变长字段定义（仅允许向前引用） |
| 默认分支总是生效 | 缺 `defaultBranchField` | `isDefaultBranch:true` 必须搭配非空 `defaultBranchField` |

---

## 5. 小协议说明

除上述"超级协议"外，库内各特性可单独作为"小协议"使用：

### 5.1 各小协议功能

| 小协议 | 功能 | 用途 |
|---|---|---|
| 纯位域协议 | 仅 addField 取字节内任意位 | 解析状态位、标志位 |
| 变长帧协议 | addField + addVariableField | TLV 结构、含负载的帧 |
| 枚举映射协议 | addField + map | 数值↔标签转换 |
| 线性变换协议 | addField 带 factor/offset | 工程量还原（温度/电压/压力） |
| 条件分支协议 | when/otherwise | 多类型帧复用一套定义 |
| 纯 JSON 驱动协议 | loadFile | 配置与代码分离，换协议不改代码 |

### 5.2 简洁使用示例

**位域小协议**（取字节0的低4位和高4位）：

```cpp
ProtocolSchema s;
s.addField("low",  0, 0, 4, Physical)   // bit0~3
 .addField("high", 0, 4, 4, Physical);  // bit4~7
QJsonObject j = s.parse(QByteArray::fromHex("1a"));
// j = {"low":10, "high":1}   (0x1a: 低4位=1010=10, 高4位=0001=1)
```

**枚举小协议**：

```cpp
ProtocolSchema s;
s.addField("mode", 0, 0, 8).map(0,"IDLE").map(1,"RUN").map(2,"STOP");
```

### 5.3 与其他协议的关联

所有"小协议"特性都**可组合**——它们对应 `Field` 结构的不同成员，`parse`/`pack` 统一处理。例如一个字段可以同时是"条件字段 + 枚举映射"（如 `tempAlarm`），也可以是"变长 + 条件"（如 `cmdPayload`）。JSON 驱动协议则是把所有小协议的配置外置到文件。

---

## 6. JSON 加载与导出示例

### 6.1 完整 JSON 加载代码

```cpp
#include "ProtocolSchema.h"
#include <QDebug>
using namespace Sqz;

// ===== 从 JSON 文件加载协议配置 =====
ProtocolSchema schema;
QString err;
if (!schema.loadFile("protocol.json", &err)) {
    qDebug() << "加载失败:" << err;
    return;
}
qDebug() << "已加载协议:" << schema.protocolName();

// 加载后直接 parse / pack
QByteArray rx = QByteArray::fromHex("aa0101000101");
QJsonObject j = schema.parse(rx);
```

### 6.2 完整 JSON 导出（构造协议配置并写文件）

库本身**不直接提供导出 API**，但可借助 Qt 的 `QJsonDocument` 自行把协议配置序列化为 JSON 文件。典型用法是把 `protocol.json` 作为"协议定义源"维护，程序只读不写。若需运行时生成：

```cpp
#include <QJsonDocument>
#include <QFile>

// 构造协议配置 JSON（结构同 protocol.json）
QJsonObject proto;
proto["protocolName"] = "动态生成协议";
proto["defaults"] = QJsonObject{
    {"endian","LittleEndian"}, {"bitOrder","Physical"}, {"type","UInt"}
};
proto["enumMaps"] = QJsonObject{
    {"frameType", QJsonObject{{"1","数据帧"},{"2","命令帧"}}}
};
QJsonArray fields;
fields.append(QJsonObject{
    {"name","head"},{"startByte",0},{"startBit",0},{"bitLength",8},{"type","HexString"}
});
fields.append(QJsonObject{
    {"name","type"},{"startByte",1},{"startBit",0},{"bitLength",8},{"enumMap","frameType"}
});
proto["fields"] = fields;

// 写出文件
QFile f("my_protocol.json");
if (f.open(QIODevice::WriteOnly)) {
    f.write(QJsonDocument(proto).toJson(QJsonDocument::Indented));
    f.close();
}

// 再加载回来验证
ProtocolSchema s;
s.loadFile("my_protocol.json");
```

### 6.3 JSON 字段含义速查表

| 顶层字段 | 必填 | 含义 |
|---|---|---|
| protocolName | 否 | 协议名（`protocolName()` 返回） |
| defaults | 否 | 标量属性缺省值，字段未写时继承 |
| enumMaps | 否 | 命名枚举库，供字段 enumMap 字符串引用 |
| fields | 是 | 字段定义数组 |

**defaults 可继承的属性**：`type` / `endian` / `bitOrder` / `isSigned` / `factor` / `offset`（字段显式指定优先于 defaults）。

**fields 内单字段属性**：

| 属性 | 必填 | 含义 |
|---|---|---|
| name | 是 | 字段名 |
| startByte | 是 | 起始字节 |
| startBit | 是 | 起始位 0~7 |
| bitLength | 固定字段必填 | 比特长度 1~64；变长字段不写（用 lenField） |
| lenField | 变长字段必填 | 长度字段名（须先于本字段定义） |
| type | 否 | 默认 UInt（可继承 defaults） |
| endian | 否 | 默认 LittleEndian（可继承） |
| bitOrder | 否 | 默认 Physical（可继承） |
| isSigned | 否 | 默认 false（可继承） |
| factor | 否 | 默认 1.0（可继承） |
| offset | 否 | 默认 0.0（可继承） |
| enumMap | 否 | 内联对象 或 enumMaps 中的命名引用字符串 |
| conditions | 否 | 条件数组，AND 逻辑，每项 `{"field":X,"value":V}` |
| isDefaultBranch | 否 | true 表示默认分支 |
| defaultBranchField | 默认分支必填 | 默认分支对应的条件字段名 |
| comment | 否 | 注释，加载时忽略 |

### 6.4 打包后的 JSON 格式结构（parse 输出）

```json
{
  "head": "aa",              // HexString → 十六进制字符串
  "type": "数据帧",           // UInt+enumMap → 字符串
  "subType": "温度",          // UInt+enumMap → 字符串
  "temp": -14.4,             // Int+线性变换 → 数字
  "tempAlarm": "高温预警",    // UInt+enumMap → 字符串
  "cmdPayloadLen": 5,        // UInt → 数字
  "cmdPayload": "hello",     // 变长 String → 字符串
  "heartbeat": 42,           // UInt → 数字
  "timestamp": 305419896     // UInt → 数字
}
```

> **值类型规律**：枚举映射字段 → 字符串；普通整数字段 → 数字；HexString → 十六进制串；String → UTF-8 字符串；RawBytes/Base64 → Base64 串。

### 6.5 解压后的 JSON 格式结构（pack 输入）

pack 接受与 parse 输出**结构一致**的 JSON。差异点：

- 枚举字段：既可传字符串（`"type":"数据帧"`，自动反查），也可传数字（`"type":1`，直接写入）。
- 线性变换字段：传**物理值**（`"temp":-14.4`），库自动逆变换为原始值。
- 变长字段：传内容（`"cmdPayload":"hello"`），长度字段可省略（自动回写）或显式提供。
- 条件字段：传当前帧类型对应的值，未激活分支字段可省略。

---

## 7. 使用步骤

### 7.1 环境配置要求

- **Qt**：5.12 及以上（测试环境 Qt 5.14.2 MinGW 64）
- **C++ 标准**：C++14
- **依赖**：仅 Qt Core 模块（`QT += core`）
- **工程集成**：把 `ProtocolSchema.h` / `ProtocolSchema.cpp` 加入工程，确保 `SqzGlobal.h` 中定义 `SQZ_FRAMEWORK_API` 导出宏（测试用桩文件已提供空宏）。

### 7.2 .pro 工程配置示例

```pro
QT -= gui
QT += core
CONFIG += console c++14
CONFIG -= app_bundle qt_gui
TEMPLATE = app
TARGET = my_app

INCLUDEPATH += $$PWD
SOURCES += main.cpp $$PWD/ProtocolSchema.cpp
HEADERS += $$PWD/ProtocolSchema.h $$PWD/SqzGlobal.h
```

### 7.3 初始化流程

```cpp
#include "ProtocolSchema.h"
using namespace Sqz;

// 方式A：编程式构建
ProtocolSchema schema;
schema.addField("type", 0, 0, 8)
      .addField("value", 1, 0, 16, Int, LittleEndian, Physical, true, 0.1, -40.0);

// 方式B：JSON 文件加载（推荐，协议与代码分离）
ProtocolSchema schema;
QString err;
if (!schema.loadFile("protocol.json", &err)) {
    qWarning() << "协议加载失败:" << err;
}
```

### 7.4 基本功能使用

```cpp
// 解析
QJsonObject j = schema.parse(rxData, &err);

// 打包
QByteArray tx;
if (!schema.pack(j, tx, &err)) {
    qWarning() << "打包失败:" << err;
}
```

### 7.5 高级功能使用

**条件分支 + 枚举 + 变长（链式）**：

```cpp
ProtocolSchema s;
s.addField("type", 0, 0, 8).map(1,"数据帧").map(2,"命令帧");
s.when("type", 2)
    .addField("len", 1, 0, 16)
    .addVariableField("payload", 3, 0, "len", String)
    .endBranch();
```

**字段重叠检测**：

```cpp
QStringList overlaps = s.checkOverlap();
if (!overlaps.isEmpty()) qDebug() << "字段重叠:" << overlaps;
```

### 7.6 常见操作场景

| 场景 | 关键调用 |
|---|---|
| 读取一个温度值 | `addField("temp",0,0,16,Int,...,0.1,-40)` → `parse` → `toDouble` |
| 处理变长负载 | `addField("len")` + `addVariableField("payload",...,"len")` |
| 区分多种帧 | `addField("type")` + `when("type",N)` 多分支 |
| 数值显示中文 | `addField("status").map(0,"关机").map(1,"开机")` |
| 换协议不改代码 | `loadFile("xxx.json")` |

---

## 8. 注意事项

### 8.1 使用限制与约束

- **位宽上限**：单字段最大 64 bit（`MAX_BIT_WIDTH`）。
- **变长上限**：变长字段最大 1MB（`MAX_VAR_BYTE_SIZE = 1024×1024`）。
- **枚举与线性互斥**：启用 `map` 后 `factor`/`offset` 自动失效，二选一。
- **枚举仅限整数字段**：`map` 只对 `Int`/`UInt` 类型有效，其它类型报错。
- **lenField 向前引用**：JSON 加载时长度字段必须**先于**变长字段定义；故多字段互引环无法经 `loadJson` 构造（自引用会被循环检测捕获）。
- **默认分支**：`isDefaultBranch:true` 必须搭配非空 `defaultBranchField`。
- **QJsonObject 值类型**：枚举映射字段解析后 value **必为字符串**（含未命中映射的数值字符串如 `"3"`）。
- **条件比较基准**：所有条件基于**原始数值**，JSON 中 conditions 的 value 应写原始数字。

### 8.2 性能优化建议

- **字段排序缓存**：`pack` 内部缓存排序结果（`m_sortedFieldsCache`），字段未变化时免重排。增删字段后自动失效。**避免在 parse/pack 高频路径上反复 addField**。
- **线程安全**：所有公开接口加 `QMutex`（递归锁），可多线程并发读 parse，但写（addField/loadJson）会阻塞。
- **大数据**：变长字段读取有边界检查防越界；超 1MB 报错。处理大帧时建议先校验总长度。
- **loadFile 失败回滚**：失败时 schema 清空，不会留半装载状态，可安全重试。

### 8.3 安全注意事项

- **输入校验**：parse 对越界、空数据、溢出均有检查，失败字段返回 null，不会越界读内存。
- **pack 失败清空**：打包失败时 `out.clear()`，调用方拿到的是干净空数据，不会误用半成品。
- **整数溢出防护**：比特偏移用 `qint64` 计算，转 `int` 前检查 `numeric_limits<int>::max()`。
- **文件加载**：`loadFile` 解析失败有详细错误（含 JSON 解析偏移），任意字段非法即整体回滚。
- **勿信任外部 JSON**：若 protocol.json 来自不可信来源，加载前建议做白名单校验（字段名、位宽合理性）。

### 8.4 版本兼容性说明

- 兼容 **Qt 5.12+**（使用 `QJsonObject` initializer_list 构造，Qt 5.4 起支持）。
- C++ 标准：**C++14**（使用 `std::numeric_limits`、lambda、`auto`）。
- 命名空间 `Sqz`，别名 `Sqz::PtlSc` 可简写。
- 枚举映射函数统一为 `map`（链式），支持编程式与 ConditionalBuilder 内调用。
- JSON 配置的 `defaults` 继承与 `enumMaps` 命名引用为稳定特性，向后兼容。

---

## 附录：错误信息索引

| 错误信息 | 触发场景 | 处理建议 |
|---|---|---|
| `Field name cannot be empty` | addField 名字为空 | 填写字段名 |
| `Duplicate field name: X` | 重名 | 改名 |
| `Field X bitLength must 1~64` | 位宽越界 | 调整 bitLength |
| `map() called before any addField` | map 前未加字段 | 先 addField 再 map |
| `map() only valid for Int/UInt field` | map 作用于非整数字段 | 改字段类型或去掉 map |
| `Cannot open file: X` | loadFile 文件不存在/不可读 | 检查路径 |
| `JSON parse error at offset N` | JSON 语法错误 | 修正 JSON |
| `field 'X' missing 'bitLength'` | 固定字段缺 bitLength | 补 bitLength 或加 lenField |
| `field 'X' lenField 'Y' not defined` | 长度字段未定义/定义在后 | 把长度字段放前面 |
| `field 'X' enumMap 'Y' not found in enumMaps` | 命名引用不存在 | 检查 enumMaps 拼写 |
| `isDefaultBranch=true requires non-empty defaultBranchField` | 默认分支缺字段 | 补 defaultBranchField |
| `Var field cycle dependency: X` | 变长字段循环依赖 | 去除自引用/环 |
| `validateSchema failed: ...` | 配置校验失败 | 按提示修正 |
| `Read out of data bit bounds` | parse 数据不足 | 检查数据长度 |
| `Missing value for field 'X'` | pack 缺字段值 | 补全 JSON |
| `Variable length X exceeds max limit` | 变长超 1MB | 检查长度字段值 |

---

*文档结束 · 如有疑问请对照 `test/main_json.cpp`（55 用例）与 `test/main.cpp`（37 用例）的完整示例。*
