#include "catalog/ScanWorker.h"

#include "cache/Cache.h"
#include "plugin/PluginHost.h"
#include "provider/IProvider.h"
#include "provider/ProcessProvider.h"
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
        emit scanFinished({}, {}, {}, {});
        return;
    }

    m_host = new PluginHost;
    m_host->load(pluginDir, disabledPluginIds);
    m_scanner = new Scanner(m_host->providers());
    m_scanner->setTrustPolicy(TrustPolicy(trustedRoots));

    // Snapshot of loaded providers for the plugins screen.
    QVector<PluginInfo> plugins;
    for (const IProvider* p : m_host->providers()) {
        PluginInfo info;
        info.id = p->pluginId().value();
        info.isProcess = dynamic_cast<const ProcessProvider*>(p) != nullptr;
        // The provider states are reported live; enabled reflects the user's
        // disabled list (the host already skipped disabled manifests, but keep
        // the flag consistent with what the UI checkbox expects).
        info.enabled = !disabledPluginIds.contains(info.id);
        if (const auto* pp = dynamic_cast<const ProcessProvider*>(p))
            info.state = processProviderStateName(pp->state());
        else
            info.state = QStringLiteral("ready");
        plugins.append(std::move(info));
    }

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
    QVector<Installation> all;
    for (int i = 0; i < environments.size(); ++i) {
        const Environment& env = environments.at(i);
        emit progress(i, environments.size());
        IProvider* provider = findProvider(env.kind());
        if (!provider) {
            m_cache->updateEnvironmentStatus(env.id(), EnvStatus::Error);
            continue;
        }

        // A placeholder manager (executable present, no environments derived)
        // is shown in the UI but has no real package storage of its own: skip
        // persisting/populating it so it neither leaves a stale cache row nor
        // triggers a pointless vcpkg invocation.
        if (env.isPlaceholder())
            continue;

        m_cache->upsertEnvironment(env);
        const auto result = provider->list(env);
        if (!result.ok()) {
            m_cache->updateEnvironmentStatus(env.id(),
                result.error == ProviderError::Untrusted ? EnvStatus::Untrusted
                                                         : EnvStatus::Error);
        } else if (result.complete) {
            // A complete listing is authoritative: replace what we had.
            m_cache->replaceInstallations(env.id(), result.value.installations);
            all += result.value.installations;
        } else {
            // Honest "I don't know": keep the previously cached packages
            // (don't wipe them with an empty list) and flag the env.
            m_cache->updateEnvironmentStatus(env.id(), EnvStatus::Error);
        }
    }

    emit scanFinished(environments, tools, all, plugins);
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