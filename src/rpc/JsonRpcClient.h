#pragma once

#include "rpc/ContentLengthFraming.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;
class QTimer;

namespace pb::rpc {

using ResultCallback = std::function<void(const QJsonObject& result, const QJsonObject& error)>;

enum class RpcError {
    None,
    NotStarted,
    WriteFailed,
    ParseFailed,
    Timeout
};

// An asynchronous JSON-RPC 2.0 client over a child process's stdin/stdout,
// framed with Content-Length. The host sends a request with a growing id and a
// callback; when the matching response arrives the callback runs. The child may
// also emit notifications (no id), which are delivered to a handler set by the
// caller. A hung child is killed after a per-request deadline, so a slow or
// wedged provider never freezes the UI.
// See docs/Плагины и JSON-RPC.md and docs/Надёжность и безопасность.md.
class JsonRpcClient : public QObject {
    Q_OBJECT

public:
    explicit JsonRpcClient(QObject* parent = nullptr);
    ~JsonRpcClient() override;

    JsonRpcClient(const JsonRpcClient&) = delete;
    JsonRpcClient& operator=(const JsonRpcClient&) = delete;

    // Start the child program. Returns false if it could not be started.
    bool start(const QString& program, const QStringList& args);

    // Send a request; cb is invoked with (result, error) when the response
    // arrives. Both are valid JSON values with matching "result"/"error"
    // members; exactly one of them is set to a non-null object on success.
    // Returns false when the channel cannot accept the request.
    bool request(const QJsonObject& params, QString method, ResultCallback cb,
                 int timeoutMs = 15000);

    // Ask the child to cancel a previously issued request (method "$/cancel").
    void cancel(int id);

    // Close the channel: terminate the child (escalating to kill) and drop any
    // still-pending requests. Used when the host is done with this provider.
    void stop(int graceMs = 2000);

    // Handler for notifications (messages without an id), e.g. "progress".
    void setNotificationHandler(
        std::function<void(const QJsonObject& params)> handler);

    // The last terminal error, if any.
    RpcError lastError() const { return m_lastError; }

    bool isRunning() const;

signals:
    // The child exited on its own (crash or clean shutdown) before a request
    // finished; the channel must be torn down.
    void processExited(int exitCode);

private:
    void onReadyRead();
    void onProcessFinished(int exitCode, int status);
    void processInput(const QByteArray& bytes);
    void failPending(RpcError error);
    void flushWriteBuffer();
    void tickTimeouts();

    QProcess* m_process = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    FrameDecoder m_decoder;
    QByteArray m_writeBuffer;
    int m_nextId = 1;
    struct Pending {
        ResultCallback callback;
        int deadlineMs;
        double startedAtMs;
    };
    QHash<int, Pending> m_pending;
    std::function<void(const QJsonObject& params)> m_notificationHandler;
    RpcError m_lastError = RpcError::None;
    int m_resolutionMs = 250;
};

} // namespace pb::rpc