#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <QObject>
#include <QUdpSocket>
#include <QMap>
#include <QFile>
#include "SqzGlobal.h"
/**
 * @brief UDP 客户端/服务端（支持普通数据报和文件传输）
 *
 * 文件传输采用分片发送（默认 1400 字节/片），接收端自动重组并保存到临时目录。
 * 支持发送进度、取消发送。
 */

namespace Sqz::NetWork {
class SQZ_FRAMEWORK_API UdpSocket : public QObject
{
    Q_OBJECT
public:
    explicit UdpSocket(QObject *parent = nullptr);
    ~UdpSocket();

    bool bind(quint16 port, QUdpSocket::BindMode mode = QUdpSocket::DefaultForPlatform);
    void sendDatagram(const QByteArray &data, const QHostAddress &addr, quint16 port);
    void sendFile(const QString &filePath, const QHostAddress &addr, quint16 port);
    void cancelFileSend();

signals:
    void datagramReceived(const QByteArray &data, const QHostAddress &senderAddr, quint16 senderPort);
    void fileReceived(const QString &filePath);
    void fileSendProgress(qint64 sentBytes, qint64 totalBytes);
    void fileSendFinished(bool success, const QString &errorMsg);

private slots:
    void onReadyRead();

private:
    void processPendingDatagrams();
    void sendNextChunk();

    QUdpSocket m_socket;

    // 文件发送状态
    QFile *m_sendFile = nullptr;
    qint64 m_sendFileTotal = 0;
    qint64 m_sendFileSent = 0;
    QHostAddress m_sendTargetAddr;
    quint16 m_sendTargetPort = 0;
    bool m_cancelSend = false;
    static constexpr qint64 MAX_PACKET_SIZE = 1400;   // 安全 UDP 负载大小

    // 文件接收状态（以 "IP:Port" 为键）
    struct ReceiveContext {
        int totalPackets = 0;
        QMap<int, QByteArray> chunks;
        int receivedCount = 0;
        QString fileName;
    };
    QMap<QString, ReceiveContext> m_receiveMap;
};
}
#endif // UDPSOCKET_H
