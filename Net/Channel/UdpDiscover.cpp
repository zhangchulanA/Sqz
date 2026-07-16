#include "UdpDiscover.h"
#include <QNetworkDatagram>
#include <QTimer>
#include <QEventLoop>

namespace Net
{
// 构造函数：绑定UDP端口，连接readyRead信号和异步超时定时器
UdpDiscover::UdpDiscover(QObject* parent)
    : QObject(parent)
{
    mUdp.bind(QHostAddress::Any, 0, QUdpSocket::ReuseAddressHint);
    connect(&mUdp, &QUdpSocket::readyRead, this, &UdpDiscover::OnReadDatagram);
    connect(&mAsyncTimer, &QTimer::timeout, this, &UdpDiscover::OnAsyncScanTimeout);
    mAsyncTimer.setSingleShot(true);
}

// 同步扫描：阻塞当前线程等待扫描结果（适合非UI线程调用）
QList<DeviceInfo> UdpDiscover::ScanLan(int waitMs, quint16 broadcastPort)
{
    mDevList.clear();
    mAsyncMode = false;
    QByteArray probe = mCodec.EncodePush("LanProbe");
    mUdp.writeDatagram(probe, QHostAddress::Broadcast, broadcastPort);

    QEventLoop loop;
    QTimer::singleShot(waitMs, &loop, &QEventLoop::quit);
    loop.exec();
    return mDevList;
}

// 异步扫描：立即返回，扫描完成后通过SignalScanFinished信号通知（适合UI线程调用）
void UdpDiscover::ScanLanAsync(int waitMs, quint16 broadcastPort)
{
    mDevList.clear();
    mAsyncMode = true;
    QByteArray probe = mCodec.EncodePush("LanProbe");
    mUdp.writeDatagram(probe, QHostAddress::Broadcast, broadcastPort);

    mAsyncTimer.start(waitMs);
}

// 异步扫描超时回调：收集结果并通知上层
void UdpDiscover::OnAsyncScanTimeout()
{
    if (mAsyncMode)
    {
        mAsyncMode = false;
        emit SignalScanFinished(mDevList);
    }
}

// 接收UDP数据报：解析响应数据中的设备信息
void UdpDiscover::OnReadDatagram()
{
    while (mUdp.hasPendingDatagrams())
    {
        QNetworkDatagram dg = mUdp.receiveDatagram();
        DeviceInfo info;
        info.ip = dg.senderAddress().toString();
        info.port = dg.senderPort();
        // 尝试从响应数据中解析设备名称，解析失败则使用IP作为标识
        info.devName = ParseDeviceName(dg.data());
        if (info.devName.isEmpty())
        {
            info.devName = info.ip;
        }
        mDevList.append(info);
    }
}

// 解析响应数据中的设备名称：尝试按UTF-8字符串解析
QString UdpDiscover::ParseDeviceName(const QByteArray& data)
{
    // 响应数据格式：FrameCodec编码的推送帧，payload为设备名称
    // 先尝试解码完整帧，提取payload
    QByteArray buffer = data;
    QList<QByteArray> frames = mCodec.DecodeBuffer(buffer);
    for (const auto& frame : frames)
    {
        MsgType type;
        quint32 seq;
        RspCode code;
        QByteArray payload;
        if (mCodec.UnpackFrame(frame, type, seq, code, payload))
        {
            if (type == MsgType::RawPush && !payload.isEmpty())
            {
                return QString::fromUtf8(payload);
            }
        }
    }
    // 如果无法按帧协议解析，尝试将整个数据作为纯文本设备名
    if (!data.isEmpty())
    {
        return QString::fromUtf8(data).trimmed();
    }
    return QString();
}
}