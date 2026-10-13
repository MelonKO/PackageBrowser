#pragma once

#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"
#include "plugin/PluginHost.h"
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
    QVector<Installation> findPackage(const QString& name) const;
    // Flat list of every installed package across all environments.
    QVector<Installation> allPackages() const;

    // Starts an asynchronous scan on the worker thread. Notifies via
    // scanProgress / environmentsChanged / scanFinished.
    void rescan();

    // Settings passthrough.
    const AppSettings& settings() const { return m_settings; }
    AppSettings& settings() { return m_settings; }

    // The plugin host on the main thread lists the loaded plugins for the
    // plugins screen (enable/disable); it never runs provider processes here.
    PluginHost* pluginHost() { return &m_host; }

signals:
    void environmentsChanged();
    void installationsChanged(const QString& envId);
    void scanProgress(int done, int total);
    void scanFinished();

private slots:
    void onWorkerResult(const QVector<Environment>& environments,
                        const QVector<ToolInstance>& tools,
                        const QVector<Installation>& allInstallations);

private:
    AppSettings m_settings;
    PluginHost m_host;        // main-thread host: plugin listing only
    QThread* m_thread = nullptr;
    ScanWorker* m_worker = nullptr;
    QString m_dbPath;
    QString m_pluginDir;

    // Latest snapshot; the UI reads from these, never from core objects.
    QVector<Environment> m_environments;
    QVector<ToolInstance> m_tools;
    QVector<Installation> m_allInstallations;
};