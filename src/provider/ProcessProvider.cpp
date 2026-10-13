#include "provider/ProcessProvider.h"

#include "domain/EnvironmentId.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {

QString scopeName(Scope s)
{
    switch (s) {
    case Scope::System: return QStringLiteral("system");
    case Scope::User: return QStringLiteral("user");
    case Scope::Project: return QStringLiteral("project");
    }
    return QString();
}

Scope scopeFromName(const QString& s)
{
    if (s == QLatin1String("system")) return Scope::System;
    if (s == QLatin1String("user")) return Scope::User;
    if (s == QLatin1String("project")) return Scope::Project;
    return Scope::Project;
}

// Map a JSON-RPC error back to the typed provider error. The plugin encodes its
// internal failure kind in `data.kind` (see docs/Плагины и JSON-RPC.md).
ProviderError errorFromRpc(const QJsonObject& error)
{
    const QJsonObject data = error.value(QStringLiteral("data")).toObject();
    const QString kind = data.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("ToolNotFound")) return ProviderError::ToolNotFound;
    if (kind == QLatin1String("EnvBroken")) return ProviderError::EnvBroken;
    if (kind == QLatin1String("Timeout")) return ProviderError::Timeout;
    if (kind == QLatin1String("ParseFailed")) return ProviderError::ParseFailed;
    if (kind == QLatin1String("Untrusted")) return ProviderError::Untrusted;
    if (kind == QLatin1String("Unsupported")) return ProviderError::Unsupported;
    return ProviderError::EnvBroken;
}

} // namespace

QString processProviderStateName(ProcessProviderState s)
{
    switch (s) {
    case ProcessProviderState::Ready: return QStringLiteral("ready");
    case ProcessProviderState::Failed: return QStringLiteral("failed");
    case ProcessProviderState::Quarantined: return QStringLiteral("quarantined");
    }
    return QStringLiteral("unknown");
}

ProcessProvider::ProcessProvider(PluginManifest manifest, QObject* parent)
    : QObject(parent)
    , m_manifest(std::move(manifest))
{
}

const PluginId& ProcessProvider::pluginId() const { return m_manifest.pluginId(); }

const ManagerKind& ProcessProvider::kind() const { return m_manifest.kind(); }

bool ProcessProvider::hasCapability(Capability c) const
{
    return m_manifest.hasCapability(c);
}

ProviderResult<QVector<ToolInstance>> ProcessProvider::detect()
{
    ProviderResult<QVector<ToolInstance>> result;
    QJsonObject rpcResult;
    ProviderError error;
    if (!call(QStringLiteral("detect"), {}, &rpcResult, &error)) {
        result.error = error;
        return result;
    }
    const QJsonArray arr = rpcResult.value(QStringLiteral("tools")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString path = o.value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            continue;
        ToolInstance tool(
            kind(),
            QStringLiteral("pp_") + QString::fromUtf8(
                QCryptographicHash::hash((m_manifest.pluginId().value() + path).toUtf8(),
                                         QCryptographicHash::Sha1).toHex().left(12)),
            path,
            o.value(QStringLiteral("version")).toString());
        result.value.append(std::move(tool));
    }
    if (result.value.isEmpty())
        result.error = ProviderError::ToolNotFound;
    return result;
}

ProviderResult<QVector<Environment>> ProcessProvider::locate(const ToolInstance& tool)
{
    ProviderResult<QVector<Environment>> result;
    QJsonObject params;
    params.insert(QStringLiteral("tool"), toolToJson(tool));
    QJsonObject rpcResult;
    ProviderError error;
    if (!call(QStringLiteral("locate"), params, &rpcResult, &error)) {
        result.error = error;
        return result;
    }
    const QJsonArray arr = rpcResult.value(QStringLiteral("environments")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString rootStr = o.value(QStringLiteral("root")).toString();
        if (rootStr.isEmpty())
            continue;
        const QString envId = o.value(QStringLiteral("id")).toString();
        Environment env(
            envId.isEmpty()
                ? EnvironmentId::fromHashed(m_manifest.pluginId().value(), rootStr)
                : EnvironmentId::fromValue(envId),
            kind(),
            scopeFromName(o.value(QStringLiteral("scope")).toString()),
            rootStr,
            o.value(QStringLiteral("tool_id")).toString(),
            o.value(QStringLiteral("inherits_from")).toString(),
            static_cast<EnvStatus>(o.value(QStringLiteral("status")).toInt()));
        result.value.append(std::move(env));
    }
    return result;
}

ProviderResult<QVector<Environment>> ProcessProvider::locateAt(const QString& scanRoot)
{
    ProviderResult<QVector<Environment>> result;
    QJsonObject params;
    params.insert(QStringLiteral("root"), scanRoot);
    QJsonObject rpcResult;
    ProviderError error;
    if (!call(QStringLiteral("locate_at"), params, &rpcResult, &error)) {
        result.error = error;
        return result;
    }
    const QJsonArray arr = rpcResult.value(QStringLiteral("environments")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString rootStr = o.value(QStringLiteral("root")).toString();
        if (rootStr.isEmpty())
            continue;
        Environment env(
            EnvironmentId::fromHashed(m_manifest.pluginId().value(), rootStr),
            kind(),
            scopeFromName(o.value(QStringLiteral("scope")).toString()),
            rootStr,
            o.value(QStringLiteral("tool_id")).toString(),
            QString(),
            EnvStatus::Untrusted);
        result.value.append(std::move(env));
    }
    return result;
}

ProviderResult<QString> ProcessProvider::fingerprint(const Environment& env)
{
    ProviderResult<QString> result;
    QJsonObject params;
    params.insert(QStringLiteral("environment"), envToJson(env));
    QJsonObject rpcResult;
    ProviderError error;
    if (!call(QStringLiteral("fingerprint"), params, &rpcResult, &error)) {
        result.error = error;
        return result;
    }
    result.value = rpcResult.value(QStringLiteral("fingerprint")).toString();
    return result;
}

ProviderResult<QVector<Installation>> ProcessProvider::list(const Environment& env,
                                                            bool withDependencies)
{
    ProviderResult<QVector<Installation>> result;
    QJsonObject params;
    params.insert(QStringLiteral("environment"), envToJson(env));
    params.insert(QStringLiteral("with_dependencies"), withDependencies);
    QJsonObject rpcResult;
    ProviderError error;
    if (!call(QStringLiteral("list"), params, &rpcResult, &error)) {
        result.error = error;
        return result;
    }
    result.complete = rpcResult.value(QStringLiteral("complete")).toBool(true);
    const QJsonArray arr = rpcResult.value(QStringLiteral("installations")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        result.value.append(
            Installation(env.id(), name, name,
                         o.value(QStringLiteral("version")).toString(),
                         o.value(QStringLiteral("location")).toString()));
    }
    return result;
}

QStringList ProcessProvider::projectMarkers() const { return {}; }
QStringList ProcessProvider::skipDirs() const { return {}; }

bool ProcessProvider::call(const QString& method, const QJsonObject& params, QJsonObject* result,
                           ProviderError* error)
{
    if (m_state == ProcessProviderState::Quarantined) {
        *error = ProviderError::Unsupported;
        return false;
    }

    if (!ensureStarted()) {
        *error = ProviderError::ToolNotFound;
        recordFailure();
        return false;
    }

    bool done = false;
    bool ok = false;
    QJsonObject gotResult;
    QJsonObject gotError;
    const bool accepted = m_client.request(
        params, method,
        [&](const QJsonObject& r, const QJsonObject& e) {
            gotResult = r;
            gotError = e;
            ok = e.isEmpty();
            done = true;
        },
        15000);
    if (!accepted) {
        *error = ProviderError::ToolNotFound;
        recordFailure();
        return false;
    }

    // Await the response without leaving the event loop starved.
    while (!done)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    if (!ok) {
        *error = errorFromRpc(gotError);
        recordFailure();
        return false;
    }
    // A live round-trip clears the failure streak.
    m_consecutiveFailures = 0;
    setState(ProcessProviderState::Ready);
    *result = gotResult;
    return true;
}

void ProcessProvider::recordFailure()
{
    ++m_consecutiveFailures;
    setState(ProcessProviderState::Failed);
    if (m_consecutiveFailures >= kQuarantineThreshold) {
        setState(ProcessProviderState::Quarantined);
        m_client.stop();
    }
}

void ProcessProvider::setState(ProcessProviderState s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void ProcessProvider::reenable()
{
    m_consecutiveFailures = 0;
    m_startFailed = false;
    m_started = false;
    setState(ProcessProviderState::Ready);
}

bool ProcessProvider::ensureStarted()
{
    if (m_started)
        return true;
    if (m_startFailed)
        return false;

    QString program = m_manifest.processProvider().program;
    QStringList args = m_manifest.processProvider().args;
    // A bare provider script path is resolved against the plugin's directory.
    const QDir pluginDir(QFileInfo(m_manifest.sourcePath()).dir());
    if (!program.contains(QLatin1Char('/')) && !program.contains(QLatin1Char('\\'))) {
        const QString local = pluginDir.filePath(program);
        if (QFileInfo::exists(local))
            program = QDir(local).canonicalPath();
    }

    m_started = m_client.start(program, args);
    if (!m_started)
        m_startFailed = true;
    return m_started;
}

QJsonObject ProcessProvider::toolToJson(const ToolInstance& tool) const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), tool.id());
    o.insert(QStringLiteral("path"), tool.path());
    o.insert(QStringLiteral("version"), tool.version());
    o.insert(QStringLiteral("provided_by_env"), tool.providedByEnv());
    return o;
}

QJsonObject ProcessProvider::envToJson(const Environment& env) const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), env.id().value());
    o.insert(QStringLiteral("root"), env.root());
    o.insert(QStringLiteral("scope"), scopeName(env.scope()));
    o.insert(QStringLiteral("tool_id"), env.toolId());
    o.insert(QStringLiteral("inherits_from"), env.inheritsFrom());
    return o;
}