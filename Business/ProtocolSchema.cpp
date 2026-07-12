#include "ProtocolSchema.h"
#include <QtEndian>
#include <QDebug>
#include <QJsonArray>
#include <limits>
#include <cmath>

namespace Sqz {
// ==================== 辅助静态函数 ====================
static inline quint64 maskBits(int bitLength) {
    return (bitLength == 64) ? ~0ULL : (1ULL << bitLength) - 1;
}

// 从字节数组中提取整数（大端排列，bitLength可能不是8的倍数）
static quint64 assembleFromBigEndianBits(const QByteArray& bitsBytes, int bitLength) {
    quint64 result = 0;
    int byteLen = bitsBytes.size();
    for (int i = 0; i < byteLen; ++i) {
        result = (result << 8) | static_cast<quint8>(bitsBytes[i]);
    }
    result >>= (byteLen * 8 - bitLength);
    return result;
}

// 将整数分解为大端比特排列的字节数组
static QByteArray decomposeToBigEndianBits(quint64 value, int bitLength) {
    int byteLen = (bitLength + 7) / 8;
    QByteArray bytes(byteLen, 0);
    value <<= (byteLen * 8 - bitLength);
    for (int i = 0; i < byteLen; ++i) {
        bytes[i] = static_cast<char>((value >> ((byteLen - 1 - i) * 8)) & 0xFF);
    }
    return bytes;
}

// ==================== ProtocolSchema 类实现 ====================
ProtocolSchema::ProtocolSchema() {}

ProtocolSchema& ProtocolSchema::addField(const QString& name, int startByte, int startBit,
                                         int bitLength, ValueType type, Endian endian,
                                         BitOrder bitOrder, bool isSigned,
                                         double factor, double offset) {
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
    m_fields.append(f);
    return *this;
}

ProtocolSchema& ProtocolSchema::addVariableField(const QString& name, int startByte, int startBit,
                                                 const QString& lenField, ValueType type,
                                                 Endian endian, BitOrder bitOrder,
                                                 double factor, double offset) {
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
    m_fields.clear();
}

QStringList ProtocolSchema::checkOverlap() const {
    QStringList overlaps;
    for (int i = 0; i < m_fields.size(); ++i) {
        const Field& a = m_fields[i];
        if (a.bitLength == 0) continue;
        int a_start = absoluteBitOffset(a);
        int a_end = a_start + a.bitLength;
        for (int j = i + 1; j < m_fields.size(); ++j) {
            const Field& b = m_fields[j];
            if (b.bitLength == 0) continue;
            int b_start = absoluteBitOffset(b);
            int b_end = b_start + b.bitLength;
            if (a_end > b_start && b_end > a_start) {
                overlaps << a.name + " and " + b.name;
            }
        }
    }
    return overlaps;
}

// ---------- 核心：按 Physical 或 MsbFirst 读取比特 ----------
bool ProtocolSchema::readBits(const QByteArray& data, int bitOffset, int bitLength,
                              quint64& out, Endian endian, BitOrder bitOrder) const {
    int totalBits = data.size() * 8;
    if (bitOffset < 0 || bitLength <= 0 || bitLength > 64 || bitOffset + bitLength > totalBits) {
        out = 0;
        return false;
    }

    // 对于 Physical 模式：直接按物理比特索引读取，bit0 为最低位
    if (bitOrder == Physical) {
        quint64 value = 0;
        for (int i = 0; i < bitLength; ++i) {
            int bitIdx = bitOffset + i;
            int byteIdx = bitIdx / 8;
            int bitInByte = bitIdx % 8;
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
    if (bitOffset < 0 || bitLength <= 0 || bitLength > 64) {
        if (errorMsg) *errorMsg = "Invalid bit offset or length";
        return false;
    }
    int totalBits = data.size() * 8;
    if (bitOffset + bitLength > totalBits) {
        if (errorMsg) *errorMsg = "Write out of bounds";
        return false;
    }

    value &= maskBits(bitLength);

    // Physical 模式
    if (bitOrder == Physical) {
        // 如果要求大端字节序且长度>8，先转换 value 为小端内部表示
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
            int bitIdx = bitOffset + i;
            int byteIdx = bitIdx / 8;
            int bitInByte = bitIdx % 8;
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
    int startByte = bitOffset / 8;
    int startBitInByte = bitOffset % 8;
    int bitsWritten = 0;
    int srcByteIdx = 0;
    int srcBitPos = 0; // 在源字节内的比特位置（0 = 最高位）
    while (bitsWritten < bitLength && srcByteIdx < bitsBytes.size()) {
        quint8 srcByte = static_cast<quint8>(bitsBytes[srcByteIdx]);
        int bitsRemainingInSrc = 8 - srcBitPos;
        int targetByteIdx = startByte + (bitsWritten / 8);
        int targetBitPos = bitsWritten % 8;
        int bitsToWrite = qMin(bitsRemainingInSrc, bitLength - bitsWritten);
        quint8 srcSegment = (srcByte >> (8 - srcBitPos - bitsToWrite)) & ((1 << bitsToWrite) - 1);
        uchar* targetByte = reinterpret_cast<uchar*>(data.data() + targetByteIdx);
        uchar clearMask = ~(((1 << bitsToWrite) - 1) << (8 - targetBitPos - bitsToWrite));
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
                                     QByteArray& out, Endian endian, BitOrder bitOrder) const {
    if (bitLength <= 0 || bitOffset < 0 || bitOffset + bitLength > data.size() * 8) {
        out.clear();
        return false;
    }
    int byteLen = (bitLength + 7) / 8;
    out.resize(byteLen);
    out.fill(0);
    for (int i = 0; i < byteLen; ++i) {
        int bitsThisByte = qMin(8, bitLength - i * 8);
        quint64 byteVal = 0;
        if (!readBits(data, bitOffset + i * 8, bitsThisByte, byteVal, endian, bitOrder)) {
            out.clear();
            return false;
        }
        out[i] = static_cast<char>(byteVal & 0xFF);
    }
    // 对于 MsbFirst 且长度>8且小端时，输出字节数组需要反转以保持物理顺序？
    // 这里为了与 encodeValue 对称，不做额外反转，由调用者负责。
    return true;
}

// ---------- 字节数组写入 ----------
bool ProtocolSchema::writeBitsToBytes(QByteArray& data, int bitOffset, const QByteArray& bytes,
                                      Endian endian, BitOrder bitOrder, QString* errorMsg) const {
    if (bitOffset < 0 || bytes.isEmpty()) return true;
    int totalBits = data.size() * 8;
    int bitLength = bytes.size() * 8;
    if (bitOffset + bitLength > totalBits) {
        if (errorMsg) *errorMsg = "WriteBitsToBytes out of bounds";
        return false;
    }
    // 逐个字节写入
    QByteArray toWrite = bytes;
    // 对于 MsbFirst 模式且需要小端时，需要先反转字节数组吗？不，writeBits 会处理内部字节序。
    for (int i = 0; i < toWrite.size(); ++i) {
        quint8 byteVal = static_cast<quint8>(toWrite[i]);
        if (!writeBits(data, bitOffset + i * 8, 8, byteVal, endian, bitOrder, errorMsg))
            return false;
    }
    return true;
}

// ---------- JSON 值转换 ----------
QByteArray ProtocolSchema::encodeValue(const QJsonValue& value, ValueType type, int fixedBytes,
                                       QString* errorMsg) const {
    QByteArray result;
    switch (type) {
    case HexString: {
        QString hex = value.toString();
        result = QByteArray::fromHex(hex.toLatin1());
        if (result.isEmpty() && !hex.isEmpty()) {
            if (errorMsg) *errorMsg = "Invalid hex string";
            return QByteArray();
        }
        break;
    }
    case Base64:
    case RawBytes: {
        QString b64 = value.toString();
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
    if (!value.isDouble()) {
        if (errorMsg) *errorMsg = "Value is not a number";
        return false;
    }
    double physicalValue = value.toDouble();
    if (qFuzzyIsNull(factor)) {
        if (errorMsg) *errorMsg = "Factor is zero, cannot invert";
        return false;
    }
    double rawDouble = (physicalValue - offset) / factor;
    qint64 signedRaw = static_cast<qint64>(rawDouble);
    if (isSigned) {
        qint64 min = -(1LL << (bitLength - 1));
        qint64 max = (1LL << (bitLength - 1)) - 1;
        if (signedRaw < min || signedRaw > max) {
            if (errorMsg) *errorMsg = QString("Inverse transformed value %1 out of signed range [%2,%3]")
                                          .arg(signedRaw).arg(min).arg(max);
            return false;
        }
        out = static_cast<quint64>(signedRaw) & maskBits(bitLength);
    } else {
        quint64 max = (bitLength == 64) ? ~0ULL : (1ULL << bitLength) - 1;
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
    if (isSigned && bitLength < 64 && (rawValue >> (bitLength - 1)) & 0x01) {
        signedRaw |= (~0ULL << bitLength);
    }
    double value = static_cast<double>(signedRaw);
    return value * factor + offset;
}

// ---------- 公有解析和打包 ----------
QJsonObject ProtocolSchema::parse(const QByteArray& data, QString* errorMsg) const {
    QJsonObject result;
    // 先解析固定长度字段
    for (const Field& f : m_fields) {
        if (f.bitLength > 0) {
            int bitOffset = absoluteBitOffset(f);
            if (f.type == Int || f.type == UInt) {
                quint64 raw;
                if (!readBits(data, bitOffset, f.bitLength, raw, f.endian, f.bitOrder)) {
                    if (errorMsg) *errorMsg = QString("Failed to read field '%1'").arg(f.name);
                    result.insert(f.name, QJsonValue::Null);
                    continue;
                }
                double finalVal = applyLinearTransform(raw, f.factor, f.offset, f.isSigned, f.bitLength);
                result.insert(f.name, finalVal);
            } else {
                QByteArray bytes;
                if (!readBitsToBytes(data, bitOffset, f.bitLength, bytes, f.endian, f.bitOrder)) {
                    if (errorMsg) *errorMsg = QString("Failed to read field '%1'").arg(f.name);
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
                if (errorMsg) *errorMsg = QString("Length field '%1' missing for '%2'")
                                              .arg(f.lenField, f.name);
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            bool ok;
            qint64 lenBytes = result[f.lenField].toVariant().toLongLong(&ok);
            if (!ok || lenBytes < 0 || lenBytes > 1024*1024) {
                if (errorMsg) *errorMsg = QString("Invalid length for field '%1'").arg(f.name);
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            int bitOffset = absoluteBitOffset(f);
            int bitLength = static_cast<int>(lenBytes * 8);
            if (bitOffset + bitLength > data.size() * 8) {
                if (errorMsg) *errorMsg = QString("Variable field '%1' out of bounds").arg(f.name);
                result.insert(f.name, QJsonValue::Null);
                continue;
            }
            QByteArray bytes;
            if (!readBitsToBytes(data, bitOffset, bitLength, bytes, f.endian, f.bitOrder)) {
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
    QHash<QString, QString> lenToVar;
    QHash<QString, int> varLengths;
    QHash<QString, QByteArray> varContents;
    for (const Field& f : m_fields) {
        if (f.bitLength == 0) lenToVar[f.lenField] = f.name;
    }
    // 计算变长字段内容
    for (const Field& f : m_fields) {
        if (f.bitLength == 0) {
            QJsonValue val = values.value(f.name);
            if (val.isNull() || val.isUndefined()) {
                if (errorMsg) *errorMsg = QString("Missing value for variable field '%1'").arg(f.name);
                return false;
            }
            QByteArray content = encodeValue(val, f.type, -1, errorMsg);
            if (content.isNull()) return false;
            varContents[f.name] = content;
            varLengths[f.name] = content.size();
        }
    }
    // 计算总大小
    int maxBit = 0;
    for (const Field& f : m_fields) {
        int bitEnd = absoluteBitOffset(f);
        if (f.bitLength > 0) bitEnd += f.bitLength - 1;
        else {
            QByteArray content = varContents.value(f.name);
            if (!content.isEmpty()) bitEnd += content.size() * 8 - 1;
        }
        if (bitEnd > maxBit) maxBit = bitEnd;
    }
    out.resize((maxBit + 7) / 8);
    out.fill(0);
    // 写入
    for (const Field& f : m_fields) {
        int bitOffset = absoluteBitOffset(f);
        if (f.bitLength > 0) {
            if (lenToVar.contains(f.name)) {
                QString varName = lenToVar[f.name];
                int lenBytes = varLengths.value(varName, -1);
                if (lenBytes < 0) {
                    if (errorMsg) *errorMsg = QString("Length field '%1' for '%2' not computed")
                                                  .arg(f.name, varName);
                    return false;
                }
                quint64 intVal = static_cast<quint64>(lenBytes) & maskBits(f.bitLength);
                if (!writeBits(out, bitOffset, f.bitLength, intVal, f.endian, f.bitOrder, errorMsg))
                    return false;
            } else {
                QJsonValue val = values.value(f.name);
                if (val.isNull() || val.isUndefined()) {
                    if (errorMsg) *errorMsg = QString("Missing value for field '%1'").arg(f.name);
                    return false;
                }
                if (f.type == Int || f.type == UInt) {
                    quint64 intVal;
                    if (!valueToInteger(val, f.bitLength, f.isSigned, f.factor, f.offset, intVal, errorMsg))
                        return false;
                    if (!writeBits(out, bitOffset, f.bitLength, intVal, f.endian, f.bitOrder, errorMsg))
                        return false;
                } else {
                    int fixedBytes = (f.bitLength + 7) / 8;
                    QByteArray bytes = encodeValue(val, f.type, fixedBytes, errorMsg);
                    if (bytes.isNull()) return false;
                    if (!writeBitsToBytes(out, bitOffset, bytes, f.endian, f.bitOrder, errorMsg))
                        return false;
                }
            }
        } else {
            QByteArray content = varContents.value(f.name);
            if (!content.isEmpty()) {
                if (!writeBitsToBytes(out, bitOffset, content, f.endian, f.bitOrder, errorMsg))
                    return false;
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
