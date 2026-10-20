#include "catalog/CatalogService.h"

#include "catalog/ScanWorker.h"
#include "cache/Cache.h"
#include "domain/EnvironmentId.h"
#include "normalization/NameNormalizer.h"

#include <QHash>
#include <QThread>

CatalogService::CatalogService(QObject* parent)
    : QObject(parent)
    , m_settings()
{
}

CatalogService::~CatalogService()
{
    if (m_thread) {
        // Stop the worker and let it delete itself on its own thread, so its
        // Cache (SQLite connection) is torn down with the correct thread
        // affinity. m_worker is otherwise owned by the worker thread through
        // the finished->deleteLater connection established in initialize().
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
}

bool CatalogService::initialize(const QString& dbPath, const QString& pluginDir)
{
    // Remember where the worker should open the cache and load providers.
    m_dbPath = dbPath;
    m_pluginDir = pluginDir;

    m_thread = new QThread(this);
    m_worker = new ScanWorker;
    m_worker->moveToThread(m_thread);

    // The worker deletes itself on its own thread when the thread finishes, so
    // its Cache (SQLite connection) is torn down with the right affinity.
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Worker signals delivered to us as queued connections (cross-thread).
    connect(m_worker, &ScanWorker::progress, this, &CatalogService::scanProgress);
    connect(m_worker, &ScanWorker::cacheLoaded, this, &CatalogService::onCacheLoaded);
    connect(m_worker, &ScanWorker::scanFinished, this, &CatalogService::onWorkerResult);

    m_thread->start();
    return true;
}

QVector<Environment> CatalogService::listEnvironments() const
{
    return m_environments;
}

QVector<Installation> CatalogService::listInstallations(const EnvironmentId& envId) const
{
    QVector<Installation> out;
    for (const Installation& inst : m_allInstallations)
        if (inst.envId() == envId)
            out.append(inst);
    return out;
}

QVector<DependencyEdge> CatalogService::dependencies(const EnvironmentId& envId) const
{
    QVector<DependencyEdge> out;
    for (const DependencyEdge& e : m_allDependencies)
        if (e.envId() == envId)
            out.append(e);
    return out;
}

QVector<Installation> CatalogService::findPackage(const QString& name) const
{
    // Normalization rules belong to the ecosystem of each environment, so the
    // query is normalized per-environment (Python PEP 503, JS/vcpkg lowercase).
    QVector<Installation> out;
    QHash<QString, Ecosystem> ecoForEnv;
    for (const Environment& env : m_environments)
        ecoForEnv.insert(env.id().value(), env.kind().ecosystem());

    for (const Installation& inst : m_allInstallations) {
        const Ecosystem eco = ecoForEnv.value(
            inst.envId().value(), Ecosystem::Python); // default: Python (legacy)
        if (inst.nameNorm() == normalizeName(eco, name))
            out.append(inst);
    }
    return out;
}

QVector<Installation> CatalogService::allPackages() const
{
    return m_allInstallations;
}

void CatalogService::rescan()
{
    // Ask the worker to run the whole scan with the current settings snapshot.
    QMetaObject::invokeMethod(
        m_worker, [this]() {
            m_worker->startScan(m_dbPath, m_pluginDir,
                                m_settings.scanRoots(),
                                m_settings.disabledPluginIds(),
                                m_settings.trustedRoots(),
                                m_settings.scanMaxDepth());
        },
        Qt::QueuedConnection);
}

void CatalogService::onCacheLoaded(const QVector<Environment>& environments,
                                   const QVector<ToolInstance>& tools,
                                   const QVector<Installation>& allInstallations,
                                   const QVector<DependencyEdge>& allDependencies)
{
    // Paint the cached snapshot immediately (stale-while-revalidate); the full
    // scan that follows will replace it via onWorkerResult. The plugins list is
    // not known until providers are loaded, so leave it untouched here.
    m_environments = environments;
    m_tools = tools;
    m_allInstallations = allInstallations;
    m_allDependencies = allDependencies;

    emit environmentsChanged();
    emit installationsChanged(QString());
    emit cacheLoaded();
}

void CatalogService::onWorkerResult(const QVector<Environment>& environments,
                                    const QVector<ToolInstance>& tools,
                                    const QVector<Installation>& allInstallations,
                                    const QVector<DependencyEdge>& allDependencies,
                                    const QVector<PluginInfo>& plugins)
{
    m_environments = environments;
    m_tools = tools;
    m_allInstallations = allInstallations;
    m_allDependencies = allDependencies;
    m_plugins = plugins;

    emit environmentsChanged();
    emit installationsChanged(QString());
    emit scanFinished();
    emit pluginsChanged();
}