INCLUDEPATH +=  $$PWD/Log \
                $$PWD/Core \
                $$PWD/Database \
                $$PWD/Utils \
                $$PWD/Widget \
                $$PWD/Config \
                $$PWD/NetWork \
                $$PWD/Business \
                $$PWD/Global \
                $$PWD/Net \
                $$PWD/Net/Base \
                $$PWD/Net/Channel \
                $$PWD/Net/Core \
                $$PWD/Net/Request \
                $$PWD/Net/Server


## ProtocolSchema 类使用注意事项
### 一、链式配置顺序（最易错点）
链式 API 必须在 addField / addVariableField 之前调用 ，配置才能作用于该字段：

```
// ✅ 正确：endian 在 addField 前
schema.factor(0.1).offset(-40.0).endian(BigEndian).addField
("temp", 0, 0, 16);

// ❌ 错误：endian 在 addField 后，不作用于已创建字段
schema.factor(0.1).addField("temp", 0, 0, 16).endian
(BigEndian);
```
每个字段创建后 m_active 会自动重置 ，链式配置不会泄漏到下一字段。但若需显式清理可调用 finishField() 。

### 二、字节序与位序的配合
BitOrder endian 含义 典型场景 Physical (默认) byte0=低位字节 物理位索引，CAN/汽车协议常用 MsbFirst byte0=高位字节 常规网络协议

endian 仅在 bitLength > 8 时生效；单字节字段无需关心字节序。

### 三、变长字段约束
1. 长度字段必须先于变长字段定义 ，且类型为 Int / UInt
2. 长度字段值代表字节数 （非位数）
3. 变长字段间禁止循环依赖（ A.len 依赖 B ， B.len 又依赖 A ）， validateSchema() 会检测并报错
4. 单字段最大 MAX_VAR_BYTE_SIZE = 1MB ，整帧最大 MAX_FRAME_BIT = 8MB
### 四、枚举映射（map / rmap）
- 正向 map() ：解析时自动将数字转文本，并备份原始值到 字段名_raw
- 反向 rmap(true) ：打包时自动将文本转回数字
- 仅对 Int / UInt 类型字段生效，字符串/十六进制字段忽略
- 未匹配的枚举值会显示为 "未知(N)"
### 五、线程安全
- 所有公共接口内部加锁，支持多线程并发 读/写不同操作
- 使用 QMutex::Recursive ，同一线程内可重入
- 但 不建议 在持有外部锁时调用，避免与内部锁产生死锁
### 六、线性变换精度
- factor 接近 0（ < 1e-9 ）时打包会报错（除零保护）
- 解析使用 double 运算，超 48 位整数可能有精度损失
- 有符号字段 signed_(true) 时， bitLength=64 的边界值需注意符号扩展
### 七、错误处理
- 所有接口支持 QString* err 出参， 不抛异常
- 解析失败时字段值为 null ，但不会中断整体解析
- 打包失败返回 false 且 out 不保证有效内容
- 建议生产环境始终传入 err 参数并检查
### 八、内存与性能
- m_fields 使用 QVector ，字段数 >1000 时查找变慢
- parse() 每次遍历全部字段，高频调用场景建议缓存 QHash
- getSortedFields() 会复制并排序，避免在热路径频繁调用
- 大帧（接近 8MB）解析会触发 OOM 防护并拒绝
### 九、Schema 校验建议
使用前务必调用 validateSchema() ，可检测：

- 变长字段循环依赖
- 长度字段未定义或类型非法
- 条件字段引用了不存在的字段
### 十、生命周期
- clear() 清空所有字段定义但保留默认配置
- clearDefaults() 同时清空类型/分组/通配符默认
- resetDefaults() 仅重置全局默认为初始值
- 析构自动释放所有内部资源，无内存泄漏
