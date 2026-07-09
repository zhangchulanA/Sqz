#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QTimer>
#include <QFile>
#include <memory>
#include <QAbstractSocket>
#include <QTcpSocket>
#include "SqzGlobal.h"

namespace Sqz::NetWork {


/**
 * @brief TCP 客户端的实际工作类（运行于子线程）
 *
 * 负责所有 socket 操作、文件分块发送/接收、重连逻辑。
 * 通过信号与主线程的 TcpClient 通信。
 */
class SQZ_FRAMEWORK_API TcpClientWorker : public QObject
{
    Q_OBJECT
public:
    explicit TcpClientWorker(QObject *parent = nullptr);
    ~TcpClientWorker();

    // 以下槽函数由主线程通过 QMetaObject::invokeMethod 调用
public slots:
    void doConnect(const QString &host, quint16 port);
    void doDisconnect();
    bool isConnected() const;
    void setAutoReconnect(bool enable, int intervalMs);
    void setChunkSize(qint64 chunkSize);      // 设置文件传输分块大小

    void doSend(const QByteArray &data);
    void doSendFile(const QString &filePath);
    void cancelSendFile();

signals:
    void messageReady(const QByteArray &msg);
    void connected();
    void disconnected();
    void error(const QString &err);
    void fileProgress(qint64 sentBytes, qint64 totalBytes);
    void fileSendFinished(bool success, const QString &err);
    void fileReceived(const QString &filePath);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError);
    void onReadyRead();
    void doReconnect();
    void startNextChunk();        // 发送下一个文件分块
    void finishFileSend(bool success, const QString &err);

private:
    void tryExtractMessages();    // 从接收缓冲区解析完整消息
    void handleFileMeta(const QByteArray &data);
    void handleFileChunk(const QByteArray &data);

    QTcpSocket *m_socket = nullptr;
    QByteArray m_recvBuffer;
    QQueue<QByteArray> m_pendingSends;   // 未连接时积压的消息

    bool m_autoReconnect = false;
    int m_reconnectInterval = 3000;
    QTimer *m_reconnectTimer = nullptr;

    // 文件发送相关
    qint64 m_chunkSize = 64 * 1024;       // 可配置的分块大小（默认64KB）
    QFile *m_file = nullptr;
    qint64 m_fileTotalBytes = 0;
    qint64 m_fileSentBytes = 0;
    bool m_fileSendActive = false;
    bool m_cancelSend = false;

    // 文件接收相关
    std::unique_ptr<QFile> m_recvFile;
    qint64 m_recvTotalBytes = 0;
    qint64 m_recvBytesWritten = 0;
    qint64 m_recvChunkSize = 0;
};




/**
 * @brief TCP 客户端（主线程接口）
 *
 * 所有网络操作均在子线程中执行，不会阻塞主线程。
 * 支持普通消息收发、文件传输（分块发送、进度回调、取消）、自动重连。
 */
class SQZ_FRAMEWORK_API TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    // ---------- 连接管理 ----------
    void connectToHost(const QString &host, quint16 port);
    void disconnect();
    bool isConnected() const;                     // 阻塞查询，立即返回结果
    void setAutoReconnect(bool enable, int intervalMs = 3000);

    // ---------- 消息发送 ----------
    void send(const QByteArray &data);

    // ---------- 文件传输 ----------
    /**
     * @brief 设置文件传输的分块大小（必须在连接前调用）
     * @param chunkSize 块大小（字节），建议范围 4KB ~ 16MB，默认 64KB
     */
    void setFileChunkSize(qint64 chunkSize);
    qint64 fileChunkSize() const { return m_chunkSize; }

    void sendFile(const QString &filePath);      // 异步发送文件
    void cancelFileSend();                       // 取消正在发送的文件

signals:
    // 普通消息接收
    void dataReceived(const QByteArray &msg);

    // 连接状态
    void connected();
    void disconnected();
    void errorOccurred(const QString &err);

    // 文件发送进度与结果
    void fileProgress(qint64 sentBytes, qint64 totalBytes);
    void fileSendFinished(bool success, const QString &errorMsg);

    // 文件接收完成（自动保存到系统临时目录）
    void fileReceived(const QString &filePath);

private slots:
    // 接收来自 Worker 的信号，转发给外部使用者
    void onWorkerMessageReady(const QByteArray &msg);
    void onWorkerConnected();
    void onWorkerDisconnected();
    void onWorkerError(const QString &err);
    void onWorkerFileProgress(qint64 sent, qint64 total);
    void onWorkerFileFinished(bool success, const QString &err);

private:
    QThread *m_thread = nullptr;
    TcpClientWorker *m_worker = nullptr;
    qint64 m_chunkSize = 64 * 1024;   // 默认 64KB，可通过 setFileChunkSize() 修改
};
} //Sqz::NetWork
#endif // TCPCLIENT_H
