#ifndef DATARECEIVEDSERVICE_H
#define DATARECEIVEDSERVICE_H

#include <QObject>
#include <QMetaObject>
#include <SqzService.h>
#include "UdpServer.h"
#include "ProtocolSchema.h"

using namespace Sqz;

class DataReceivedService : public SqzService
{
    Q_OBJECT
public:
    explicit DataReceivedService(QObject *parent = nullptr);
    ~DataReceivedService();

protected:
    void onInit() override;

    void onClose() override;

public slots:
    void onGetUdpData(const QHostAddress address ,const int port,QByteArray& data);


protected:
    QString className() const override{return this->metaObject()->className();}

signals:  

private:
    UdpServer * m_udp;
    PtlSc * m_ptlsc;

};

#endif // DATARECEIVEDSERVICE_H
