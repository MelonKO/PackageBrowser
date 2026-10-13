#include "scanner/Scanner.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

Scanner::Scanner(const QVector<IProvider*>& providers)
    : m_providers(providers)
{
}

void Scanner::setTrustPolicy(const TrustPolicy& policy)
{
    m_trust = policy;
}

void Scanner::scanGlobals(QVector<Environment>& envs, QVector<ToolInstance>& tools)
{
    for (IProvider* provider : m_providers) {
        const auto detect = provider->detect();
        if (!detect.ok())
            continue;

        int located = 0;
        for (const ToolInstance& tool : detect.value) {
            tools.append(tool);
            const auto loc = provider->locate(tool);
            if (loc.ok()) {
                envs.append(loc.value);
                located += loc.value.size();
            }
        }

        // A manager whose executable is present but which yielded no
        // environments must still be visible: emit a placeholder environment
        // rooted at the tool's directory so the UI shows the manager (with an
        // empty package list) instead of hiding it entirely.
        if (located == 0 && !detect.value.isEmpty()) {
            const ToolInstance& tool = detect.value.first();
            const QString root = QFileInfo(tool.path()).dir().path();
            envs.append(Environment(
                EnvironmentId::fromHashed(provider->pluginId().value(), root),
                provider->kind(),
                Scope::System,
                root,
                tool.id()));
        }
    }
}

void Scanner::scanRoots(const QStringList& roots,
                        int maxDepth,
                        const QStringList& ignoreRoots,
                        int maxEnvironments,
                        QVector<Environment>& envs)
{
    // Aggregated scanner hints from every provider: directories that carry an
    // environment marker must not be descended into, and skip dirs (e.g.
    // node_modules) are never walked for nested environments.
    QStringList projectMarkers;
    QStringList skipDirs;
    for (const IProvider* provider : m_providers) {
        for (const QString& m : provider->projectMarkers())
            if (!projectMarkers.contains(m))
                projectMarkers.append(m);
        for (const QString& d : provider->skipDirs())
            if (!skipDirs.contains(d))
                skipDirs.append(d);
    }

    QSet<QString> ignored;
    for (const QString& r : ignoreRoots) {
        const QString c = QFileInfo(r).canonicalFilePath();
        if (!c.isEmpty())
            ignored.insert(QDir::cleanPath(c));
    }

    // BFS over the given roots, calling locateAt on each directory.
    struct Item { QString path; int depth; };
    QVector<Item> queue;
    for (const QString& r : roots)
        queue.push_back({ r, 0 });

    QSet<QString> seen;
    int found = 0;
    while (!queue.isEmpty()) {
        const Item item = queue.takeFirst();
        const QDir dir(item.path);
        if (!dir.exists())
            continue;

        const QString canonical = dir.canonicalPath();
        if (seen.contains(canonical))
            continue;
        seen.insert(canonical);

        // A user-specified ignored root is skipped at any depth. The manifest
        // skip dirs (e.g. node_modules) are only skipped below the scan root:
        // the root itself is always walked, even if its name or a parent
        // segment matches a skip dir, otherwise a user-supplied root inside a
        // dependency container would be silently dropped.
        if (ignored.contains(canonical))
            continue;

        if (item.depth > 0) {
            bool isSkip = false;
            const QString leaf = dir.dirName();
            for (const QString& sk : skipDirs) {
                if (leaf == sk || canonical.contains(QLatin1Char('/') + sk + QLatin1Char('/'))) {
                    isSkip = true;
                    break;
                }
            }
            if (isSkip)
                continue;
        }

        for (IProvider* provider : m_providers) {
            const auto loc = provider->locateAt(item.path);
            if (loc.ok() && !loc.value.isEmpty()) {
                for (Environment env : loc.value) {
                    // The provider reports a project environment as Untrusted;
                    // the scanner owns the user's trust decision and upgrades it.
                    if (env.scope() == Scope::Project)
                        env.setStatus(m_trust.statusForProject(env.root()));
                    envs.append(env);
                    ++found;
                }
            }
        }

        if (found >= maxEnvironments)
            break;
        if (item.depth >= maxDepth)
            continue;

        // Do not descend into a directory that is itself an environment (it
        // carries a marker) or that is a dependency container the manifest
        // told us to skip. The marker has already been collected by locateAt.
        bool isEnv = false;
        for (const QString& marker : projectMarkers)
            if (dir.exists(marker))
                isEnv = true;
        if (isEnv)
            continue;

        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& sk : skipDirs)
            subdirs.removeAll(sk);
        subdirs.removeAll(QStringLiteral("__pycache__"));
        for (const QString& sub : subdirs) {
            const QString subPath = dir.filePath(sub);
            if (!QFileInfo(subPath).isSymLink())
                queue.append({ subPath, item.depth + 1 });
        }
    }
}