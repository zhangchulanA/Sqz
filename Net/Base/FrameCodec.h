#ifndef FRAMECODEC_H
#define FRAMECODEC_H

#include "NetGlobal.h"
#include "NetDef.h"
#include <QByteArray>
#include <QList>

namespace Net
{
// 帧编解码器，解决TCP粘包，统一协议格式
class NET_EXPORT FrameCodec
{
public:
    FrameCodec() = default;

    // 编码单向推送帧
    QByteArray EncodePush(const QByteArray& payload);
    // 编码请求帧
    QByteArray EncodeRequest(quint32 seq, const QByteArray& payload);
    // 编码应答帧
    QByteArray EncodeResponse(quint32 seq, RspCode code, const QByteArray& payload);
    // 编码心跳包
    QByteArray EncodeHeartBeat();

    // 解析缓冲区，返回完整帧列表，剩余半包留在buffer
    QList<QByteArray> DecodeBuffer(QByteArray& buffer);
    // 从完整帧拆分元数据与业务载荷
    bool UnpackFrame(const QByteArray& frame, MsgType& type, quint32& seq, RspCode& code, QByteArray& payload);
};
}

#endif
