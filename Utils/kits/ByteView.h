#ifndef BYTEVIEW_H
#define BYTEVIEW_H

#include <QByteArray>
#include <vector>
#include <cstring>
#include <QString>

class ByteView {
public:
    explicit ByteView(const QByteArray& data)
        : m_data(data), m_ptr(data.constData()), m_len(data.size()) {}

    // 按单字符分割
    std::vector<QByteArray> split(char sep, bool skipEmpty = true) const {
        std::vector<QByteArray> parts;
        parts.reserve(16);

        const char* start = m_ptr;
        const char* end = m_ptr + m_len;
        const char* pos = start;

        while (pos < end) {
            const char* next = static_cast<const char*>(
                memchr(pos, sep, end - pos)
            );
            if (!next) next = end;

            size_t len = next - pos;
            if (!skipEmpty || len > 0) {
                // 零拷贝，仅引用原数据
                parts.emplace_back(QByteArray::fromRawData(pos, len));
            }

            pos = next + 1;
        }
        return parts;
    }

    // 按字符串分割（如 "\r\n"）
    std::vector<QByteArray> split(const QByteArray& sep, bool skipEmpty = true) const {
        std::vector<QByteArray> parts;
        parts.reserve(16);

        const char* start = m_ptr;
        const char* end = m_ptr + m_len;
        const char* pos = start;
        size_t sepLen = sep.size();

        if (sepLen == 0) {
            parts.emplace_back(QByteArray::fromRawData(m_ptr, m_len));
            return parts;
        }

        while (pos < end) {
            const char* next = static_cast<const char*>(
                memmem(pos, end - pos, sep.constData(), sepLen)
            );
            if (!next) next = end;

            size_t len = next - pos;
            if (!skipEmpty || len > 0) {
                parts.emplace_back(QByteArray::fromRawData(pos, len));
            }

            pos = next + sepLen;
        }
        return parts;
    }

    // 获取原始数据引用
    const QByteArray& data() const { return m_data; }
    size_t size() const { return m_len; }
    bool isEmpty() const { return m_len == 0; }

    // 转为 QString（按需复制）
    QString toString() const { return QString::fromUtf8(m_ptr, m_len); }

private:
    const QByteArray& m_data;
    const char* m_ptr;
    size_t m_len;
};

#endif // BYTEVIEW_H
