#pragma once

#include "catalog/PluginInfo.h"
#include "domain/DependencyEdge.h"
#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"
#include "settings/AppSettings.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class QThread;
class ScanWorker;
class Cache;

// Facade of the core and the single contact point for the UI. Scanning runs on
// a dedicated worker thread (owning its own providers, cache and SQLite
// connection); results are delivered back as plain data snapshots via queued
// signals, so the UI thread never touches core objects across threads. Reads
// come from the latest snapshot, never from a live cross-thread object.
// See docs/UI на Qt.md ("Threads") and docs/Хранилище и кэш.md.
class CatalogService : public QObject {
    Q_OBJECT

public:
    explicit CatalogService(QObject* parent = nullptr);
    ~CatalogService() override;

    CatalogService(const CatalogService&) = delete;
    CatalogService& operator=(const CatalogService&) = delete;

    // One-time initialization: open the worker-thread plumbing. Actually opens
    // nothing on the main thread; the worker opens its cache and loads plugins.
    bool initialize(const QString& dbPath, const QString& pluginDir);

    // Public read API (UI calls these; they read the latest snapshot).
    QVector<Environment> listEnvironments() const;
    QVector<Installation> listInstallations(const EnvironmentId& envId) const;
    QVector<DependencyEdge> dependencies(const EnvironmentId& envId) const;
    QVector<Installation> findPackage(const QString& name) const;
    // Flat list of every installed package across all environments.
    QVector<Installation> allPackages() const;

    // Starts an asynchronous scan on the worker thread. Notifies via
    // scanProgress / environmentsChanged / scanFinished.
    void rescan();

    // Settings passthrough.
    const AppSettings& settings() const { return m_settings; }
    AppSettings& settings() { return m_settings; }

    // Plugins screen data: latest snapshot of loaded providers.
    QVector<PluginInfo> plugins() const { return m_plugins; }

signals:
    // Emitted when the cached snapshot is painted on startup, before the
    // background scan finishes (stale-while-revalidate).
    void cacheLoaded();
    void environmentsChanged();
    void installationsChanged(const QString& envId);
    void scanProgress(int done, int total);
    void scanFinished();
    void pluginsChanged();

private slots:
    void onCacheLoaded(const QVector<Environment>& environments,
                       const QVector<ToolInstance>& tools,
                       const QVector<Installation>& allInstallations,
                       const QVector<DependencyEdge>& allDependencies);
    void onWorkerResult(const QVector<Environment>& environments,
                        const QVector<ToolInstance>& tools,
                        const QVector<Installation>& allInstallations,
                        const QVector<DependencyEdge>& allDependencies,
                        const QVector<PluginInfo>& plugins);

private:
    AppSettings m_settings;
    QThread* m_thread = nullptr;
    ScanWorker* m_worker = nullptr;
    QString m_dbPath;
    QString m_pluginDir;

    // Latest snapshots; the UI reads from these, never from core objects.
    QVector<Environment> m_environments;
    QVector<ToolInstance> m_tools;
    QVector<Installation> m_allInstallations;
    QVector<DependencyEdge> m_allDependencies;
    QVector<PluginInfo> m_plugins;
};