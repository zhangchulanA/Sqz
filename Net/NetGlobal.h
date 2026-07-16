#ifndef NETGLOBAL_H
#define NETGLOBAL_H

#include <QObject>
#include <QByteArray>
#include <QList>

// DLL导出宏
#if defined(NET_LIB)
#  define NET_EXPORT Q_DECL_EXPORT
#else
#  define NET_EXPORT Q_DECL_IMPORT
#endif

namespace Net
{
// 前置所有对外类
class DataTransporter;
class RequestBuilder;
class AbstractChannel;
class TcpChannel;
class LocalChannel;
class UdpDiscover;
class TcpServer;
class LocalServer;
class FrameCodec;

struct RequestOption;
struct RequestError;
struct RequestResult;
struct DeviceInfo;

// 传输通道模式
enum class TransMode
{
    Auto,
    Tcp,
    LocalIpc
};
}

#endif
