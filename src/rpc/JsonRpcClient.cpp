#include "rpc/JsonRpcClient.h"

#include "rpc/ContentLengthFraming.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QProcess>
#include <QTimer>

#include <functional>

namespace pb::rpc {

JsonRpcClient::JsonRpcClient(QObject* parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process,
            &QProcess::readyReadStandardOutput, this, &JsonRpcClient::onReadyRead);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JsonRpcClient::onProcessFinished);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(m_resolutionMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, &JsonRpcClient::tickTimeouts);
}

JsonRpcClient::~JsonRpcClient()
{
    failPending(RpcError::NotStarted);
}

bool JsonRpcClient::start(const QString& program, const QStringList& args)
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start(program, args);
    return m_process->waitForStarted(3000);
}

bool JsonRpcClient::request(const QJsonObject& params, QString method, ResultCallback cb,
                            int timeoutMs)
{
    if (!isRunning())
        return false;

    const int id = m_nextId++;
    m_pending.insert(id, { std::move(cb), timeoutMs, double(QDateTime::currentMSecsSinceEpoch()) });

    QJsonObject frame;
    frame.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    frame.insert(QStringLiteral("id"), id);
    frame.insert(QStringLiteral("method"), method);
    frame.insert(QStringLiteral("params"), params);
    const QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    m_writeBuffer += frameMessage(payload);

    if (!m_timeoutTimer->isActive())
        m_timeoutTimer->start();
    QTimer::singleShot(0, this, &JsonRpcClient::flushWriteBuffer);
    return true;
}

void JsonRpcClient::tickTimeouts()
{
    if (m_pending.isEmpty()) {
        m_timeoutTimer->stop();
        return;
    }
    const double now = double(QDateTime::currentMSecsSinceEpoch());
    QList<int> expired;
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        if (it.value().deadlineMs > 0 && now - it.value().startedAtMs >= it.value().deadlineMs)
            expired.append(it.key());
    }
    for (const int id : expired) {
        const Pending pending = m_pending.take(id);
        m_lastError = RpcError::Timeout;
        if (pending.callback)
            pending.callback(QJsonObject(), QJsonObject());
    }
}

void JsonRpcClient::flushWriteBuffer()
{
    if (!isRunning())
        return;
    const qint64 written = m_process->write(m_writeBuffer);
    if (written < 0)
        failPending(RpcError::WriteFailed);
    else
        m_writeBuffer.clear();
}

void JsonRpcClient::cancel(int id)
{
    QJsonObject params;
    params.insert(QStringLiteral("id"), id);
    QJsonObject frame;
    frame.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    frame.insert(QStringLiteral("method"), QStringLiteral("$/cancel"));
    frame.insert(QStringLiteral("params"), params);
    const QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    if (isRunning())
        m_process->write(frameMessage(payload));
}

void JsonRpcClient::stop(int graceMs)
{
    if (!isRunning())
        return;
    // Ask the child to shut down cleanly, then kill if it ignores us. Pending
    // requests are dropped; the callback already returned via a shutdown path.
    m_process->closeWriteChannel();
    if (m_process->waitForFinished(graceMs))
        return;
    m_process->kill();
    m_process->waitForFinished(graceMs);
    failPending(RpcError::NotStarted);
}

void JsonRpcClient::setNotificationHandler(std::function<void(const QJsonObject&)> handler)
{
    m_notificationHandler = std::move(handler);
}

bool JsonRpcClient::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void JsonRpcClient::onReadyRead()
{
    processInput(m_process->readAllStandardOutput());
}

void JsonRpcClient::processInput(const QByteArray& bytes)
{
    const QList<QByteArray> payloads = m_decoder.push(bytes);
    if (m_decoder.malformed()) {
        // Garbage on stdout: the channel is not a JSON-RPC peer any more.
        failPending(RpcError::ParseFailed);
        return;
    }
    for (const QByteArray& payload : payloads) {
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject())
            continue;
        const QJsonObject msg = doc.object();

        if (!msg.contains(QStringLiteral("id"))) {
            // Notification: no id, deliver params to the handler.
            if (m_notificationHandler)
                m_notificationHandler(msg.value(QStringLiteral("params")).toObject());
            continue;
        }
        const int id = msg.value(QStringLiteral("id")).toInt(-1);
        if (id < 0 || !m_pending.contains(id))
            continue;
        const Pending pending = m_pending.take(id);
        const QJsonObject result = msg.value(QStringLiteral("result")).toObject();
        const QJsonObject error = msg.value(QStringLiteral("error")).toObject();
        if (pending.callback)
            pending.callback(result, error);
    }
}

void JsonRpcClient::onProcessFinished(int /*exitCode*/, int /*status*/)
{
    // A child that answers then exits may deliver `finished` before the pipe
    // has flushed the last bytes to us, so drain once now, give the OS a short
    // window to push any remaining output, then drain again before failing the
    // requests that are genuinely unresolved.
    processInput(m_process->readAllStandardOutput());
    if (!m_pending.isEmpty() && m_process->waitForReadyRead(200))
        processInput(m_process->readAllStandardOutput());
    if (!m_pending.isEmpty())
        failPending(RpcError::NotStarted);
    emit processExited(m_process->exitCode());
}

void JsonRpcClient::failPending(RpcError error)
{
    m_lastError = error;
    const auto pending = m_pending;
    m_pending.clear();
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
        if (it.value().callback)
            it.value().callback(QJsonObject(), QJsonObject());
    }
    m_writeBuffer.clear();
}

} // namespace pb::rpc