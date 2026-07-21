#ifndef PROTOCOLSCHEMATEST_H
#define PROTOCOLSCHEMATEST_H

#include <QtTest>
#include "ProtocolSchema.h"

class ProtocolSchemaTest : public QObject
{
    Q_OBJECT
private slots:
    // 基础字段配置校验
    void testFieldAddParamCheck();
    void testDuplicateFieldName();
    void testVarFieldCycleDependency();

    // 位域基础读写 MsbFirst
    void testMsbFirstUint8();
    void testMsbFirstUint16BigEndian();
    void testMsbFirstUint16LittleEndian();
    void testMsbFirstCrossByteBitField();

    // Physical物理位模式读写
    void testPhysicalNibbleLow4();
    void testPhysical12BitBigEndian();

    // 有符号整数 & 线性缩放factor/offset
    void testSignedInt16LinearTransform();
    void test64BitSignedInt();
    void testFactorZeroError();
    void testFloatRoundFix();

    // 字符串/十六进制/base64固定长度字段
    void testHexStringOddLengthError();
    void testUtf8StringFixedFillZero();
    void testRawBytesEncodeDecode();

    // 变长字段逻辑
    void testVarFieldBasic();
    void testVarFieldLengthOverflow();
    void testVarLenFieldMissingError();

    // 字段重叠检测
    void testFieldOverlapStatic();
    void testVarFieldOverlapRuntime();

    // 编解码往返一致性（核心校验）
    void testPackParseRoundTripAllTypes();

    // 边界异常：超大bit偏移、非法位宽、越界读写
    void testBitOffsetOverflowInt();
    void testInvalidBitLength();
    void testReadWriteOutOfDataBound();
};

#endif // PROTOCOLSCHEMATEST_H
