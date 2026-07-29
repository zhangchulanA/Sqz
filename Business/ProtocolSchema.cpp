#include "ProtocolSchema.h"
#include <QtEndian>
#include <QDebug>
#include <QJsonArray>
#include <limits>
#include <cmath>
#include <algorithm>
namespace Sqz {
// ==================== 辅助静态函数 ====================
static inline quint64 maskBits(int bitLength) {
    if (bitLength <= 0 || bitLength > ProtocolSchema::MAX_BIT_WIDTH)
        return 0;
    return (bitLength == 64) ? ~0ULL : (1ULL << bitLength) - 1;
}

// 将整数分解为大端比特排列的字节数组
static QByteArray decomposeToBigEndianBits(quint64 value, int bitLength) {
    int byteLen = (bitLength + 7) / 8;
    QByteArray bytes(byteLen, 0);
    value &= maskBits(bitLength);
    value <<= (byteLen * 8 - bitLength);
    for (int i = 0; i < byteLen; ++i) {
        bytes[i] = static_cast<char>((value >> ((byteLen - 1 - i) * 8)) & 0xFF);
    }
    return bytes;
}
// ==================== ProtocolSchema 类实现 ====================
ProtocolSchema::ProtocolSchema()
{
}
ProtocolSchema::~ProtocolSchema()
{
}
ProtocolSchema& ProtocolSchema::addField(const QString& name, int startByte, int startBit,
                                         int bitLength, ValueType type, Endian endian,
                                         BitOrder bitOrder, bool isSigned,
                                         double factor, double offset, QString* err) {
    QMutexLocker lock(&m_mutex);
    // 参数合法性校验
    if (name.trimmed().isEmpty()) {
        if (err) *err = "Field name cannot be empty";
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Field %1 startBit must 0~7").arg(name);
        return *this;
    }
    if (bitLength < 1 || bitLength > MAX_BIT_WIDTH) {
        if (err) *err = QString("Field %1 bitLength must 1~64").arg(name);
        return *this;
    }
    // 同名字段重复检测
    for (const auto& f : m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate field name: %1").arg(name);
            return *this;
        }
    }
    Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = bitLength;
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = isSigned;
    f.factor = factor;
    f.offset = offset;
    f.lenField = "";
    m_fields.append(f);
    return *this;
}
ProtocolSchema& ProtocolSchema::addVariableField(const QString& name, int startByte, int startBit,
                                                 const QString& lenField, ValueType type,
                                                 Endian endian, BitOrder bitOrder,
                                                 double factor, double offset, QString* err) {
    QMutexLocker lock(&m_mutex);
    if (name.trimmed().isEmpty()) {
        if (err) *err = "Variable field name cannot be empty";
        return *this;
    }
    if (lenField.trimmed().isEmpty()) {
        if (err) *err = QString("Var field %1 lenField empty").arg(name);
        return *this;
    }
    if (startByte < 0) {
        if (err) *err = QString("Var field %1 startByte negative").arg(name);
        return *this;
    }
    if (startBit < 0 || startBit > 7) {
        if (err) *err = QString("Var field %1 startBit must 0~7").arg(name);
        return *this;
    }
    // 同名字段重复检测
    for (const auto& f : m_fields) {
        if (f.name == name) {
            if (err) *err = QString("Duplicate var field name: %1").arg(name);
            return *this;
        }
    }
    Field f;
    f.name = name;
    f.startByte = startByte;
    f.startBit = startBit;
    f.bitLength = 0;
    f.type = type;
    f.endian = endian;
    f.bitOrder = bitOrder;
    f.isSigned = false;
    f.lenField = lenField;
    f.factor = factor;
    f.offset = offset;
    m_fields.append(f);
    return *this;
}
void ProtocolSchema::clear() {
    QMutexLocker lock(&m_mutex);
    m_fields.clear();
}
bool ProtocolSchema::validateSchema(QString* errMsg) const
{
    QMutexLocker lock(&m_mutex);
    QStringList cycle;
    if (hasVarCycleDependency(cycle, errMsg))
        return false;
    return true;
}
bool ProtocolSchema::hasVarCycleDependency(QStringList& cycleList, QString* err) const
{
    QHash<QString, QString> varMap;
    for (const auto& f : m_fields) {
        if (f.bitLength == 0)
            varMap[f.name] = f.lenField;
    }
    QSet<QString> visited;
    for (const auto& var : varMap.keys()) {
        QString cur = var;
        QStringList path;
        while (varMap.contains(cur)) {
            if (visited.contains(cur)) {
                if (path.contains(cur)) {
                    auto idx = path.indexOf(cur);
                    cycleList = path.mid(idx);
                    if (err) *err = QString("Var field cycle dependency: %1").arg(cycleList.join("->"));
                    return true;
                }
                break;
            }
            visited.insert(cur);
            path << cur;
            cur = varMap[cur];
        }
    }
    return false;
}
QVector<ProtocolSchema::Field> ProtocolSchema::getSortedFields() const
{
    QMutexLocker lock(&m_mutex);
    QVector<Field> copy = m_fields;
    std::sort(copy.begin(), copy.end(), [](const Field& a, const Field& b) {
        return absoluteBitOffset(a) < absoluteBitOffset(b);
    });
    return copy;
}
QStringList ProtocolSchema::checkOverlap(const QJsonObject& runtimeVarData, QString* err) const {
    QMutexLocker lock(&m_mutex);
    QStringList overlaps;
    QVector<QPair<qint64, qint64>> fieldRanges;
    for (const auto& f : m_fields) {
        qint64 start = absoluteBitOffset(f);
        qint64 end;
        if (f.bitLength > 0) {
            end = start + f.bitLength;
        } else {
            // 动态变长字段计算运行时bit范围
            if (!runtimeVarData.contains(f.lenField)) {
                if (err) *err = QString("Overlap check missing len field %1 for %2").arg(f.lenField, f.name);
                continue;
            }
            bool ok;
            qint64 lenByte = runtimeVarData[f.lenField].toVariant().toLongLong(&ok);
            if (!ok || lenByte < 0 || lenByte > MAX_VAR_BYTE_SIZE)
                continue;
            end = start + lenByte * 8;
        }
        fieldRanges.append({start, end});
    }
    for (int i = 0; i < fieldRanges.size(); ++i) {
        qint64 aS = fieldRanges[i].first;
        qint64 aE = fieldRanges[i].second;
        QString aName = m_fields[i].name;
        for (int j = i + 1; j < fieldRanges.size(); ++j) {
            qint64 bS = fieldRanges[j].first;
            qint64 bE = fieldRanges[j].second;
            QString bName = m_fields[j].name;
            if (aE > bS && bE > aS) {
                overlaps << aName + " and " + bName;
            }
        }
    }
    return overlaps;
}
// ---------- 核心：按 Physical 或 MsbFirst 读取比特 ----------
bool ProtocolSchema::readBits(const QByteArray& data, int bitOffset, int bitLength,
                              quint64& out, Endian endian, BitOrder bitOrder, QString* err) const {
    out = 0;
    if (bitOffset < 0) {
        if (err) *err = "bitOffset negative";
        return false;
    }
    if (bitLength <= 0 || bitLength > MAX_BIT_WIDTH) {
        if (err) *err = QString("bitLength out of limit 1~64, got %1").arg(bitLength);
        return false;
    }
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (err) *err = "Read out of data bit bounds";
        return false;
    }
    if (data.isEmpty()) {
        if (err) *err = "Empty input data buffer";
        return false;
    }
    // 对于 Physical 模式：直接按物理比特索引读取，bit0 为最低位
    if (bitOrder == Physical) {
        quint64 value = 0;
        for (int i = 0; i < bitLength; ++i) {
            qint64 bitIdx = static_cast<qint64>(bitOffset) + i;
            int byteIdx = static_cast<int>(bitIdx / 8);
            int bitInByte = static_cast<int>(bitIdx % 8);
            quint8 byte = static_cast<quint8>(data.at(byteIdx));
            quint8 bitVal = (byte >> bitInByte) & 0x01;
            if (bitVal)
                value |= (1ULL << i);   // 第 i 个读到的比特成为整数的第 i 位
        }
        // 对于 Physical 模式，字节序仅当 bitLength > 8 时影响多字节整数的字节顺序
        // 但 Physical 模式已经按物理小端顺序组装了 value（低位在低地址），如果用户要求大端，需要交换字节
        if (bitLength > 8 && endian == BigEndian) {
            int byteCount = (bitLength + 7) / 8;
            quint64 swapped = 0;
            for (int i = 0; i < byteCount; ++i) {
                swapped |= ((value >> (i * 8)) & 0xFF) << ((byteCount - 1 - i) * 8);
            }
            value = swapped;
        }
        out = value & maskBits(bitLength);
        return true;
    }
    // ========== MsbFirst 模式 ==========
    int startByte = bitOffset / 8;
    int startBitInByte = bitOffset % 8;
    int endByte = (bitOffset + bitLength - 1) / 8;
    int byteSpan = endByte - startByte + 1;
    QByteArray bytes = data.mid(startByte, byteSpan);
    // MsbFirst 下，字节内比特顺序就是自然顺序，无需反转
    // 但需要将提取的比特段按大端方式组合（先读到的为高位）
    quint64 value = 0;
    int bitsLeft = bitLength;
    int byteIdx = 0;
    int bitPos = startBitInByte; // 当前字节内起始位（0 = 最高位）
    while (bitsLeft > 0 && byteIdx < bytes.size()) {
        quint8 byte = static_cast<quint8>(bytes[byteIdx]);
        int bitsFromThisByte = qMin(8 - bitPos, bitsLeft);
        int shift = 8 - bitPos - bitsFromThisByte;
        quint8 segment = (byte >> shift) & ((1 << bitsFromThisByte) - 1);
        value = (value << bitsFromThisByte) | segment;
        bitsLeft -= bitsFromThisByte;
        bitPos = 0;
        ++byteIdx;
    }
    // 字节序转换（仅当 bitLength > 8 且需要小端时）
    if (bitLength > 8 && endian == LittleEndian) {
        int byteCount = (bitLength + 7) / 8;
        quint64 swapped = 0;
        for (int i = 0; i < byteCount; ++i) {
            swapped |= ((value >> (i * 8)) & 0xFF) << ((byteCount - 1 - i) * 8);
        }
        value = swapped;
    }
    out = value & maskBits(bitLength);
    return true;
}
// ---------- 写入比特（对称于 readBits） ----------
bool ProtocolSchema::writeBits(QByteArray& data, int bitOffset, int bitLength, quint64 value,
                               Endian endian, BitOrder bitOrder, QString* errorMsg) const {
    if (bitOffset < 0) {
        if (errorMsg) *errorMsg = "bitOffset negative";
        return false;
    }
    if (bitLength <= 0 || bitLength > MAX_BIT_WIDTH) {
        if (errorMsg) *errorMsg = QString("bitLength out of limit 1~64, got %1").arg(bitLength);
        return false;
    }
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (errorMsg) *errorMsg = "Write out of bounds";
        return false;
    }
    if (data.isNull()) {
        if (errorMsg) *errorMsg = "Target data buffer null";
        return false;
    }
    value &= maskBits(bitLength);
    // Physical 模式
    if (bitOrder == Physical) {
        // 如果要求大端字节序且长度>8，先转换 value 为小端内部表示（与read对称逆运算）
        quint64 toWrite = value;
        if (bitLength > 8 && endian == BigEndian) {
            int byteCount = (bitLength + 7) / 8;
            quint64 swapped = 0;
            for (int i = 0; i < byteCount; ++i) {
                swapped |= ((value >> ((byteCount - 1 - i) * 8)) & 0xFF) << (i * 8);
            }
            toWrite = swapped;
        }
        // 按物理比特索引写入
        for (int i = 0; i < bitLength; ++i) {
            qint64 bitIdx = static_cast<qint64>(bitOffset) + i;
            int byteIdx = static_cast<int>(bitIdx / 8);
            int bitInByte = static_cast<int>(bitIdx % 8);
            quint8 bitVal = (toWrite >> i) & 0x01;
            uchar* targetByte = reinterpret_cast<uchar*>(data.data() + byteIdx);
            if (bitVal)
                *targetByte |= (1 << bitInByte);
            else
                *targetByte &= ~(1 << bitInByte);
        }
        return true;
    }
    // ========== MsbFirst 模式 ==========
    // 先处理字节序（仅当长度>8时）
    quint64 internalValue = value;
    if (bitLength > 8 && endian == LittleEndian) {
        int byteCount = (bitLength + 7) / 8;
        quint64 swapped = 0;
        for (int i = 0; i < byteCount; ++i) {
            swapped |= ((internalValue >> ((byteCount - 1 - i) * 8)) & 0xFF) << (i * 8);
        }
        internalValue = swapped;
    }
    // 分解为大端比特排列的字节数组
    QByteArray bitsBytes = decomposeToBigEndianBits(internalValue, bitLength);
    int bitsWritten = 0;
    int srcByteIdx = 0;
    int srcBitPos = 0; // 在源字节内的比特位置（0 = 最高位）
    while (bitsWritten < bitLength && srcByteIdx < bitsBytes.size()) {
        quint8 srcByte = static_cast<quint8>(bitsBytes[srcByteIdx]);
        int bitsRemainingInSrc = 8 - srcBitPos;
        qint64 targetBitIdx = static_cast<qint64>(bitOffset) + bitsWritten;
        int targetByteIdx = static_cast<int>(targetBitIdx / 8);
        int targetBitPos = static_cast<int>(targetBitIdx % 8);
        int bitsToWrite = qMin(bitsRemainingInSrc, bitLength - bitsWritten);
        quint8 srcSegment = (srcByte >> (8 - srcBitPos - bitsToWrite)) & ((1 << bitsToWrite) - 1);
        uchar* targetByte = reinterpret_cast<uchar*>(data.data() + targetByteIdx);
        // 修复 bitsToWrite=8 移位溢出问题
        quint8 clearMask;
        if (bitsToWrite == 8) {
            clearMask = 0x00;
        } else {
            clearMask = ~(((1 << bitsToWrite) - 1) << (8 - targetBitPos - bitsToWrite));
        }
        *targetByte &= clearMask;
        *targetByte |= (srcSegment << (8 - targetBitPos - bitsToWrite));
        bitsWritten += bitsToWrite;
        srcBitPos += bitsToWrite;
        if (srcBitPos >= 8) {
            srcBitPos = 0;
            ++srcByteIdx;
        }
    }
    return true;
}
// ---------- 字节数组读取（复用 readBits） ----------
bool ProtocolSchema::readBitsToBytes(const QByteArray& data, int bitOffset, int bitLength,
                                     QByteArray& out, Endian endian, BitOrder bitOrder, QString* err) const {
    out.clear();
    if (bitLength <= 0 || bitOffset < 0) {
        if (err) *err = "Invalid bit offset/length for bytes read";
        return false;
    }
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (err) *err = "Bytes read out of data bounds";
        return false;
    }
    int byteLen = (bitLength + 7) / 8;
    out.resize(byteLen);
    out.fill(0);
    for (int i = 0; i < byteLen; ++i) {
        int bitsThisByte = qMin(8, bitLength - i * 8);
        quint64 byteVal = 0;
        QString subErr;
        if (!readBits(data, bitOffset + i * 8, bitsThisByte, byteVal, endian, bitOrder, &subErr)) {
            if (err) *err = subErr;
            out.clear();
            return false;
        }
        out[i] = static_cast<char>(byteVal & 0xFF);
    }
    return true;
}
// ---------- 字节数组写入 ----------
bool ProtocolSchema::writeBitsToBytes(QByteArray& data, int bitOffset, const QByteArray& bytes,
                                      Endian endian, BitOrder bitOrder, QString* errorMsg) const {
    if (bitOffset < 0) return true;
    if (bytes.isEmpty()) return true;
    qint64 totalBits = static_cast<qint64>(data.size()) * 8;
    int bitLength = bytes.size() * 8;
    qint64 targetEndBit = static_cast<qint64>(bitOffset) + bitLength;
    if (targetEndBit > totalBits) {
        if (errorMsg) *errorMsg = "WriteBitsToBytes out of bounds";
        return false;
    }
    // 逐个字节写入
    QByteArray toWrite = bytes;
    for (int i = 0; i < toWrite.size(); ++i) {
        quint8 byteVal = static_cast<quint8>(toWrite[i]);
        QString subErr;
        if (!writeBits(data, bitOffset + i * 8, 8, byteVal, endian, bitOrder, &subErr)) {
            if (errorMsg) *errorMsg = subErr;
            return false;
        }
    }
    return true;
}
// ---------- JSON 值转换 ----------
QByteArray ProtocolSchema::encodeValue(const QJsonValue& value, ValueType type, int fixedBytes,
                                       QString* errorMsg) const {
    QByteArray result;
    switch (type) {
    case HexString: {
        QString hex = value.toString().trimmed();
        // 检测奇数长度十六进制
        if ((hex.length() % 2) != 0) {
            if (errorMsg) *errorMsg = QString("Hex string length must even, got %1").arg(hex.length());
            return QByteArray();
        }
        result = QByteArray::fromHex(hex.toLatin1());
        if (result.isEmpty() && !hex.isEmpty()) {
            if (errorMsg) *errorMsg = "Invalid hex string";
            return QByteArray();
        }
        break;
    }
    case Base64:
    case RawBytes: {
        QString b64 = value.toString().trimmed();
        result = QByteArray::fromBase64(b64.toLatin1());
        if (result.isEmpty() && !b64.isEmpty()) {
            if (errorMsg) *errorMsg = "Invalid base64 string";
            return QByteArray();
        }
        break;
    }
    case String: {
        QString str = value.toString();
        result = str.toUtf8();
        break;
    }
    default:
        if (errorMsg) *errorMsg = "encodeValue called on non-bytes type";
        return QByteArray();
    }
    if (fixedBytes > 0) {
        if (result.size() < fixedBytes) {
            result.append(fixedBytes - result.size(), '\0');
        } else if (result.size() > fixedBytes) {
            if (errorMsg) *errorMsg = "Value too long for fixed-length field";
            return QByteArray();
        }
    }
    return result;
}
bool ProtocolSchema::valueToInteger(const QJsonValue& value, int bitLength, bool isSigned,
                                    double factor, double offset,
                                    quint64& out, QString* errorMsg) const {
    out = 0;
    if (!value.isDouble()) {
        if (errorMsg) *errorMsg = "Value is not a number";
        return false;
    }
    double physicalValue = value.toDouble();
    if (qAbs(factor) < FLOAT_EPS) {
        if (errorMsg) *errorMsg = "Factor is zero, cannot invert";
        return false;
    }
    double rawDouble = (physicalValue - offset) / factor;
    // 四舍五入修复截断精度丢失
    qint64 signedRaw = static_cast<qint64>(std::round(rawDouble));
    if (isSigned) {
        qint64 minVal;
        qint64 maxVal;
        if (bitLength == 64) {
            minVal = std::numeric_limits<qint64>::min();
            maxVal = std::numeric_limits<qint64>::max();
        } else {
            minVal = -(1ULL << (bitLength - 1));
            maxVal = static_cast<qint64>((1ULL << (bitLength - 1)) - 1);
        }
        if (signedRaw < minVal || signedRaw > maxVal) {
            if (errorMsg) *errorMsg = QString("Inverse transformed value %1 out of signed range [%2,%3]")
                                          .arg(signedRaw).arg(minVal).arg(maxVal);
            return false;
        }
        out = static_cast<quint64>(signedRaw) & maskBits(bitLength);
    } else {
        quint64 max = maskBits(bitLength);
        if (signedRaw < 0 || static_cast<quint64>(signedRaw) > max) {
            if (errorMsg) *errorMsg = QString("Inverse transformed value %1 out of unsigned range [0,%2]")
                                          .arg(signedRaw).arg(max);
            return false;
        }
        out = static_cast<quint64>(signedRaw);
    }
    return true;
}
double ProtocolSchema::applyLinearTransform(quint64 rawValue, double factor, double offset,
                                            bool isSigned, int bitLength) {
    qint64 signedRaw = static_cast<qint64>(rawValue);
    // 修复64位有符号整数符号扩展缺失问题
    if (isSigned) {
        if (bitLength == 64) {
            signedRaw = static_cast<qint64>(rawValue);
        } else if ((rawValue >> (bitLength - 1)) & 0x01) {
            signedRaw |= (~0ULL << bitLength);
        }
    }
    double value = static_cast<double>(signedRaw);
    return value * factor + offset;
}
// ---------- 公有解析和打包 ----------
QJsonObject ProtocolSchema::parse(const QByteArray& data, QString* errorMsg) const {
    QMutexLocker lock(&m_mutex);
    QJsonObject result;
    QString errBuf;
    // 先解析固定长度字段
    for (const Field& f : m_fields) {
        if (f.bitLength > 0) {
            qint64 bitOffset64 = absoluteBitOffset(f);
            if (bitOffset64 > std::numeric_limits<int>::max()) {
                errBuf = QString("Field %1 bit offset overflow int").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            int bitOffset = static_cast<int>(bitOffset64);
            if (f.type == Int || f.type == UInt) {
                quint64 raw;
                QString subErr;
                if (!readBits(data, bitOffset, f.bitLength, raw, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Failed to read field '%1': %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    result.insert(f.name, QJsonValue::Null);
                    continue;
                }
                double finalVal = applyLinearTransform(raw, f.factor, f.offset, f.isSigned, f.bitLength);
                result.insert(f.name, finalVal);
            } else {
                QByteArray bytes;
                QString subErr;
                if (!readBitsToBytes(data, bitOffset, f.bitLength, bytes, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Failed to read field '%1': %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    result.insert(f.name, QJsonValue::Null);
                    continue;
                }
                switch (f.type) {
                case HexString: result.insert(f.name, QString::fromLatin1(bytes.toHex())); break;
                case Base64:    result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
                case RawBytes:  result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
                case String:    result.insert(f.name, QString::fromUtf8(bytes)); break;
                default:        result.insert(f.name, QJsonValue::Null);
                }
            }
        }
    }
    // 再解析变长字段
    for (const Field& f : m_fields) {
        if (f.bitLength == 0) {
            if (!result.contains(f.lenField)) {
                errBuf = QString("Length field '%1' missing for '%2'").arg(f.lenField, f.name);
                if (errorMsg) *errorMsg = errBuf;
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            bool ok;
            qint64 lenBytes = result[f.lenField].toVariant().toLongLong(&ok);
            if (!ok || lenBytes < 0 || lenBytes > MAX_VAR_BYTE_SIZE) {
                errBuf = QString("Invalid length for field '%1'").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            qint64 bitOffset64 = absoluteBitOffset(f);
            if (bitOffset64 > std::numeric_limits<int>::max()) {
                errBuf = QString("Var field %1 bit offset overflow int").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            int bitOffset = static_cast<int>(bitOffset64);
            int bitLength = static_cast<int>(lenBytes * 8);
            qint64 totalBits = static_cast<qint64>(data.size()) * 8;
            if (bitOffset64 + bitLength > totalBits) {
                errBuf = QString("Variable field '%1' out of bounds").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            QByteArray bytes;
            QString subErr;
            if (!readBitsToBytes(data, bitOffset, bitLength, bytes, f.endian, f.bitOrder, &subErr)) {
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            switch (f.type) {
            case HexString: result.insert(f.name, QString::fromLatin1(bytes.toHex())); break;
            case Base64:    result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
            case RawBytes:  result.insert(f.name, QString::fromLatin1(bytes.toBase64())); break;
            case String:    result.insert(f.name, QString::fromUtf8(bytes)); break;
            default:        result.insert(f.name, QJsonValue::Null);
            }
        }
    }
    return result;
}
bool ProtocolSchema::pack(const QJsonObject& values, QByteArray& out, QString* errorMsg) const {
    QMutexLocker lock(&m_mutex);
    QString errBuf;
    QHash<QString, QString> lenToVar;
    QHash<QString, int> varLengths;
    QHash<QString, QByteArray> varContents;
    // 长度字段 -> 变长字段映射
    for (const Field& f : m_fields) {
        if (f.bitLength == 0) lenToVar[f.lenField] = f.name;
    }
    // 计算变长字段内容
    for (const Field& f : m_fields) {
        if (f.bitLength == 0) {
            QJsonValue val = values.value(f.name);
            if (val.isNull() || val.isUndefined()) {
                errBuf = QString("Missing value for variable field '%1'").arg(f.name);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            QString subErr;
            QByteArray content = encodeValue(val, f.type, -1, &subErr);
            if (content.isNull()) {
                errBuf = QString("Encode var field %1 failed: %2").arg(f.name, subErr);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            if (content.size() > MAX_VAR_BYTE_SIZE) {
                errBuf = QString("Var field %1 size exceed max limit %2").arg(f.name).arg(MAX_VAR_BYTE_SIZE);
                if (errorMsg) *errorMsg = errBuf;
                return false;
            }
            varContents[f.name] = content;
            varLengths[f.name] = content.size();
        }
    }
    // 计算总最大bit，防止int溢出
    qint64 maxBit = 0;
    for (const Field& f : m_fields) {
        qint64 bitStart = absoluteBitOffset(f);
        qint64 bitEnd = bitStart;
        if (f.bitLength > 0) {
            bitEnd += f.bitLength - 1;
        } else {
            QByteArray content = varContents.value(f.name);
            if (!content.isEmpty()) bitEnd += static_cast<qint64>(content.size()) * 8 - 1;
        }
        if (bitEnd > maxBit) maxBit = bitEnd;
    }
    if (maxBit < 0) {
        errBuf = "Calculated frame bit length negative";
        if (errorMsg) *errorMsg = errBuf;
        return false;
    }
    // 分配内存
    qint64 byteCount64 = (maxBit + 7) / 8;
    if (byteCount64 > std::numeric_limits<int>::max()) {
        errBuf = "Frame size overflow int limit";
        if (errorMsg) *errorMsg = errBuf;
        return false;
    }
    int byteCount = static_cast<int>(byteCount64);
    out.resize(byteCount);
    out.fill(0);
    // 按bit偏移升序排序字段写入，防止覆盖
    QVector<Field> sortedFields = getSortedFields();
    for (const Field& f : sortedFields) {
        qint64 bitOffset64 = absoluteBitOffset(f);
        if (bitOffset64 > std::numeric_limits<int>::max()) {
            errBuf = QString("Field %1 bit offset overflow int").arg(f.name);
            if (errorMsg) *errorMsg = errBuf;
            return false;
        }
        int bitOffset = static_cast<int>(bitOffset64);
        if (f.bitLength > 0) {
            if (lenToVar.contains(f.name)) {
                QString varName = lenToVar[f.name];
                int lenBytes = varLengths.value(varName, -1);
                if (lenBytes > MAX_VAR_BYTE_SIZE)
                {
                    errBuf = QString("Variable length %1 exceeds max limit %2").arg(lenBytes).arg(MAX_VAR_BYTE_SIZE);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
                if (lenBytes < 0) {
                    errBuf = QString("Length field '%1' for '%2' not computed").arg(f.name, varName);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
                quint64 intVal = static_cast<quint64>(lenBytes);
                quint64 masked = intVal & maskBits(f.bitLength);
                if (masked != intVal) {
                    errBuf = QString("Length value %1 overflow field %2 bit width").arg(lenBytes).arg(f.name);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
                QString subErr;
                if (!writeBits(out, bitOffset, f.bitLength, masked, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Write len field %1 failed: %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
            } else {
                QJsonValue val = values.value(f.name);
                if (val.isNull() || val.isUndefined()) {
                    errBuf = QString("Missing value for field '%1'").arg(f.name);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
                if (f.type == Int || f.type == UInt) {
                    quint64 intVal;
                    QString subErr;
                    if (!valueToInteger(val, f.bitLength, f.isSigned, f.factor, f.offset, intVal, &subErr)) {
                        errBuf = QString("Convert field %1 value failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                    if (!writeBits(out, bitOffset, f.bitLength, intVal, f.endian, f.bitOrder, &subErr)) {
                        errBuf = QString("Write field %1 failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                } else {
                    int fixedBytes = (f.bitLength + 7) / 8;
                    QString subErr;
                    QByteArray bytes = encodeValue(val, f.type, fixedBytes, &subErr);
                    if (bytes.isNull()) {
                        errBuf = QString("Encode field %1 failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                    if (!writeBitsToBytes(out, bitOffset, bytes, f.endian, f.bitOrder, &subErr)) {
                        errBuf = QString("Write bytes field %1 failed: %2").arg(f.name, subErr);
                        if (errorMsg) *errorMsg = errBuf;
                        return false;
                    }
                }
            }
        } else {
            QByteArray content = varContents.value(f.name);
            if (!content.isEmpty()) {
                QString subErr;
                if (!writeBitsToBytes(out, bitOffset, content, f.endian, f.bitOrder, &subErr)) {
                    errBuf = QString("Write var field %1 failed: %2").arg(f.name, subErr);
                    if (errorMsg) *errorMsg = errBuf;
                    return false;
                }
            }
        }
    }
    return true;
}
QByteArray ProtocolSchema::packToArray(const QJsonObject& values, QString* errorMsg) const {
    QByteArray result;
    if (!pack(values, result, errorMsg)) result.clear();
    return result;
}
}
