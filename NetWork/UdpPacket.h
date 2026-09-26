#pragma once

#include <QByteArray>
#include <QHash>
#include <QVector>
#include <QString>
#include <QElapsedTimer>
#include <cstring>

namespace Sqz {

// ============================================================
// 协议常量
// ============================================================
namespace Packet {
    constexpr quint32 kMagic      = 0x53515A31; // "SQZ1"
    constexpr quint8  kVersion    = 0x01;
    constexpr int     kHeaderSize = 16;

    // 单个数据报最大字节数（含头）。局域网可调 1400，公网保守 512~1200
    constexpr int     kMaxDatagram = 1200;

    // 每片可携带的数据量
    constexpr int     kChunkSize = kMaxDatagram - kHeaderSize; // 1184

    // 分片重组超时（毫秒）
    constexpr int     kReassemblyTimeoutMs = 3000;

    // 最多同时重组的消息数
    constexpr int     kMaxPendingMessages = 64;
}

// ---------- 大端读写辅助 ----------
inline void writeU16(char* p, quint16 v) {
    p[0] = char((v >> 8) & 0xFF);
    p[1] = char(v & 0xFF);
}
inline void writeU32(char* p, quint32 v) {
    p[0] = char((v >> 24) & 0xFF);
    p[1] = char((v >> 16) & 0xFF);
    p[2] = char((v >> 8) & 0xFF);
    p[3] = char(v & 0xFF);
}
inline quint16 readU16(const char* p) {
    return quint16((quint8(p[0]) << 8) | quint8(p[1]));
}
inline quint32 readU32(const char* p) {
    return (quint32(quint8(p[0])) << 24) |
           (quint32(quint8(p[1])) << 16) |
           (quint32(quint8(p[2])) << 8)  |
            quint32(quint8(p[3]));
}

// ============================================================
// 分包器：把完整 QByteArray 切成若干片
// ============================================================
class PacketSplitter
{
public:
    /**
     * @brief 把 payload 切成分片
     * @param payload 待发送的完整数据
     * @param msgId   消息 ID（同一消息的片共享它，用于接收端聚合）
     * @return 分片列表，每片都是一个可直接发送的 QByteArray；
     *         payload 为空或过大时返回空列表
     *
     * @note 单包也走同一格式：返回长度为 1 的列表，totalChunks=1,index=0
     */
    static QList<QByteArray> split(const QByteArray& payload,
                                   quint16 msgId)
    {
        QList<QByteArray> out;
        if (payload.isEmpty() || payload.size() > 0xFFFFFFFFu) {
            return out;
        }

        const int total = (payload.size() + Packet::kChunkSize - 1)
                          / Packet::kChunkSize;
        if (total <= 0 || total > 0xFFFF) {
            return out;
        }

        out.reserve(total);
        for (int i = 0; i < total; ++i) {
            const int offset = i * Packet::kChunkSize;
            const int len = qMin(Packet::kChunkSize,
                                 payload.size() - offset);

            QByteArray dgram(Packet::kHeaderSize + len, Qt::Uninitialized);
            char* p = dgram.data();

            writeU32(p + 0,  Packet::kMagic);
            p[4] = char(Packet::kVersion);
            p[5] = char(total > 1 ? 0x01 : 0x00); // 分片标志
            writeU16(p + 6,  msgId);
            writeU16(p + 8,  quint16(total));
            writeU16(p + 10, quint16(i));
            writeU32(p + 12, quint32(payload.size()));

            std::memcpy(p + Packet::kHeaderSize,
                        payload.constData() + offset, len);

            out.append(std::move(dgram));
        }
        return out;
    }

    /**
     * @brief 便捷接口：内部自增 msgId，返回 (msgId, 分片列表)
     * @note  非线程安全。多线程请每个线程各持一个实例，或外部加锁。
     */
    QList<QByteArray> splitAuto(const QByteArray& payload)
    {
        return split(payload, ++m_nextMsgId);
    }

private:
    quint16 m_nextMsgId = 0;
};

// ============================================================
// 合包器：把分片攒回完整 QByteArray
// ============================================================
class PacketAssembler
{
public:
    /**
     * @brief 接收一个数据报
     * @param datagram 收到的单包数据
     * @param out      若本次凑齐一条完整消息，写入 out 并返回 true
     * @return true 表示 out 被填充为完整消息；false 表示还没凑齐或本包无效
     *
     * @note 若同一发送方多路复用，请为每个发送方各持一个实例，
     *       或调用带 streamKey 的重载版本。
     */
    bool feed(const QByteArray& datagram, QByteArray& out)
    {
        return feed(QString(), datagram, out);
    }

    /**
     * @brief 带流标识的接收（多发送方场景）
     * @param streamKey 区分不同来源的 key（如 sender.toString()+port）
     */
    bool feed(const QString& streamKey,
              const QByteArray& datagram,
              QByteArray& out)
    {
        if (datagram.size() < Packet::kHeaderSize) {
            return false;
        }
        const char* p = datagram.constData();

        if (readU32(p) != Packet::kMagic) {
            return false;
        }
        if (quint8(p[4]) != Packet::kVersion) {
            return false;
        }

        const quint16 msgId       = readU16(p + 6);
        const quint16 totalChunks = readU16(p + 8);
        const quint16 chunkIndex  = readU16(p + 10);
        const quint32 totalLength = readU32(p + 12);

        if (totalChunks == 0 || chunkIndex >= totalChunks) {
            return false;
        }

        const QByteArray chunkData = datagram.mid(Packet::kHeaderSize);

        const QString key = streamKey + ":" + QString::number(msgId);

        auto it = m_pending.find(key);
        if (it == m_pending.end()) {
            if (m_pending.size() >= Packet::kMaxPendingMessages) {
                return false;
            }
            PendingMessage pm;
            pm.buffer.resize(int(totalLength));
            pm.received.resize(totalChunks);
            pm.totalChunks   = totalChunks;
            pm.totalLength   = totalLength;
            pm.timer.start();
            it = m_pending.insert(key, pm);
        }

        PendingMessage& pm = it.value();

        // 一致性校验
        if (pm.totalChunks != totalChunks ||
            pm.totalLength != totalLength) {
            m_pending.erase(it);
            return false;
        }

        // 去重
        if (pm.received[chunkIndex]) {
            return false;
        }

        const int offset = chunkIndex * Packet::kChunkSize;
        const int len    = qMin(chunkData.size(),
                                int(totalLength) - offset);
        if (len <= 0) {
            m_pending.erase(it);
            return false;
        }

        std::memcpy(pm.buffer.data() + offset,
                    chunkData.constData(), len);
        pm.received[chunkIndex] = true;
        ++pm.receivedCount;

        if (pm.receivedCount == pm.totalChunks) {
            out = pm.buffer;
            m_pending.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief 清理超时未凑齐的消息
     * @note  需要你定期调用（比如 QTimer 每秒一次）
     * @return 被清理的消息数
     */
    int cleanupExpired()
    {
        int removed = 0;
        auto it = m_pending.begin();
        while (it != m_pending.end()) {
            if (it.value().timer.elapsed() >
                Packet::kReassemblyTimeoutMs) {
                it = m_pending.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    /// 当前未完成的消息数（调试用）
    int pendingCount() const { return m_pending.size(); }

    /// 清空所有未完成的重组状态
    void clear() { m_pending.clear(); }

private:
    struct PendingMessage {
        QByteArray    buffer;
        QVector<bool> received;
        int           receivedCount = 0;
        int           totalChunks   = 0;
        quint32       totalLength   = 0;
        QElapsedTimer timer;
    };

    QHash<QString, PendingMessage> m_pending;
};

} // namespace Sqz
