#include "scanner/Scanner.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

Scanner::Scanner(IProvider* provider)
    : m_provider(provider)
{
}

void Scanner::setTrustPolicy(const TrustPolicy& policy)
{
    m_trust = policy;
}

void Scanner::scanGlobals(QVector<Environment>& envs, QVector<ToolInstance>& tools)
{
    const auto detect = m_provider->detect();
    if (!detect.ok())
        return;

    for (const ToolInstance& tool : detect.value) {
        tools.append(tool);
        const auto loc = m_provider->locate(tool);
        if (loc.ok())
            envs.append(loc.value);
    }
}

void Scanner::scanRoots(const QStringList& roots,
                        int maxDepth,
                        const QStringList& ignoreRoots,
                        int maxEnvironments,
                        QVector<Environment>& envs)
{
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

        // Skip explicitly ignored roots and nested dependency dirs.
        if (ignored.contains(canonical)
            || canonical.contains(QStringLiteral("/node_modules/"))
            || canonical.endsWith(QStringLiteral("/node_modules")))
            continue;

        const auto loc = m_provider->locateAt(item.path);
        if (loc.ok() && !loc.value.isEmpty()) {
            for (Environment env : loc.value) {
                // The provider reports a project environment as Untrusted; the
                // scanner owns the user's trust decision and upgrades it here.
                if (env.scope() == Scope::Project)
                    env.setStatus(m_trust.statusForProject(env.root()));
                envs.append(env);
                ++found;
            }
        }

        if (found >= maxEnvironments)
            break;
        if (item.depth >= maxDepth)
            continue;

        // Do not descend into a venv or node_modules when searching nested
        // environments, otherwise the walk visits thousands of foreign packages.
        // The marker at this directory has already been collected by locateAt.
        if (dir.exists(QLatin1String("pyvenv.cfg"))
            || dir.exists(QLatin1String("node_modules")))
            continue;

        const QStringList subdirs =
            dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
                .filter(QRegularExpression(QStringLiteral("^(?!node_modules$|__pycache__$).*$")));
        for (const QString& sub : subdirs) {
            const QString subPath = dir.filePath(sub);
            if (!QFileInfo(subPath).isSymLink())
                queue.append({ subPath, item.depth + 1 });
        }
    }
}