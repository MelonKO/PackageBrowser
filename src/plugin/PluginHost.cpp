#include "plugin/PluginHost.h"

#include "provider/BuiltinPipProvider.h"
#include "provider/DeclarativeProvider.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

PluginHost::~PluginHost()
{
    qDeleteAll(m_providers);
}

void PluginHost::load(const QString& pluginDir, const QStringList& disabledIds)
{
    // The built-in pip provider is always present, independent of any manifest.
    m_providers.append(new BuiltinPipProvider);

    if (pluginDir.isEmpty())
        return; // no plugin directory configured: do not scan the CWD

    const QDir dir(pluginDir);
    if (!dir.exists())
        return;

    // Each plugin lives in its own subfolder (plugins/<name>/plugin.toml), so
    // recurse and pick up every manifest. Iteration is by file, subdirectories
    // included. A user-disabled plugin is skipped entirely.
    QDirIterator it(pluginDir, { QStringLiteral("*.toml") },
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString path = it.filePath();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream ts(&file);
        const QString text = ts.readAll();
        file.close();

        QString error;
        auto manifest = PluginManifest::parse(text, path, &error);
        if (!manifest) {
            // Rejected: skip, never abort the whole scan.
            continue;
        }
        if (disabledIds.contains(manifest->pluginId().value())) {
            // Disabled by the user: not loaded. Data stays in the cache.
            continue;
        }
        m_providers.append(new DeclarativeProvider(std::move(*manifest)));
    }
}

bool PluginHost::hasDeclarativeProjects() const
{
    for (IProvider* provider : m_providers) {
        if (const auto* declarative = dynamic_cast<DeclarativeProvider*>(provider))
            if (!declarative->manifest().locators().isEmpty())
                return true;
    }
    return false;
}

QStringList PluginHost::projectMarkers() const
{
    QStringList out;
    for (IProvider* provider : m_providers) {
        if (const auto* declarative = dynamic_cast<DeclarativeProvider*>(provider)) {
            const QStringList markers = declarative->projectMarkers();
            for (const QString& m : markers)
                if (!out.contains(m))
                    out.append(m);
        }
    }
    return out;
}

QStringList PluginHost::skipDirs() const
{
    QStringList out;
    for (IProvider* provider : m_providers) {
        if (const auto* declarative = dynamic_cast<DeclarativeProvider*>(provider)) {
            const QStringList dirs = declarative->skipDirs();
            for (const QString& d : dirs)
                if (!out.contains(d))
                    out.append(d);
        }
    }
    return out;
}