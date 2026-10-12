#include "catalog/CatalogService.h"

#include "cache/Cache.h"
#include "domain/EnvironmentId.h"
#include "normalization/NameNormalizer.h"
#include "provider/IProvider.h"
#include "trust/TrustPolicy.h"

#include <QHash>

CatalogService::CatalogService(QObject* parent)
    : QObject(parent)
    , m_settings()
{
}

CatalogService::~CatalogService()
{
    delete m_cache;
}

bool CatalogService::initialize(const QString& dbPath, const QString& pluginDir)
{
    m_cache = new Cache(dbPath);
    if (!m_cache->open()) {
        delete m_cache;
        m_cache = nullptr;
        return false;
    }

    // Load providers (built-in pip + one per valid plugin.toml), then build the
    // scanner over them. Trust roots are shared across every provider; disabled
    // plugins are skipped entirely.
    m_host.load(pluginDir, m_settings.disabledPluginIds());
    m_scanner = Scanner(m_host.providers());
    m_scanner.setTrustPolicy(TrustPolicy(m_settings.trustedRoots()));
    return true;
}

QVector<Environment> CatalogService::listEnvironments() const
{
    return m_environments;
}

QVector<Installation> CatalogService::listInstallations(const EnvironmentId& envId) const
{
    if (!m_cache)
        return {};
    return m_cache->installations(envId);
}

QVector<Installation> CatalogService::findPackage(const QString& name) const
{
    if (!m_cache)
        return {};

    // Normalization rules belong to the ecosystem of each environment, so the
    // query is normalized per-environment (Python PEP 503, JS/vcpkg lowercase).
    QVector<Installation> out;
    const QVector<Installation> all = m_cache->allInstallations();
    // Cache the ecosystem lookup: envId -> ecosystem, so the scan is O(n).
    QHash<QString, Ecosystem> ecoForEnv;
    for (const Environment& env : m_environments)
        ecoForEnv.insert(env.id().value(), env.kind().ecosystem());

    for (const Installation& inst : all) {
        const Ecosystem eco = ecoForEnv.value(
            inst.envId().value(), Ecosystem::Python); // default: Python (legacy)
        if (inst.nameNorm() == normalizeName(eco, name))
            out.append(inst);
    }
    return out;
}

QVector<Installation> CatalogService::allPackages() const
{
    if (!m_cache)
        return {};
    return m_cache->allInstallations();
}

void CatalogService::rescan()
{
    if (!m_cache)
        return;

    // 1. Global tools and their environments.
    QVector<Environment> globalEnvs;
    QVector<ToolInstance> globalTools;
    m_scanner.scanGlobals(globalEnvs, globalTools);

    // 2. Project environments from configured roots.
    QVector<Environment> projectEnvs;
    const QStringList roots = m_settings.scanRoots();
    if (!roots.isEmpty())
        m_scanner.scanRoots(roots, m_settings.scanMaxDepth(), {}, 200, projectEnvs);

    m_tools = globalTools;
    m_environments = globalEnvs + projectEnvs;

    // 3. Persist tools, environments and their packages, then notify.
    emit scanProgress(0, m_environments.size());
    for (const ToolInstance& tool : m_tools)
        m_cache->upsertTool(tool);
    for (int i = 0; i < m_environments.size(); ++i) {
        const Environment& env = m_environments[i];
        m_cache->upsertEnvironment(env);
        normalizeAndStoreInstallations(env);
        emit scanProgress(i + 1, m_environments.size());
    }

    emit environmentsChanged();
    emit scanFinished();
}

void CatalogService::normalizeAndStoreInstallations(const Environment& env)
{
    if (!m_cache)
        return;

    // Find the provider that owns this environment's manager kind.
    IProvider* provider = nullptr;
    for (IProvider* p : m_host.providers()) {
        if (p->kind() == env.kind()) {
            provider = p;
            break;
        }
    }
    if (!provider) {
        m_cache->updateEnvironmentStatus(env.id(), EnvStatus::Error);
        return;
    }

    // If the environment is untrusted, the provider reads files; otherwise it
    // may run the manager. Either way it returns the package list to cache.
    const auto result = provider->list(env);
    if (!result.ok()) {
        // An untrusted project without a file-based fallback is refused, not
        // broken: keep the meaningful Untrusted status instead of Error.
        const EnvStatus status = result.error == ProviderError::Untrusted
            ? EnvStatus::Untrusted
            : EnvStatus::Error;
        m_cache->updateEnvironmentStatus(env.id(), status);
        return;
    }
    m_cache->replaceInstallations(env.id(), result.value);
    emit installationsChanged(env.id().value());
}