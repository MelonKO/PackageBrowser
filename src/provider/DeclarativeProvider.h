#pragma once

#include "plugin/PluginManifest.h"
#include "provider/IProvider.h"

#include <QHash>
#include <QStringList>

// Supports a CLI manager from its plugin.toml alone, with no plugin code.
// Each locator in the manifest says where environments are and how to read the
// packages there: by parsing files (no execution) or by running the manager and
// reading JSON.
// See docs/Плагины и JSON-RPC.md and docs/Обнаружение окружений.md.
class DeclarativeProvider : public IProvider {
public:
    explicit DeclarativeProvider(PluginManifest manifest);

    const PluginId& pluginId() const override;
    const ManagerKind& kind() const override;
    bool hasCapability(Capability c) const override;

    ProviderResult<QVector<ToolInstance>> detect() override;
    ProviderResult<QVector<Environment>> locate(const ToolInstance& tool) override;
    ProviderResult<QVector<Environment>> locateAt(const QString& scanRoot) override;
    ProviderResult<QString> fingerprint(const Environment& env) override;
    ProviderResult<ListResult> list(const Environment& env, bool withDependencies = false) override;

    const PluginManifest& manifest() const { return m_manifest; }

    // Marker files from every project locator: the scanner must not descend
    // into a directory that is itself an environment.
    QStringList projectMarkers() const;

    // Directories that must never be walked for nested environments
    // (declared list_files dirs, e.g. node_modules).
    QStringList skipDirs() const;

private:
    // Locator that produced an environment; null when none matches.
    const LocatorSpec* locatorFor(const Environment& env) const;

    ProviderResult<ListResult> listFromFiles(const Environment& env,
                                             const LocatorSpec& locator);
    ProviderResult<ListResult> listFromCommand(const Environment& env,
                                               const LocatorSpec& locator);

    PluginManifest m_manifest;
    // envId -> candidate tool path inside the environment (executes nothing).
    QHash<QString, QString> m_envToolPaths;
    // envId -> index of the locator that produced the environment.
    QHash<QString, int> m_envLocators;
};
