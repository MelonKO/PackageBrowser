#include "plugin/PluginHost.h"

#include "provider/BuiltinPipProvider.h"
#include "provider/DeclarativeProvider.h"
#include "provider/ProcessProvider.h"

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
    // Re-loading must not accumulate providers from a previous call: drop the
    // current set first, so invoking load() twice yields the same result.
    qDeleteAll(m_providers);
    m_providers.clear();

    // The built-in pip provider is always present, independent of any manifest.
    m_providers.append(new BuiltinPipProvider);

    if (pluginDir.isEmpty())
        return; // no plugin directory configured: do not scan the CWD

    const QDir dir(pluginDir);
    if (!dir.exists())
        return;

    // Each plugin lives in its own subfolder (plugins/<name>/plugin.toml), so
    // recurse and load only files literally named "plugin.toml"; any other .toml
    // (e.g. editor metadata) is ignored. A user-disabled plugin is skipped.
    QDirIterator it(pluginDir, { QStringLiteral("plugin.toml") },
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

        auto manifest = PluginManifest::parse(text, path, nullptr);
        if (!manifest) {
            // Rejected: skip, never abort the whole scan.
            continue;
        }
        if (disabledIds.contains(manifest->pluginId().value())) {
            // User-disabled: the plugin is not loaded, so its environments are
            // not scanned. Their cached data is left untouched and remains
            // visible until the plugin is re-enabled; it is not marked stale.
            continue;
        }
        if (manifest->isProcessProvider())
            m_providers.append(new ProcessProvider(std::move(*manifest)));
        else
            m_providers.append(new DeclarativeProvider(std::move(*manifest)));
    }
}

QVector<DeclarativeProvider*> PluginHost::declarativeProviders() const
{
    QVector<DeclarativeProvider*> out;
    for (IProvider* p : m_providers)
        if (auto* d = dynamic_cast<DeclarativeProvider*>(p))
            out.append(d);
    return out;
}

QVector<ProcessProvider*> PluginHost::processProviders() const
{
    QVector<ProcessProvider*> out;
    for (IProvider* p : m_providers)
        if (auto* pp = dynamic_cast<ProcessProvider*>(p))
            out.append(pp);
    return out;
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