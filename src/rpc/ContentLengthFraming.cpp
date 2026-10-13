#include "rpc/ContentLengthFraming.h"

#include <QList>

#include <cstdio>

namespace pb::rpc {

QByteArray frameMessage(const QByteArray& payload)
{
    // Header + blank line, then the raw payload. The length is the byte count
    // of the payload itself, not of the frame.
    const QByteArray header = "Content-Length: "
        + QByteArray::number(payload.size()) + "\r\n\r\n";
    return header + payload;
}

QList<QByteArray> FrameDecoder::push(const QByteArray& bytes)
{
    m_buffer += bytes;

    // Scan repeatedly: one push may contain several framed messages.
    QList<QByteArray> out;
    for (;;) {
        const int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            // No complete header yet. If the buffer no longer has room for a
            // plausible header, treat the stream as garbage rather than growing
            // forever waiting for a delimiter that never comes.
            constexpr int kMaxHeader = 16 * 1024;
            if (m_buffer.size() > kMaxHeader) {
                m_malformed = true;
                return out;
            }
            return out;
        }

        const QByteArray header = m_buffer.left(headerEnd);
        int length = -1;
        const QList<QByteArray> lines = header.split('\n');
        for (const QByteArray& line : lines) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("Content-Length:"))
                length = trimmed.mid(sizeof("Content-Length:") - 1).trimmed().toInt();
        }
        if (length < 0 || length > m_maxPayload) {
            // A header that resolves to an invalid or oversized body is a
            // protocol violation: fail rather than buffer unbounded output.
            m_malformed = true;
            return out;
        }

        const int bodyStart = headerEnd + 4;
        if (m_buffer.size() < bodyStart + length)
            return out; // still waiting for the full body

        const QByteArray payload = m_buffer.mid(bodyStart, length);
        m_buffer.remove(0, bodyStart + length);
        out.append(payload);
    }
}

} // namespace pb::rpc