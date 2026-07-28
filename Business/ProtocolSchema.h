/*****************************************************************************
 * 类名: ProtocolSchema
 * 功能: 二进制协议编解码器，支持比特级精确解析和打包
 *
 * 特性:
 *   - 支持位域（任意比特偏移和长度，1~64位）
 *   - 支持两种比特顺序：MsbFirst（字节内高位在前）和 Physical（物理位索引，bit0=最低位）
 *   - 只有长度 >8 位时才应用字节序（Endian），单字节内忽略 Endian
 *   - 支持变长字段（长度由另一个字段决定）
 *   - 支持线性变换：实际值 = 原始值 * factor + offset（系数和偏移）
 *   - 支持多种数据类型：有/无符号整数、十六进制字符串、Base64、原始字节数组、UTF-8字符串
 *   - 解析结果输出为 QJsonObject，打包输入也为 QJsonObject
 *   - 高效实现：位操作使用掩码和移位
 *   - 错误处理：每个函数返回 bool 并输出详细错误信息
 *   - 字段重叠检测（可选）
 *   - 枚举映射：支持数字↔文本自动转换，便于阅读（新增）
 *   优化加固项：参数强校验、64位符号扩展修复、编解码对称、内存防护、线程互斥、字段排序写入、循环依赖检测、溢出防护、跨平台移位安全
 *
 * 使用方式:
 *   // 1. 定义协议格式（默认小端）
 *   ProtocolSchema schema;
 *   schema.addField("temperature", 0, 0, 16, ProtocolSchema::Int)   // 16位有符号，系数1，偏移0
 *         .addField("low_nibble", 0, 0, 4, ProtocolSchema::UInt, ProtocolSchema::LittleEndian,
 *                   ProtocolSchema::Physical);                     // 取字节0的低4位
 *   // 2. 枚举映射（可选）
 *   schema.map("mode", {{0,"停止"},{1,"启动"},{2,"故障"}});        // 数字转文本
 *   schema.rmap("mode");                                          // 启用文本转数字（打包）
 *   // 3. 解析
 *   QJsonObject json = schema.parse(rxData);
 *   QString mode = json["mode"].toString();  // "启动" 而不是 1
 *   // 4. 打包（支持文本值）
 *   QJsonObject tx;
 *   tx["mode"] = "启动";  // 自动转为 1
 *   QByteArray packed = schema.packToArray(tx);
 *****************************************************************************/
#ifndef PROTOCOLSCHEMA_H
#define PROTOCOLSCHEMA_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QPair>
#include <QHash>
#include <QSet>
#include <QMap>
#include "SqzGlobal.h"

namespace Sqz {

class SQZ_FRAMEWORK_API ProtocolSchema
{
public:
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
    };

    // 全局常量配置
    static constexpr int MAX_BIT_WIDTH = 64;
    static constexpr int MAX_VAR_BYTE_SIZE = 1024 * 1024;
    static constexpr double FLOAT_EPS = 1e-9;

    ProtocolSchema();
    ~ProtocolSchema();

    // ---------- 原有接口（完全不变）----------

    // 添加固定长度字段（链式调用）
    ProtocolSchema& addField(const QString& name, int startByte, int startBit, int bitLength,
                             ValueType type = UInt, Endian endian = LittleEndian,
                             BitOrder bitOrder = Physical, bool isSigned = false,
                             double factor = 1.0, double offset = 0.0,
                             QString* err = nullptr);

    // 添加变长字段（长度由另一个整数字段的值决定，单位：字节）
    ProtocolSchema& addVariableField(const QString& name, int startByte, int startBit,
                                     const QString& lenField, ValueType type = RawBytes,
                                     Endian endian = LittleEndian, BitOrder bitOrder = MsbFirst,
                                     double factor = 1.0, double offset = 0.0,
                                     QString* err = nullptr);

    // 清空所有字段定义
    void clear();

    // 检测字段定义是否有重叠
    QStringList checkOverlap(const QJsonObject& runtimeVarData = {}, QString* err = nullptr) const;

    // 解析二进制数据为JSON对象
    QJsonObject parse(const QByteArray& data, QString* errorMsg = nullptr) const;

    // 打包JSON对象为二进制数据
    bool pack(const QJsonObject& values, QByteArray& out, QString* errorMsg = nullptr) const;

    // 打包的便捷版本，直接返回字节数组（失败返回空）
    QByteArray packToArray(const QJsonObject& values, QString* errorMsg = nullptr) const;

    // 校验字段配置合法性
    bool validateSchema(QString* errMsg = nullptr) const;

    // ---------- 新增：枚举映射接口（简洁版）----------

    /**
     * @brief 为字段添加枚举映射（数字→文本）
     * @param field 字段名
     * @param mapping 映射表 {数值: 文本}
     * @return 自身引用（链式调用）
     *
     * 示例: schema.map("mode", {{0,"停止"},{1,"启动"},{2,"故障"}});
     *       解析后 JSON 中 mode 显示为 "启动" 而不是 1
     */
    ProtocolSchema& map(const QString& field, const QMap<int, QString>& mapping);
    ProtocolSchema& map(const QString& field, std::initializer_list<std::pair<int, QString>> mapping);

    /**
     * @brief 启用字段的反向映射（文本→数字），用于打包
     * @param field 字段名
     * @return 自身引用（链式调用）
     *
     * 示例: schema.map("mode", {...}).rmap("mode");
     *       打包时 JSON 中 mode 可写 "启动"，自动转为 1
     */
    ProtocolSchema& rmap(const QString& field);

    /**
     * @brief 批量启用反向映射
     * @param fields 字段名列表
     * @return 自身引用（链式调用）
     */
    ProtocolSchema& rmap(const QStringList& fields);

    /**
     * @brief 获取字段的枚举映射（只读）
     */
    QMap<int, QString> getMap(const QString& field) const;

    /**
     * @brief 清除所有映射
     */
    void clearMaps();

private:
    mutable QMutex m_mutex{QMutex::Recursive};
    QVector<Field> m_fields;    // 存储所有字段定义

    // ---------- 新增：枚举映射相关 ----------
    QHash<QString, QMap<int, QString>> m_enumMaps;   // 字段→映射表
    QSet<QString> m_reverseFields;                   // 启用反向映射的字段

    // 内部映射处理函数
    QJsonObject applyMaps(const QJsonObject& input) const;
    QJsonObject reverseMaps(const QJsonObject& input, QString* errorMsg = nullptr) const;

    // ---------- 原有私有函数（完全不变）----------

    // 读取最多64位整数（任意比特偏移）
    bool readBits(const QByteArray& data, int bitOffset, int bitLength, quint64& out,
                  Endian endian, BitOrder bitOrder, QString* err = nullptr) const;

    // 写入最多64位整数（任意比特偏移）
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

    // 获取字段的绝对比特偏移
    static inline qint64 absoluteBitOffset(const Field& field) {
        return static_cast<qint64>(field.startByte) * 8 + field.startBit;
    }

    // 检测变长字段循环依赖
    bool hasVarCycleDependency(QStringList& cycleList, QString* err = nullptr) const;

    // 按绝对bit偏移升序排序字段（打包防止覆盖）
    QVector<Field> getSortedFields() const;

    // 内部解析（不应用映射）
    QJsonObject parseInternal(const QByteArray& data, QString* errorMsg = nullptr) const;
};

} // namespace Sqz

#endif // PROTOCOLSCHEMA_H
