#pragma once

#include "plugin/PluginManifest.h"
#include "provider/IProvider.h"
#include "rpc/JsonRpcClient.h"

#include <QByteArray>
#include <QString>

class QJsonObject;
class QJsonValue;

// Lifecycle of an external provider process, per docs/Плагины и JSON-RPC.md.
// A provider starts lazily (Ready on first request), and repeated failures
// (crash, hang, malformed output) escalate to Quarantined so a broken plugin
// is not retried on every scan. Quarantined needs a manual re-enable.
enum class ProcessProviderState {
    Ready,
    Failed,
    Quarantined
};

QString processProviderStateName(ProcessProviderState s);

// Adapts an external JSON-RPC provider process to the IProvider contract. The
// manifest declared `[provider] exec = [...]`; each read operation becomes one
// RPC request (detect / locate / locate_at / fingerprint / list) and the answer
// is awaited with a nested event loop, because the scanner and catalog call
// providers synchronously in the MVP. A hung plugin is cut by the request
// deadline, and its failure maps to a typed ProviderError instead of crashing.
// See docs/Плагины и JSON-RPC.md ("What is JSON-RPC") and "Contract rules".
class ProcessProvider : public QObject, public IProvider {
    Q_OBJECT

public:
    // Threshold of consecutive failures before a provider is quarantined.
    static constexpr int kQuarantineThreshold = 3;

    ProcessProvider(PluginManifest manifest, QObject* parent = nullptr);

    const PluginId& pluginId() const override;
    const ManagerKind& kind() const override;
    bool hasCapability(Capability c) const override;

    ProviderResult<QVector<ToolInstance>> detect() override;
    ProviderResult<QVector<Environment>> locate(const ToolInstance& tool) override;
    ProviderResult<QVector<Environment>> locateAt(const QString& scanRoot) override;
    ProviderResult<QString> fingerprint(const Environment& env) override;
    ProviderResult<ListResult> list(const Environment& env,
                                    bool withDependencies = false) override;

    QStringList projectMarkers() const override;
    QStringList skipDirs() const override;

    // Current lifecycle state (Ready / Failed / Quarantined).
    ProcessProviderState state() const { return m_state; }

    // Reset a quarantined provider so it is tried again on the next call.
    void reenable();

signals:
    void stateChanged(ProcessProviderState state);

private:
    // Resolve an executable name on PATH, skipping files that QProcess cannot
    // spawn (e.g. 0-byte Windows Store shims), so a provider's interpreter is
    // actually started.
    static QString resolveOnPath(const QString& program);

    // Send one request and block until its response or deadline. Returns true
    // when the plugin answered with a `result`; on RPC error or timeout fills
    // `*error` and returns false.
    bool call(const QString& method, const QJsonObject& params, QJsonObject* result,
              ProviderError* error);

    // Start the child lazily on the first call.
    bool ensureStarted();

    // Record a failed request; escalate Failed→Quarantined past the threshold.
    void recordFailure();
    void setState(ProcessProviderState s);

    QJsonObject toolToJson(const ToolInstance& tool) const;
    QJsonObject envToJson(const Environment& env) const;

    PluginManifest m_manifest;
    pb::rpc::JsonRpcClient m_client;
    bool m_started = false;
    bool m_startFailed = false;
    ProcessProviderState m_state = ProcessProviderState::Ready;
    int m_consecutiveFailures = 0;
};