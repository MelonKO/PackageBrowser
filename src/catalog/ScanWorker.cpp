#include "catalog/ScanWorker.h"

#include "cache/Cache.h"
#include "plugin/PluginHost.h"
#include "provider/IProvider.h"
#include "scanner/Scanner.h"
#include "trust/TrustPolicy.h"

ScanWorker::ScanWorker() = default;

ScanWorker::~ScanWorker()
{
    delete m_scanner;
    delete m_host;
    delete m_cache;
}

void ScanWorker::startScan(const QString& dbPath,
                           const QString& pluginDir,
                           const QStringList& scanRoots,
                           const QStringList& disabledPluginIds,
                           const QStringList& trustedRoots,
                           int maxDepth)
{
    // Own everything on this thread. By constructing these here (not on the
    // main thread) the providers and the SQLite connection are created on the
    // worker thread and remain thread-affine.
    m_cache = new Cache(dbPath);
    if (!m_cache->open()) {
        delete m_cache;
        m_cache = nullptr;
        emit scanFinished({}, {}, {});
        return;
    }

    m_host = new PluginHost;
    m_host->load(pluginDir, disabledPluginIds);
    m_scanner = new Scanner(m_host->providers());
    m_scanner->setTrustPolicy(TrustPolicy(trustedRoots));

    // Global tools and their environments.
    QVector<Environment> globalEnvs;
    QVector<ToolInstance> tools;
    m_scanner->scanGlobals(globalEnvs, tools);

    // Project environments from configured roots.
    QVector<Environment> projectEnvs;
    if (!scanRoots.isEmpty())
        m_scanner->scanRoots(scanRoots, maxDepth, {}, 200, projectEnvs);

    const QVector<Environment> environments = globalEnvs + projectEnvs;

    // Persist, then list each environment's packages.
    emit progress(0, environments.size());
    for (const ToolInstance& tool : tools)
        m_cache->upsertTool(tool);
    int index = 0;
    QVector<Installation> all;
    for (const Environment& env : environments) {
        m_cache->upsertEnvironment(env);
        IProvider* provider = findProvider(env.kind());
        if (!provider) {
            m_cache->updateEnvironmentStatus(env.id(), EnvStatus::Error);
        } else {
            const auto result = provider->list(env);
            if (!result.ok()) {
                m_cache->updateEnvironmentStatus(env.id(),
                    result.error == ProviderError::Untrusted ? EnvStatus::Untrusted
                                                             : EnvStatus::Error);
            } else {
                m_cache->replaceInstallations(env.id(), result.value);
                all += result.value;
            }
        }
        emit progress(++index, environments.size());
    }

    emit scanFinished(environments, tools, all);
}

IProvider* ScanWorker::findProvider(const ManagerKind& kind)
{
    if (!m_host)
        return nullptr;
    for (IProvider* p : m_host->providers())
        if (p->kind() == kind)
            return p;
    return nullptr;
}