#pragma once

#include <QByteArray>
#include <QList>

namespace pb::rpc {

// A single message bounded by a Content-Length header, as used by LSP and the
// JSON-RPC-over-stdio transport. The wire format is:
//     Content-Length: <N>\r\n\r\n<payload of N bytes>
// Anything on stderr is not part of the protocol and must never land here.

// Serialize a payload into a framed record ready to be written to the channel.
QByteArray frameMessage(const QByteArray& payload);

// Incremental decoder for an incoming byte stream. Feed it the raw bytes read
// from the channel; it returns every complete payload found in the batch, or
// an empty list when more data is needed. A malformed frame (a Content-Length
// header that never resolves) sets malformed() so the caller fails the channel
// instead of waiting forever.
class FrameDecoder {
public:
    // Append bytes and return all complete, unframed payloads in the batch.
    QList<QByteArray> push(const QByteArray& bytes);

    // True when push() detected an unrecoverable framing error.
    bool malformed() const { return m_malformed; }

private:
    QByteArray m_buffer;
    bool m_malformed = false;
};

} // namespace pb::rpc