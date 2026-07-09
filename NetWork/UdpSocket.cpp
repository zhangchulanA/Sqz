#include "UdpSocket.h"
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QNetworkDatagram>
#include <QTimer>

namespace Sqz::NetWork {
UdpSocket::UdpSocket(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpSocket::onReadyRead);
}

UdpSocket::~UdpSocket()
{
    delete m_sendFile;
}

bool UdpSocket::bind(quint16 port, QUdpSocket::BindMode mode)
{
    return m_socket.bind(port, mode);
}

void UdpSocket::sendDatagram(const QByteArray &data, const QHostAddress &addr, quint16 port)
{
    QByteArray packet;
    packet.append(char(0x00));   // 类型 0x00 = 普通消息
    packet.append(data);
    m_socket.writeDatagram(packet, addr, port);
}

void UdpSocket::sendFile(const QString &filePath, const QHostAddress &addr, quint16 port)
{
    if (m_sendFile) {
        emit fileSendFinished(false, tr("Already sending a file"));
        return;
    }

    m_cancelSend = false;
    m_sendFile = new QFile(filePath);
    if (!m_sendFile->open(QIODevice::ReadOnly)) {
        emit fileSendFinished(false, m_sendFile->errorString());
        delete m_sendFile;
        m_sendFile = nullptr;
        return;
    }

    m_sendFileTotal = m_sendFile->size();
    m_sendFileSent = 0;
    m_sendTargetAddr = addr;
    m_sendTargetPort = port;

    // 发送元数据包（类型 0x01）
    QByteArray meta;
    QDataStream ds(&meta, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint8(0x01) << QFileInfo(filePath).fileName() << m_sendFileTotal << quint16(MAX_PACKET_SIZE);
    m_socket.writeDatagram(meta, addr, port);

    sendNextChunk();
}

void UdpSocket::cancelFileSend()
{
    m_cancelSend = true;
}

void UdpSocket::onReadyRead()
{
    processPendingDatagrams();
}

void UdpSocket::processPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket.receiveDatagram();
        QByteArray data = dg.data();
        if (data.isEmpty())
            continue;

        quint8 type = static_cast<quint8>(data[0]);
        QByteArray payload = data.mid(1);

        if (type == 0x00) {
            emit datagramReceived(payload, dg.senderAddress(), dg.senderPort());
        }
        else if (type == 0x01) {
            // 文件元数据
            QDataStream ds(payload);
            ds.setByteOrder(QDataStream::BigEndian);
            QString fileName;
            qint64 totalBytes;
            quint16 chunkSize;
            ds >> fileName >> totalBytes >> chunkSize;

            QString sessionKey = QString("%1:%2").arg(dg.senderAddress().toString()).arg(dg.senderPort());
            ReceiveContext &ctx = m_receiveMap[sessionKey];
            ctx.totalPackets = (totalBytes + chunkSize - 1) / chunkSize;
            ctx.receivedCount = 0;
            ctx.chunks.clear();
            ctx.fileName = fileName;
        }
        else if (type == 0x02) {
            // 文件数据分片
            QDataStream ds(payload);
            ds.setByteOrder(QDataStream::BigEndian);
            quint16 index;
            QByteArray chunkData;
            ds >> index >> chunkData;

            QString sessionKey = QString("%1:%2").arg(dg.senderAddress().toString()).arg(dg.senderPort());
            auto it = m_receiveMap.find(sessionKey);
            if (it != m_receiveMap.end()) {
                ReceiveContext &ctx = *it;
                if (!ctx.chunks.contains(index)) {
                    ctx.chunks[index] = chunkData;
                    ctx.receivedCount++;
                }

                if (ctx.receivedCount == ctx.totalPackets) {
                    // 重组文件
                    QByteArray fileData;
                    bool missing = false;
                    for (int i = 0; i < ctx.totalPackets; ++i) {
                        if (!ctx.chunks.contains(i)) {
                            missing = true;
                            break;
                        }
                        fileData.append(ctx.chunks[i]);
                    }
                    if (!missing) {
                        QString savePath = QDir::temp().absoluteFilePath(ctx.fileName);
                        QFile outFile(savePath);
                        if (outFile.open(QIODevice::WriteOnly)) {
                            outFile.write(fileData);
                            outFile.close();
                            emit fileReceived(savePath);
                        }
                    }
                    m_receiveMap.erase(it);
                }
            }
        }
    }
}

void UdpSocket::sendNextChunk()
{
    if (m_cancelSend) {
        if (m_sendFile) {
            m_sendFile->close();
            delete m_sendFile;
            m_sendFile = nullptr;
        }
        emit fileSendFinished(false, tr("Cancelled"));
        return;
    }

    if (m_sendFileSent >= m_sendFileTotal) {
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        emit fileSendFinished(true, QString());
        return;
    }

    qint64 toRead = qMin(MAX_PACKET_SIZE, m_sendFileTotal - m_sendFileSent);
    QByteArray chunk = m_sendFile->read(toRead);
    if (chunk.isEmpty()) {
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        emit fileSendFinished(false, tr("File read error"));
        return;
    }

    QByteArray packet;
    QDataStream ds(&packet, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint8(0x02) << quint16(m_sendFileSent / MAX_PACKET_SIZE) << chunk;
    m_socket.writeDatagram(packet, m_sendTargetAddr, m_sendTargetPort);

    m_sendFileSent += chunk.size();
    emit fileSendProgress(m_sendFileSent, m_sendFileTotal);

    QTimer::singleShot(0, this, &UdpSocket::sendNextChunk);
}
}
