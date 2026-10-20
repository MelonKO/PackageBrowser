#pragma once

#include "catalog/PluginInfo.h"
#include "domain/DependencyEdge.h"
#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ToolInstance.h"

#include <QObject>
#include <QString>
#include <QStringList>

class Cache;
class PluginHost;
class Scanner;
class IProvider;

// Lives on a dedicated worker thread and owns everything the scan touches, so
// with one move both the providers (QProcess/JsonRpcClient) and the SQLite
// connection are created on the worker thread and stay thread-affine. The main
// thread never calls into core across threads: it asks startScan() and receives
// plain data snapshots via queued signals (Qt SQL cannot share a connection).
// See docs/UI на Qt.md ("Threads") and docs/Надёжность и безопасность.md.
class ScanWorker : public QObject {
    Q_OBJECT

public:
    ScanWorker();
    ~ScanWorker() override;

public slots:
    // Opens the cache, loads providers and runs the whole scan in one shot.
    void startScan(const QString& dbPath,
                   const QString& pluginDir,
                   const QStringList& scanRoots,
                   const QStringList& disabledPluginIds,
                   const QStringList& trustedRoots,
                   int maxDepth);

signals:
    void progress(int done, int total);
    // Full snapshot of the resulting state, delivered to the main thread.
    void scanFinished(const QVector<Environment>& environments,
                      const QVector<ToolInstance>& tools,
                      const QVector<Installation>& allInstallations,
                      const QVector<DependencyEdge>& allDependencies,
                      const QVector<PluginInfo>& plugins);

private:
    IProvider* findProvider(const ManagerKind& kind);

    Cache* m_cache = nullptr;
    PluginHost* m_host = nullptr;
    Scanner* m_scanner = nullptr;
};