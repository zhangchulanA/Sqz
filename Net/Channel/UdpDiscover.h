#ifndef UDPDISCOVER_H
#define UDPDISCOVER_H

#include "NetGlobal.h"
#include "NetDef.h"
#include "FrameCodec.h"
#include <QUdpSocket>
#include <QTimer>

namespace Net
{
// UDP局域网设备扫描工具，仅探测，不传输业务数据
// 支持同步阻塞扫描和异步非阻塞扫描两种模式
class NET_EXPORT UdpDiscover : public QObject
{
    Q_OBJECT
public:
    explicit UdpDiscover(QObject* parent = nullptr);

    // 同步扫描：阻塞等待waitMs毫秒后返回结果（适合非UI线程调用）
    QList<DeviceInfo> ScanLan(int waitMs, quint16 broadcastPort);

    // 异步扫描：立即返回，扫描完成后通过SignalScanFinished信号通知（适合UI线程调用）
    void ScanLanAsync(int waitMs, quint16 broadcastPort);

signals:
    // 异步扫描完成信号，携带扫描到的设备列表
    void SignalScanFinished(const QList<DeviceInfo>& devices);

private slots:
    void OnReadDatagram();
    void OnAsyncScanTimeout();

private:
    // 解析响应数据中的设备名称，失败返回空字符串
    QString ParseDeviceName(const QByteArray& data);

    QUdpSocket mUdp;
    FrameCodec mCodec;
    QList<DeviceInfo> mDevList;
    QTimer mAsyncTimer;
    bool mAsyncMode = false;
};
}

#endif
