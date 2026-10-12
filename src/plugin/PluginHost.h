#pragma once

#include "plugin/PluginManifest.h"
#include "trust/TrustPolicy.h"

#include <QString>
#include <QStringList>
#include <QVector>

class IProvider;

// Owns the set of live providers: the built-in pip provider plus one
// DeclarativeProvider per valid plugin.toml found in the plugins directory.
// A malformed or incompatible manifest is skipped (Rejected) rather than
// aborting the whole scan.
// See docs/Плагины и JSON-RPC.md ("Lifecycle") and docs/Надёжность и безопасность.md.
class PluginHost {
public:
    PluginHost() = default;
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Loads every *.toml in `pluginDir` and instantiates the providers.
    // builtin.pip is always available even when no directory exists. Manifests
    // whose plugin id is in `disabledIds` are skipped (user turned them off).
    void load(const QString& pluginDir, const QStringList& disabledIds = {});

    // Live providers, built-in pip first. Owned by this host.
    const QVector<IProvider*>& providers() const { return m_providers; }

    // Providers that expose project markers / skip dirs for the scanner.
    bool hasDeclarativeProjects() const;

    // Union of project marker files across declarative providers.
    QStringList projectMarkers() const;

    // Union of directories the scanner must never walk for nested environments.
    QStringList skipDirs() const;

private:
    QVector<IProvider*> m_providers;
};