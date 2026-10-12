#pragma once

#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"
#include "plugin/PluginHost.h"
#include "scanner/Scanner.h"
#include "settings/AppSettings.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class Cache;

// Facade of the core and the single contact point for the UI. Reads come from
// the cache. Scanning is synchronous on the calling thread for stage 1 (the
// UI defers it after startup and reacts to scanProgress); a fingerprint-based
// stale-while-revalidate refresh is planned for a later stage.
// See docs/UI на Qt.md and docs/Хранилище и кэш.md.
class CatalogService : public QObject {
    Q_OBJECT

public:
    explicit CatalogService(QObject* parent = nullptr);
    ~CatalogService() override;

    CatalogService(const CatalogService&) = delete;
    CatalogService& operator=(const CatalogService&) = delete;

    // One-time initialization: open the cache, load settings, load plugins.
    bool initialize(const QString& dbPath, const QString& pluginDir);

    // Public read API (UI calls these and connects to signals).
    QVector<Environment> listEnvironments() const;
    QVector<Installation> listInstallations(const EnvironmentId& envId) const;
    QVector<Installation> findPackage(const QString& name) const;
    // Flat list of every installed package across all environments.
    QVector<Installation> allPackages() const;

    // Runs a scan synchronously on the calling (UI) thread. In later stages
    // the UI moves this call to a worker thread and reacts to scanProgress;
    // progress is emitted here so the signal is exercised and ready.
    void rescan();

    // Settings passthrough.
    const AppSettings& settings() const { return m_settings; }
    AppSettings& settings() { return m_settings; }

    // The plugin host owns the live providers; the UI uses it for the plugins
    // screen (list + enable/disable).
    PluginHost* pluginHost() { return &m_host; }

signals:
    void environmentsChanged();
    void installationsChanged(const QString& envId);
    void scanProgress(int done, int total);
    void scanFinished();

private:
    void normalizeAndStoreInstallations(const Environment& env);

    AppSettings m_settings;
    Cache* m_cache = nullptr;
    PluginHost m_host;
    Scanner m_scanner{ QVector<IProvider*>{} };

    QVector<Environment> m_environments; // in-memory view used by the UI
    QVector<ToolInstance> m_tools;
};