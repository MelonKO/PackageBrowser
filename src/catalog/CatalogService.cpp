#include "catalog/CatalogService.h"

#include "cache/Cache.h"
#include "domain/EnvironmentId.h"
#include "normalization/Pep503Normalizer.h"

CatalogService::CatalogService(QObject* parent)
    : QObject(parent)
    , m_settings()
    , m_pip()
    , m_scanner(&m_pip)
{
}

CatalogService::~CatalogService()
{
    delete m_cache;
}

bool CatalogService::initialize(const QString& dbPath)
{
    m_cache = new Cache(dbPath);
    if (!m_cache->open()) {
        delete m_cache;
        m_cache = nullptr;
        return false;
    }

    m_pip.setTrustedRoots(m_settings.trustedRoots());
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
    const QString norm = Pep503Normalizer::normalize(name);
    QVector<Installation> out;
    for (const Installation& inst : m_cache->allInstallations())
        if (inst.nameNorm() == norm)
            out.append(inst);
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
    for (const ToolInstance& tool : m_tools)
        m_cache->upsertTool(tool);
    for (const Environment& env : m_environments) {
        m_cache->upsertEnvironment(env);
        normalizeAndStoreInstallations(env);
    }

    emit environmentsChanged();
    emit scanFinished();
}

void CatalogService::normalizeAndStoreInstallations(const Environment& env)
{
    if (!m_cache)
        return;

    // If the environment is untrusted, the provider reads files; otherwise it
    // may run pip. Either way it returns the package list to cache.
    const auto result = m_pip.list(env);
    if (!result.ok()) {
        m_cache->updateEnvironmentStatus(env.id(), EnvStatus::Error);
        return;
    }
    m_cache->replaceInstallations(env.id(), result.value);
    emit installationsChanged(env.id().value());
}