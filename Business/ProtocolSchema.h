/*****************************************************************************
 * 类名: ProtocolSchema
 * 功能: 二进制协议编解码器，支持比特级精确解析和打包
 *
 * 特性:
 *   - 支持位域（任意比特偏移和长度，1~64位）
 *   - 支持两种比特顺序：MsbFirst（字节内高位在前）和 Physical（物理位索引，bit0=最低位）
 *   - 只有长度 >8 位时才应用字节序（Endian），单字节内忽略 Endian
 *   - 支持变长字段（长度由另一个字段决定）
 *   - 支持条件分支（根据字段值决定后续字段解析）
 *   - 支持线性变换：实际值 = 原始值 * factor + offset（系数和偏移）
 *   - 支持枚举映射 map()：原始字节值 <-> 字符串，解析输出字符串、打包接受字符串反查
 *   - 枚举映射与线性变换互斥：启用 map() 后自动禁用 factor/offset
 *   - 支持多种数据类型：有/无符号整数、十六进制字符串、Base64、原始字节数组、UTF-8字符串
 *   - 解析结果输出为 QJsonObject，打包输入也为 QJsonObject
 *   - 高效实现：位操作使用掩码和移位
 *   - 错误处理：每个函数返回 bool 并输出详细错误信息
 *   - 字段重叠检测（可选）
 *   优化加固项：参数强校验、64位符号扩展修复、编解码对称、内存防护、线程互斥、字段排序写入、循环依赖检测、溢出防护、跨平台移位安全
 *
 * 使用方式:
 *   // 1. 定义协议格式（默认小端）
 *   ProtocolSchema schema;
 *   schema.addField("temperature", 0, 0, 16, ProtocolSchema::Int)   // 16位有符号，系数1，偏移0
 *         .addField("low_nibble", 0, 0, 4, ProtocolSchema::UInt, ProtocolSchema::LittleEndian,
 *                   ProtocolSchema::Physical);                     // 取字节0的低4位
 *   // 2. 解析
 *   QJsonObject json = schema.parse(rxData);
 *   double temp = json["temperature"].toDouble();
 *   // 3. 打包
 *   QJsonObject tx;
 *   tx["temperature"] = 25.6;
 *   QByteArray packed = schema.packToArray(tx);
 *
 *   // 4. 枚举映射示例（map 链式调用，启用后自动禁用线性变换）
 *   ProtocolSchema s;
 *   s.addField("status", 0, 0, 8, UInt)
 *    .map(0, "关机")     // 原始值 0 -> "关机"
 *    .map(1, "开机")     // 链式追加映射
 *    .map(2, "待机");    // 启用映射后 factor/offset 自动失效
 *   // 解析后 {"status": "开机"}，value 必为字符串
 *   // 打包时 tx["status"] = "待机"; 系统自动反查为 2 写入字节流
 *
 *   // 4. 条件分支示例
 *   schema.addField("type", 0, 0, 8);
 *   schema.when("type", 1)
 *       .addField("temp", 1, 0, 16, ProtocolSchema::Int)
 *       .when("type", 2)
 *       .addField("pressure", 1, 0, 32, ProtocolSchema::UInt)
 *       .otherwise()
 *       .addField("error", 1, 0, 8, ProtocolSchema::UInt)
 *       .endBranch();
 *****************************************************************************/
#ifndef PROTOCOLSCHEMA_H
#define PROTOCOLSCHEMA_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QPair>
#include <QVariant>
#include "SqzGlobal.h"

namespace Sqz {

// ==================== 全局枚举定义 ====================

// 字节序（只有多字节整数 >8 位时才有效）
enum Endian {
    LittleEndian,   // 小端：低地址存放低字节
    BigEndian       // 大端：低地址存放高字节
};

// 字节内比特顺序
enum BitOrder {
    MsbFirst,   // 高位优先：bit7 为最高位（常规网络协议），startBit=0 表示从字节高位开始
    Physical    // 物理位索引：bit0 为最低位，startBit=0 表示从字节的最右侧（bit0）开始，不反转权重
};

// 字段值类型（JSON中存储的形式）
enum ValueType {
    Int,            // 有符号整数（JSON存储为数字）
    UInt,           // 无符号整数（JSON存储为数字）
    HexString,      // 十六进制字符串（例如 "1A2B"）
    Base64,         // Base64编码的字符串
    RawBytes,       // 原始字节数组（JSON中存储为Base64）
    String          // UTF-8字符串
};

// ==================== 前向声明 ====================
class ProtocolSchema;
class ConditionalBuilder;

// ==================== ProtocolSchema 类 ====================
class SQZ_FRAMEWORK_API ProtocolSchema
{
    friend class ConditionalBuilder;  // 允许 ConditionalBuilder 访问私有成员

public:
    // 全局常量配置
    static constexpr int MAX_BIT_WIDTH = 64;
    static constexpr int MAX_VAR_BYTE_SIZE = 1024 * 1024;
    static constexpr double FLOAT_EPS = 1e-9;

    // 单个字段的定义
    struct Field {
        QString name;           // 字段名（JSON中的key）
        int startByte;          // 起始字节偏移（0-based）
        int startBit;           // 起始位（0~7，含义由 BitOrder 决定）
        int bitLength;          // 比特长度，0表示变长（需配合lenField）
        ValueType type;         // 值类型
        Endian endian;          // 字节序（仅当 bitLength > 8 时有效）
        BitOrder bitOrder;      // 比特顺序
        bool isSigned;          // 是否有符号（仅对Int有效）
        QString lenField;       // 变长字段依赖的长度字段名（bitLength==0时有效）
        double factor;          // 系数（默认1.0）
        double offset;          // 偏移（默认0.0）

        // ===== 枚举映射（字节值 <-> 字符串）=====
        // valueMap:  原始字节值 -> 字符串 的映射表
        // hasMapping: 是否启用枚举映射
        // 互斥规则:  hasMapping 为 true 时，factor/offset 线性变换自动失效（二选一）
        // 解析行为:  读取原始字节值后查表输出字符串（保证 JSON value 为字符串类型）
        //            未命中映射时输出原始数值的字符串形式（如 "3"），保持 value 类型一致
        // 打包行为:  接受字符串反向查表写入原始字节值；也接受数字直接写入
        // 条件比较:  含映射的条件字段统一按原始数值比较，保证可靠性
        QHash<quint64, QString> valueMap;   // 枚举映射表（key=原始字节值, value=字符串）
        bool hasMapping;                    // 是否启用枚举映射

        // 条件分支相关
        QList<QPair<QString, QVariant>> conditions; // 所有条件 (AND逻辑, 空列表=无条件)
        bool isDefaultBranch;   // 是否为默认分支
        QString defaultBranchField; // 默认分支对应的条件字段名
    };

    ProtocolSchema();
    ~ProtocolSchema();

    // ---------- 固定长度字段 ----------
    // name: 字段名
    // startByte: 起始字节偏移
    // startBit: 起始位（0~7，由 BitOrder 决定其物理含义）
    // bitLength: 比特长度（1~64）
    // type: 值类型，默认无符号整数
    // endian: 字节序，默认小端（仅 bitLength > 8 时有效）
    // bitOrder: 比特顺序，默认 Physical
    // isSigned: 仅对Int有效，默认false
    // factor: 系数，默认1.0
    // offset: 偏移，默认0.0
    ProtocolSchema& addField(const QString& name, int startByte, int startBit, int bitLength,
                             ValueType type = UInt, Endian endian = LittleEndian,
                             BitOrder bitOrder = Physical, bool isSigned = false,
                             double factor = 1.0, double offset = 0.0,
                             QString* err = nullptr);

    // ---------- 变长字段 ----------
    // name: 字段名
    // startByte: 起始字节偏移
    // startBit: 起始位
    // lenField: 指示长度的字段名（该字段的值表示变长字段的字节数）
    // type: 值类型，默认为原始字节数组
    // endian, bitOrder, factor, offset: 同addField
    ProtocolSchema& addVariableField(const QString& name, int startByte, int startBit,
                                     const QString& lenField, ValueType type = RawBytes,
                                     Endian endian = LittleEndian, BitOrder bitOrder = MsbFirst,
                                     double factor = 1.0, double offset = 0.0,
                                     QString* err = nullptr);

    // ---------- 枚举映射 map ----------
    // 为"最近添加的字段"追加一条 原始字节值 -> 字符串 的映射，支持链式调用
    // rawValue: 原始字节值（解析时从比特流读到的无符号整数）
    // str:      映射目标字符串（解析后写入 JSON 的 value）
    // err:      错误信息输出（可选）
    // 返回 *this 以支持链式：schema.addField(...).map(0,"A").map(1,"B")
    // 注意事项:
    //   1. 必须先调用 addField/addVariableField 添加字段，否则报错
    //   2. 仅对 Int/UInt 类型字段有效，其它类型报错
    //   3. 一旦添加映射，自动禁用 factor/offset 线性变换（互斥机制，二选一）
    //   4. 解析后该字段在 JSON 中 value 必为字符串类型
    //   5. 打包时输入字符串会自动反查为原始值写入；输入数字也可正常处理
    //   6. 对同一原始值重复 map 会覆盖之前的字符串
    ProtocolSchema& map(quint64 rawValue, const QString& str, QString* err = nullptr);

    // ---------- 条件分支 ----------
    // 开始一个条件分支：当 fieldName 的值等于 value 时，后续字段生效
    // 必须先定义条件字段本身（通过 addField）
    ConditionalBuilder when(const QString& fieldName, const QVariant& value);

    // 默认分支：当条件不匹配任何 when 时生效
    ConditionalBuilder otherwise();

    // ---------- JSON 配置加载 ----------
    // 从 JSON 对象加载完整协议（先清空再装载），失败回滚到空 schema
    // root: 协议配置 JSON（含 protocolName/defaults/enumMaps/fields）
    bool loadJson(const QJsonObject& root, QString* err = nullptr);

    // 从 JSON 文件加载协议定义（读文件 -> 解析 -> loadJson）
    // path: JSON 文件路径
    bool loadFile(const QString& path, QString* err = nullptr);

    // 获取协议名（JSON protocolName 字段，未加载则为空）
    QString protocolName() const;

    // ---------- 清空 ----------
    void clear();

    // ---------- 验证 ----------
    // 检测字段定义是否有重叠（静态固定字段+运行时变长动态范围）
    // 返回重叠的字段名列表（空表示无重叠）
    QStringList checkOverlap(const QJsonObject& runtimeVarData = {}, QString* err = nullptr) const;

    // 校验字段配置合法性（外部可调用预校验）
    bool validateSchema(QString* errMsg = nullptr) const;

    // ---------- 解析和打包 ----------
    // 解析二进制数据为JSON对象
    // data: 原始数据
    // errorMsg: 输出错误信息（可选）
    QJsonObject parse(const QByteArray& data, QString* errorMsg = nullptr) const;

    // 打包JSON对象为二进制数据
    // values: 包含所有字段值的JSON对象
    // out: 输出打包后的字节数组
    // errorMsg: 输出错误信息（可选）
    bool pack(const QJsonObject& values, QByteArray& out, QString* errorMsg = nullptr) const;

    // 打包的便捷版本，直接返回字节数组（失败返回空）
    QByteArray packToArray(const QJsonObject& values, QString* errorMsg = nullptr) const;

private:
    mutable QMutex m_mutex{QMutex::Recursive};
    QVector<Field> m_fields;    // 存储所有字段定义

    // 字段排序缓存：避免 pack() 中每次调用 getSortedFields() 都重新排序
    // 当 m_fields 变化时通过 invalidateSortCache() 置失效
    mutable QVector<Field> m_sortedFieldsCache;
    mutable bool m_sortCacheValid{false};

    // 最近一次 when() 使用的条件字段名，供 otherwise() 直接在 ProtocolSchema 上调用时使用
    QString m_lastConditionField;

    // 协议名（来自 JSON protocolName 字段，编程式构建时为空）
    QString m_protocolName;

    // 使排序缓存失效（在字段增删时调用）
    inline void invalidateSortCache() const { m_sortCacheValid = false; }

    // ---------- 底层位操作（私有）----------
    // 读取最多64位整数（任意比特偏移）
    // 注意：当 bitLength > 8 时应用 endian，否则忽略 endian
    bool readBits(const QByteArray& data, int bitOffset, int bitLength, quint64& out,
                  Endian endian, BitOrder bitOrder, QString* err = nullptr) const;

    // 写入最多64位整数（任意比特偏移）
    // 注意：当 bitLength > 8 时应用 endian，否则忽略 endian
    bool writeBits(QByteArray& data, int bitOffset, int bitLength, quint64 value,
                   Endian endian, BitOrder bitOrder, QString* errorMsg = nullptr) const;

    // 读取任意长度的比特段为字节数组
    bool readBitsToBytes(const QByteArray& data, int bitOffset, int bitLength, QByteArray& out,
                         Endian endian, BitOrder bitOrder, QString* err = nullptr) const;

    // 写入任意长度的字节数组到比特流
    bool writeBitsToBytes(QByteArray& data, int bitOffset, const QByteArray& bytes,
                          Endian endian, BitOrder bitOrder, QString* errorMsg = nullptr) const;

    // 将JSON值转换为字节数组（根据字段类型和固定长度要求）
    QByteArray encodeValue(const QJsonValue& value, ValueType type, int fixedBytes,
                           QString* errorMsg = nullptr) const;

    // 将JSON值（实际物理值）转换为整数字段所需的原始编码值（应用逆变换）
    bool valueToInteger(const QJsonValue& value, int bitLength, bool isSigned,
                        double factor, double offset,
                        quint64& out, QString* errorMsg = nullptr) const;

    // 将原始整数值（读取后）应用系数和偏移变换为最终物理值
    static double applyLinearTransform(quint64 rawValue, double factor, double offset,
                                       bool isSigned, int bitLength);

    // ---------- 枚举映射辅助（私有）----------
    // 按字段名查找字段定义（返回指针，未找到返回 nullptr）
    // 用于条件求值时获取条件字段的映射配置
    const Field* findField(const QString& name) const;

    // 反向映射查找：根据 JSON 值查找原始字节值
    // 查找策略（保证打包可靠性）:
    //   1. 若值为字符串，先在 valueMap 中反向匹配字符串得到原始值
    //   2. 若未命中，尝试将字符串解析为数值（兼容用户传入数值字符串）
    //   3. 若值为数字，直接使用（兼容用户传入数字的情况）
    // 成功返回 true 并通过 out 输出原始字节值
    bool reverseMapLookup(const Field& f, const QJsonValue& val, quint64& out, QString* err) const;

    // 将条件值或上下文值规范化为"原始数值"用于可靠条件比较
    // field: 条件字段定义（用于判断是否含枚举映射）
    // val:   待规范化的值（可能为字符串或数字）
    // out:   输出规范化后的 QVariant
    // 规则: 含映射的字符串值会被反查为原始数值；数字和不带映射字段原样返回
    bool normalizeToRawValue(const Field& field, const QVariant& val, QVariant& out) const;

    // 比较单个条件是否满足（基于原始数值比较，保证枚举映射字段条件可靠）
    // fieldName: 条件字段名
    // condValue: 条件期望值
    // context:   已解析字段的 JSON 上下文
    bool matchCondition(const QString& fieldName, const QVariant& condValue,
                        const QJsonObject& context) const;

    // 获取字段的绝对比特偏移
    static inline qint64 absoluteBitOffset(const Field& field) {
        return static_cast<qint64>(field.startByte) * 8 + field.startBit;
    }

    // 检测变长字段循环依赖
    bool hasVarCycleDependency(QStringList& cycleList, QString* err = nullptr) const;

    // 按绝对bit偏移升序排序字段（打包防止覆盖）
    QVector<Field> getSortedFields() const;

    // 评估条件是否满足
    bool evaluateCondition(const Field& field, const QJsonObject& context) const;

    // 解析单个字段（内部使用）
    bool parseField(const Field& field, const QByteArray& data, QJsonObject& result,
                    QString* errorMsg) const;

    // 解析变长字段（内部使用）：根据长度字段读取变长数据并按类型转换
    bool parseVariableField(const Field& field, const QByteArray& data, QJsonObject& result,
                            QString* errorMsg) const;

    // 从单个字段 JSON 对象构建并追加字段（含 defaults 继承与 enumMap 解析）
    // fo:       字段 JSON 对象
    // defaults: 顶层缺省值对象（字段未指定时继承）
    // enumMaps: 顶层命名枚举库（字段 enumMap 可为字符串引用其名称）
    bool loadOneField(const QJsonObject& fo, const QJsonObject& defaults,
                      const QJsonObject& enumMaps, QString* err);
};

// ==================== ConditionalBuilder 类 ====================
// 条件分支构建器，支持链式调用
class SQZ_FRAMEWORK_API ConditionalBuilder
{
public:
    // 构造函数（由 ProtocolSchema 调用）
    ConditionalBuilder(ProtocolSchema* schema, const QString& conditionField,
                       const QVariant& conditionValue, bool isDefault = false);
    ~ConditionalBuilder();

    // ---------- 添加字段到当前分支 ----------
    // 所有参数同 ProtocolSchema::addField
    ConditionalBuilder& addField(const QString& name, int startByte, int startBit, int bitLength,
                                 ValueType type = UInt, Endian endian = LittleEndian,
                                 BitOrder bitOrder = Physical, bool isSigned = false,
                                 double factor = 1.0, double offset = 0.0,
                                 QString* err = nullptr);

    // 添加变长字段到当前分支
    ConditionalBuilder& addVariableField(const QString& name, int startByte, int startBit,
                                         const QString& lenField, ValueType type = RawBytes,
                                         Endian endian = LittleEndian, BitOrder bitOrder = MsbFirst,
                                         double factor = 1.0, double offset = 0.0,
                                         QString* err = nullptr);

    // 为"最近添加的字段"追加枚举映射（条件分支内链式调用）
    // 语义同 ProtocolSchema::map，返回 *this 支持链式
    // 注意: 启用映射后该字段 factor/offset 自动失效；条件字段仍按原始数值比较
    ConditionalBuilder& map(quint64 rawValue, const QString& str, QString* err = nullptr);

    // ---------- 条件分支嵌套 ----------
    // 在当前分支内部开启新的条件分支
    ConditionalBuilder when(const QString& fieldName, const QVariant& value);

    // 当前分支的默认分支
    ConditionalBuilder otherwise();

    // ---------- 结束分支 ----------
    // 结束当前条件分支，返回到上一级
    ProtocolSchema& endBranch();

private:
    ProtocolSchema* m_schema;           // 关联的 schema
    QString m_conditionField;           // 当前分支的条件字段
    QVariant m_conditionValue;          // 当前分支的条件值
    bool m_isDefault;                   // 是否为默认分支
    QList<QPair<QString, QVariant>> m_inheritedConditions; // 从父级继承的条件列表（AND逻辑）
};

using PtlSc = ProtocolSchema;

} // namespace Sqz

#endif // PROTOCOLSCHEMA_H
