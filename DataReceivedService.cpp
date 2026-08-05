#include "DataReceivedService.h"

DataReceivedService::DataReceivedService(QObject *parent) : SqzService(parent)
  ,m_udp(new UdpServer),m_ptlsc(new PtlSc)
{
    QString err;
    m_ptlsc->loadFile("./config/NcpV2.json",&err);
    if(!err.isEmpty()) logerror << err;
}

DataReceivedService::~DataReceivedService()
{
    m_udp->deleteLater();
}

void DataReceivedService::onInit()
{

    connect(m_udp,&UdpServer::DataReceivedAndIP,this,&DataReceivedService::onGetUdpData);
}

void DataReceivedService::onClose()
{

}

void DataReceivedService::onGetUdpData(const QHostAddress address, const int port, QByteArray &data)
{

}

SQZ_REG_NOARG(DataReceivedService)
