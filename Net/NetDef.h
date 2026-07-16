#ifndef NETDEF_H
#define NETDEF_H

#include <QString>
#include <QByteArray>


namespace Net
{
// 帧消息类型
enum class MsgType : quint8
{
    RawPush = 0,    // 单向推送无应答
    Request = 1,    // 请求报文
    Response = 2,   // 应答报文
    HeartBeat = 3   // 内部心跳包
};

// 应答状态码
enum class RspCode : quint8
{
    Success = 0,
    BusinessFail = 1,
    Timeout = 2,
    Disconnect = 3
};

// 失败统一载体
struct RequestError
{
    RspCode code;
    QString msg;
    QByteArray rawData;
};

// 请求配置参数
struct RequestOption
{
    int timeoutMs = 5000;
    bool cycleEnable = false;
    int cycleIntervalMs = 1000;
    int cycleMaxCount = -1; // -1无限循环
    int retryCount = 0;
};

// 同步请求返回结果
struct RequestResult
{
    RspCode code;
    QByteArray payload;
    bool IsSuccess() const { return code == RspCode::Success; }
};

// UDP扫描设备信息
struct DeviceInfo
{
    QString ip;
    quint16 port;
    QString devName;
};
}

#endif
