#ifndef UDPSERVER_H
#define UDPSERVER_H

#include <QMutex>
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#include <QTimer>
#include <QUdpSocket>
#include "SqzGlobal.h"

namespace Sqz::NetWork {
class SQZ_FRAMEWORK_API UdpServer : public QObject
{
    Q_OBJECT

public:
    UdpServer();
    ~UdpServer();
//******************************************************单播
    //设置接收方ip和port
    void SetSendIpAndPort(QString ip,quint16 port);
    void SetSendIpAndPort(QHostAddress address,quint16 port);
    //设置接收方ip和port
    void SendSendInfo(QString ip,quint16 port);
//设置本地ip和端口
    bool SetIpAndPort(QString ip,quint16 port);
//ip设为任意地址，设置端口
    bool SetAnyHostPort();
//ip设为本地循环 ，设置端口
    bool SetLocalHostPort(quint16 port);
    //单播发送消息
    void SendMessage(const QString& data);
    void SendMessage(const char* data,int len);
    void SendMessage(const char* data,int len,QString ip,quint16 port);
    void SendMessage(const QString& data,QString ip,quint16 port);
    void SendMessage(const QByteArray& data);
    void SendMessage(const QHostAddress& address,const quint16& port,const QByteArray& data);

//******************************************************组播
    //组播设置224.0.2.0
    bool SetMulticastIpAndPort(QString groupIp,int groupPort = 5678);
    //组播发送消息
    void SendMulticastMessage(const char* data, int len);
    //接收消息
    void ReceiveData();
    void ReceiveGroupData();
    //获得udp
    QUdpSocket* GetUdpSocket();
    //断开当前链接
    void Close();
    void GroupClose();
public slots:
    void onReadyRead();
    void groupRecvData();
signals:
    void DataReceived(const QByteArray& data);
    void DataReceivedAndIP(const QHostAddress address ,const int port, const QByteArray& data);
    void GroupDataReceived(const QByteArray& data);


private:

    QUdpSocket* m_Socket = nullptr;
    //接收方ip端口
    QHostAddress _ip;
    quint16 _port;
    //绑定发送方的端口
    QHostAddress _sendIp;
    quint16 _sendPort;
//--------------------------------组播
    QUdpSocket *groupSocket = nullptr;
    QHostAddress groupAddr;//组播地址
    int _groupPort;//组播端口



};
}
#endif // UDPSERVER_H
