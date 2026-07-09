#include "TcpClient.h"

#include <QThread>

#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QThread>

namespace Sqz::NetWork {
TcpClientWorker::TcpClientWorker(QObject *parent)
    : QObject(parent)
{
}

TcpClientWorker::~TcpClientWorker()
{
    delete m_socket;
}

void TcpClientWorker::doConnect(const QString &host, quint16 port)
{
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_socket = new QTcpSocket(nullptr);

    // 连接信号（注意 error 信号重载，使用 QOverload）
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &TcpClientWorker::onSocketError);
    connect(m_socket, &QTcpSocket::connected, this, &TcpClientWorker::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClientWorker::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClientWorker::onReadyRead);

    m_socket->connectToHost(host, port);
}

void TcpClientWorker::doDisconnect()
{
    if (m_socket)
        m_socket->disconnectFromHost();
}

bool TcpClientWorker::isConnected() const
{
    return m_socket && m_socket->state() == QTcpSocket::ConnectedState;
}

void TcpClientWorker::setAutoReconnect(bool enable, int intervalMs)
{
    m_autoReconnect = enable;
    m_reconnectInterval = intervalMs;

    if (enable && !m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClientWorker::doReconnect);
    } else if (!enable && m_reconnectTimer) {
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }
}

void TcpClientWorker::setChunkSize(qint64 chunkSize)
{
    if (chunkSize > 0)
        m_chunkSize = chunkSize;
}

void TcpClientWorker::doSend(const QByteArray &data)
{
    if (!m_socket || m_socket->state() != QTcpSocket::ConnectedState) {
        m_pendingSends.enqueue(data);
        if (m_pendingSends.size() > 1000)
            m_pendingSends.dequeue();   // 限制队列长度，防止内存爆炸
        return;
    }

    // 构造长度头（4字节大端序）
    QByteArray packet;
    QDataStream ds(&packet, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint32(data.size());
    packet.append(data);
    m_socket->write(packet);
}

void TcpClientWorker::doSendFile(const QString &filePath)
{
    if (m_fileSendActive) {
        emit fileSendFinished(false, tr("Already sending a file"));
        return;
    }

    m_cancelSend = false;
    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        emit fileSendFinished(false, m_file->errorString());
        delete m_file;
        m_file = nullptr;
        return;
    }

    m_fileTotalBytes = m_file->size();
    m_fileSentBytes = 0;
    m_fileSendActive = true;

    // 发送元数据（类型 0x01）
    QByteArray meta;
    QDataStream ds(&meta, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint8(0x01) << QFileInfo(filePath).fileName() << m_fileTotalBytes << quint64(m_chunkSize);
    doSend(meta);

    // 开始发送第一个分块
    startNextChunk();
}

void TcpClientWorker::cancelSendFile()
{
    m_cancelSend = true;
}

// ---------- 私有槽函数 ----------
void TcpClientWorker::onSocketConnected()
{
    // 发送积压的消息
    while (!m_pendingSends.isEmpty())
        doSend(m_pendingSends.dequeue());

    emit connected();
    if (m_reconnectTimer)
        m_reconnectTimer->stop();
}

void TcpClientWorker::onSocketDisconnected()
{
    emit disconnected();
    if (m_autoReconnect && m_reconnectTimer && !m_reconnectTimer->isActive())
        m_reconnectTimer->start(m_reconnectInterval);
}

void TcpClientWorker::onSocketError(QAbstractSocket::SocketError)
{
    emit error(m_socket ? m_socket->errorString() : tr("Socket error"));
}

void TcpClientWorker::onReadyRead()
{
    m_recvBuffer.append(m_socket->readAll());
    tryExtractMessages();
}

void TcpClientWorker::doReconnect()
{
    if (m_socket && m_socket->state() != QTcpSocket::ConnectedState) {
        m_socket->connectToHost(m_socket->peerName(), m_socket->peerPort());
    }
}

void TcpClientWorker::startNextChunk()
{
    if (m_cancelSend) {
        finishFileSend(false, tr("Cancelled by user"));
        return;
    }

    if (m_fileSentBytes >= m_fileTotalBytes) {
        finishFileSend(true, QString());
        return;
    }

    qint64 toRead = qMin(m_chunkSize, m_fileTotalBytes - m_fileSentBytes);
    QByteArray chunk = m_file->read(toRead);
    if (chunk.isEmpty()) {
        finishFileSend(false, tr("File read error"));
        return;
    }

    // 构造数据块消息（类型 0x02 + 块序号 + 数据）
    QByteArray blockMsg;
    QDataStream ds(&blockMsg, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint8(0x02) << quint32(m_fileSentBytes / m_chunkSize) << chunk;
    doSend(blockMsg);

    m_fileSentBytes += chunk.size();
    emit fileProgress(m_fileSentBytes, m_fileTotalBytes);

    // 递归发送下一块，使用单次定时器避免深度递归
    QTimer::singleShot(0, this, &TcpClientWorker::startNextChunk);
}

void TcpClientWorker::finishFileSend(bool success, const QString &err)
{
    m_fileSendActive = false;
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    emit fileSendFinished(success, err);
}

void TcpClientWorker::tryExtractMessages()
{
    while (true) {
        if (m_recvBuffer.size() < 4)
            break;

        QDataStream ds(m_recvBuffer);
        ds.setByteOrder(QDataStream::BigEndian);
        quint32 len;
        ds >> len;

        if (len > 10 * 1024 * 1024) {   // 单包上限 10MB
            emit error(tr("Packet too large"));
            m_socket->disconnectFromHost();
            return;
        }

        if (m_recvBuffer.size() < static_cast<int>(4 + len))
            break;

        QByteArray msg = m_recvBuffer.mid(4, len);
        m_recvBuffer.remove(0, 4 + len);

        if (!msg.isEmpty()) {
            quint8 type = static_cast<quint8>(msg[0]);
            if (type == 0x01)
                handleFileMeta(msg);
            else if (type == 0x02)
                handleFileChunk(msg);
            else
                emit messageReady(msg);
        }
    }
}

void TcpClientWorker::handleFileMeta(const QByteArray &data)
{
    QDataStream ds(data);
    ds.setByteOrder(QDataStream::BigEndian);
    quint8 type;
    QString fileName;
    qint64 totalBytes;
    quint64 chunkSize;
    ds >> type >> fileName >> totalBytes >> chunkSize;
    if (type != 0x01)
        return;

    m_recvFile.reset(new QFile(QDir::temp().absoluteFilePath(fileName)));
    if (!m_recvFile->open(QIODevice::WriteOnly)) {
        emit error(tr("Cannot create file: %1").arg(m_recvFile->errorString()));
        return;
    }
    m_recvTotalBytes = totalBytes;
    m_recvBytesWritten = 0;
    m_recvChunkSize = chunkSize;   // 记录对端使用的块大小（仅用于信息）
}

void TcpClientWorker::handleFileChunk(const QByteArray &data)
{
    if (!m_recvFile)
        return;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::BigEndian);
    quint8 type;
    quint32 index;
    QByteArray chunk;
    ds >> type >> index >> chunk;
    if (type != 0x02)
        return;

    m_recvFile->write(chunk);
    m_recvBytesWritten += chunk.size();

    if (m_recvBytesWritten >= m_recvTotalBytes) {
        m_recvFile->close();
        emit fileReceived(m_recvFile->fileName());
        m_recvFile.reset();
    }
}



TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
{
    m_thread = new QThread(this);
    m_worker = new TcpClientWorker();

    // 将 Worker 移动到子线程
    m_worker->moveToThread(m_thread);

    // 连接 Worker 信号到本类的转发槽函数（队列连接，保证线程安全）
    connect(m_worker, &TcpClientWorker::messageReady,
            this, &TcpClient::onWorkerMessageReady, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::connected,
            this, &TcpClient::onWorkerConnected, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::disconnected,
            this, &TcpClient::onWorkerDisconnected, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::error,
            this, &TcpClient::onWorkerError, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::fileProgress,
            this, &TcpClient::onWorkerFileProgress, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::fileSendFinished,
            this, &TcpClient::onWorkerFileFinished, Qt::QueuedConnection);
    connect(m_worker, &TcpClientWorker::fileReceived,
            this, &TcpClient::fileReceived, Qt::QueuedConnection);

    // 启动子线程
    m_thread->start();

    // 将分块大小传递给 Worker
    QMetaObject::invokeMethod(m_worker, "setChunkSize",
                              Qt::QueuedConnection, Q_ARG(qint64, m_chunkSize));
}

TcpClient::~TcpClient()
{
    m_thread->quit();
    m_thread->wait();
}

void TcpClient::connectToHost(const QString &host, quint16 port)
{
    QMetaObject::invokeMethod(m_worker, "doConnect",
                              Qt::QueuedConnection,
                              Q_ARG(QString, host),
                              Q_ARG(quint16, port));
}

void TcpClient::disconnect()
{
    QMetaObject::invokeMethod(m_worker, "doDisconnect", Qt::QueuedConnection);
}

bool TcpClient::isConnected() const
{
    bool result = false;
    QMetaObject::invokeMethod(m_worker, "isConnected",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result));
    return result;
}

void TcpClient::setAutoReconnect(bool enable, int intervalMs)
{
    QMetaObject::invokeMethod(m_worker, "setAutoReconnect",
                              Qt::QueuedConnection,
                              Q_ARG(bool, enable),
                              Q_ARG(int, intervalMs));
}

void TcpClient::setFileChunkSize(qint64 chunkSize)
{
    if (chunkSize <= 0)
        return;
    m_chunkSize = chunkSize;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setChunkSize",
                                  Qt::QueuedConnection,
                                  Q_ARG(qint64, m_chunkSize));
    }
}

void TcpClient::send(const QByteArray &data)
{
    QMetaObject::invokeMethod(m_worker, "doSend",
                              Qt::QueuedConnection,
                              Q_ARG(QByteArray, data));
}

void TcpClient::sendFile(const QString &filePath)
{
    QMetaObject::invokeMethod(m_worker, "doSendFile",
                              Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
}

void TcpClient::cancelFileSend()
{
    QMetaObject::invokeMethod(m_worker, "cancelSendFile", Qt::QueuedConnection);
}

// ---------- 转发槽函数 ----------
void TcpClient::onWorkerMessageReady(const QByteArray &msg)
{
    emit dataReceived(msg);
}

void TcpClient::onWorkerConnected()
{
    emit connected();
}

void TcpClient::onWorkerDisconnected()
{
    emit disconnected();
}

void TcpClient::onWorkerError(const QString &err)
{
    emit errorOccurred(err);
}

void TcpClient::onWorkerFileProgress(qint64 sent, qint64 total)
{
    emit fileProgress(sent, total);
}

void TcpClient::onWorkerFileFinished(bool success, const QString &err)
{
    emit fileSendFinished(success, err);
}
}
