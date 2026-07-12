#include "UdpServer.h"

#include <QDebug>
#include <QNetworkConfiguration>
#include <Logger.h>
#include <SqzBus.h>
#include <QMetaType>

Q_DECLARE_METATYPE(QHostAddress)
namespace Sqz {


UdpServer::UdpServer()
{
    m_Socket = new QUdpSocket(this);
    groupSocket = new QUdpSocket(this);
    groupSocket->setSocketOption(QAbstractSocket::MulticastTtlOption,1);

    connect(m_Socket,&QUdpSocket::readyRead,this,&UdpServer::onReadyRead);
    connect(groupSocket, &QUdpSocket::readyRead, this, &UdpServer::groupRecvData);

}

UdpServer::~UdpServer()
{
    m_Socket->deleteLater();
    groupSocket->deleteLater();
}

void UdpServer::SetSendIpAndPort(QString ip, quint16 port)
{
    _ip.setAddress(ip);
    _port = port;
}

void UdpServer::SetSendIpAndPort(QHostAddress address, quint16 port)
{
    _ip = address;
    _port = port;
}


void UdpServer::SendSendInfo(QString ip, quint16 port)
{
    _ip.setAddress(ip);
    _port = port;
}

bool UdpServer::SetIpAndPort(QString ip, quint16 port)
{
    _sendIp.setAddress(ip);
    _sendPort = port;
    if(m_Socket->bind(_sendIp,_sendPort))
        return true;
    else
        return false;
}

bool UdpServer::SetAnyHostPort()
{
    if(m_Socket->bind(QHostAddress(QHostAddress::Any)))
        return true;
    else
        return false;
}

bool UdpServer::SetLocalHostPort(quint16 port)
{
    _sendIp.setAddress(QHostAddress::LocalHost);
    _sendPort = port;
    if(m_Socket->bind(_sendIp,_sendPort))
        return true;
    else
        return false;
}

bool UdpServer::SetMulticastIpAndPort(QString groupIp, int groupPort)
{
    groupAddr = QHostAddress(groupIp);
    _groupPort = groupPort;
    if(groupSocket->bind(QHostAddress::AnyIPv4,_groupPort,QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        //加入组播
        if(groupSocket->joinMulticastGroup(groupAddr))
            return true;
        else
            return false;
    }
    else
        return false;
}

void UdpServer::SendMulticastMessage(const char *data, int len)
{
    groupSocket->writeDatagram(data,len,groupAddr,_groupPort);
}
void UdpServer::SendMessage(const QString &data)
{

    m_Socket->writeDatagram(data.toUtf8(),_ip,_port);
}

void UdpServer::SendMessage(const char *data, int len)
{

    m_Socket->writeDatagram(data,len,_ip,_port);
}

void UdpServer::SendMessage(const char *data, int len, QString ip, quint16 port)
{
    _ip.setAddress(ip);
    _port = port;
    m_Socket->writeDatagram(data,len,_ip,_port);
}


void UdpServer::SendMessage(const QString &data, QString ip, quint16 port)
{
    _ip.setAddress(ip);
    _port = port;
    m_Socket->writeDatagram(data.toUtf8(),_ip,_port);
}

void UdpServer::SendMessage(const QByteArray &data)
{
    m_Socket->writeDatagram(data,_ip,_port);
}

void UdpServer::SendMessage(const QHostAddress &address, const quint16 &port, const QByteArray &data)
{

    m_Socket->writeDatagram(data,address,port);
}

void UdpServer::ReceiveData()
{
    while (m_Socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(m_Socket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        qint64 size = m_Socket->readDatagram(data.data(),data.size(),&senderAddress,&senderPort);
        if(size != -1)
        {
            SqzBus::Send("GET_UDP_DATA",{QVariant::fromValue(senderAddress),senderPort,data});
            //            emit DataReceived(data);
        }
    }

}

void UdpServer::ReceiveGroupData()
{
    while (groupSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(groupSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        qint64 aa = groupSocket->readDatagram(data.data(),data.size(),&senderAddress,&senderPort);
        if(aa != -1)
            emit GroupDataReceived(data);
    }
}

QUdpSocket *UdpServer::GetUdpSocket()
{
    return m_Socket;
}

void UdpServer::Close()
{
    if(m_Socket->state() == QUdpSocket::BoundState){
        m_Socket->close();
    }
}

void UdpServer::GroupClose()
{
    if(groupSocket->state() == QUdpSocket::BoundState){
        groupSocket->close();
    }
}

void UdpServer::onReadyRead()
{
    ReceiveData();
}

void UdpServer::groupRecvData()
{
    ReceiveGroupData();
}
}
