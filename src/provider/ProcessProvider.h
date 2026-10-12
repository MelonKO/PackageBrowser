#pragma once

#include "plugin/PluginManifest.h"
#include "provider/IProvider.h"
#include "rpc/JsonRpcClient.h"

#include <QByteArray>
#include <QString>

class QJsonObject;
class QJsonValue;

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
    ProcessProvider(PluginManifest manifest, QObject* parent = nullptr);

    const PluginId& pluginId() const override;
    const ManagerKind& kind() const override;
    bool hasCapability(Capability c) const override;

    ProviderResult<QVector<ToolInstance>> detect() override;
    ProviderResult<QVector<Environment>> locate(const ToolInstance& tool) override;
    ProviderResult<QVector<Environment>> locateAt(const QString& scanRoot) override;
    ProviderResult<QString> fingerprint(const Environment& env) override;
    ProviderResult<QVector<Installation>> list(const Environment& env,
                                               bool withDependencies = false) override;

    QStringList projectMarkers() const override;
    QStringList skipDirs() const override;

private:
    // Send one request and block until its response or deadline. Returns true
    // when the plugin answered with a `result`; on RPC error or timeout fills
    // `*error` and returns false.
    bool call(const QString& method, const QJsonObject& params, QJsonObject* result,
              ProviderError* error);

    // Start the child lazily on the first call.
    bool ensureStarted();

    QJsonObject toolToJson(const ToolInstance& tool) const;
    QJsonObject envToJson(const Environment& env) const;

    PluginManifest m_manifest;
    pb::rpc::JsonRpcClient m_client;
    bool m_started = false;
    bool m_startFailed = false;
};