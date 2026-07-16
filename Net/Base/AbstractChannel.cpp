#include "AbstractChannel.h"
#include <QDataStream>

namespace Net
{
// 构造函数：初始化QObject基类
AbstractChannel::AbstractChannel(QObject* parent)
    : QObject(parent)
{}

// IO可读回调：从设备读取数据，通过FrameCodec解析完整帧后逐帧向上发射
void AbstractChannel::OnReadyRead(QIODevice* dev)
{
    mRecvBuf.append(dev->readAll());
    auto frameList = mCodec.DecodeBuffer(mRecvBuf);
    for (auto& frame : frameList)
    {
        MsgType type;
        quint32 seq;
        RspCode code;
        QByteArray payload;
        if (mCodec.UnpackFrame(frame, type, seq, code, payload))
        {
            emit SignalRecvFullFrame(type, seq, code, payload);
        }
    }
}
}