#include "FrameCodec.h"
#include <QDataStream>
#include <QtEndian>

namespace Net
{
// 编码单向推送帧：bodyLen(4B) + type(1B) + seq(4B) + code(1B) + payload(NB)
QByteArray FrameCodec::EncodePush(const QByteArray& payload)
{
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    quint32 bodyLen = 1 + 4 + 1 + payload.size();
    ds << bodyLen;
    ds << quint8(MsgType::RawPush);
    ds << quint32(0);
    ds << quint8(0);
    frame.append(payload);
    return frame;
}

// 编码请求帧：seq为请求序列号，用于匹配应答
QByteArray FrameCodec::EncodeRequest(quint32 seq, const QByteArray& payload)
{
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    quint32 bodyLen = 1 + 4 + 1 + payload.size();
    ds << bodyLen;
    ds << quint8(MsgType::Request);
    ds << seq;
    ds << quint8(0);
    frame.append(payload);
    return frame;
}

// 编码应答帧：seq回传请求序列号，code表示应答状态
QByteArray FrameCodec::EncodeResponse(quint32 seq, RspCode code, const QByteArray& payload)
{
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    quint32 bodyLen = 1 + 4 + 1 + payload.size();
    ds << bodyLen;
    ds << quint8(MsgType::Response);
    ds << seq;
    ds << quint8(code);
    frame.append(payload);
    return frame;
}

// 编码心跳包：无业务载荷，仅含协议头
QByteArray FrameCodec::EncodeHeartBeat()
{
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    quint32 bodyLen = 1 + 4 + 1;
    ds << bodyLen;
    ds << quint8(MsgType::HeartBeat);
    ds << quint32(0);
    ds << quint8(0);
    return frame;
}

// 从缓冲区解析完整帧列表：先读取4字节bodyLen，再按长度截取帧体，剩余半包留在buffer
QList<QByteArray> FrameCodec::DecodeBuffer(QByteArray& buffer)
{
    QList<QByteArray> frames;
    while (buffer.size() >= 4)
    {
        quint32 totalLen = qFromBigEndian<quint32>((const uchar*)buffer.data());
        if ((quint32)buffer.size() < totalLen + 4)
            break;
        QByteArray oneFrame = buffer.mid(4, totalLen);
        buffer = buffer.mid(4 + totalLen);
        frames.append(oneFrame);
    }
    return frames;
}

// 从完整帧体中拆分元数据：type(1B) + seq(4B) + code(1B) + payload(NB)，帧体最小长度6字节
bool FrameCodec::UnpackFrame(const QByteArray& frame, MsgType& type, quint32& seq, RspCode& code, QByteArray& payload)
{
    if (frame.size() < 6) return false;
    QDataStream ds(frame);
    ds.setByteOrder(QDataStream::BigEndian);
    quint8 t;
    ds >> t;
    type = MsgType(t);
    ds >> seq;
    quint8 c;
    ds >> c;
    code = RspCode(c);
    payload = frame.mid(6);
    return true;
}
}