#pragma once

#include "domain/ManagerKind.h"
#include "domain/PluginId.h"
#include "domain/Scope.h"
#include "provider/IProvider.h"

#include <optional>

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

// Compatibility version of the host/plugin contract. A manifest declaring a
// higher `api` is rejected instead of being misread.
// See docs/Плагины и JSON-RPC.md ("Contract rules").
inline constexpr int kHostApiVersion = 1;

// Command template placeholders the manifest whitelist allows. Anything else
// inside braces is a manifest error, never a silent pass-through.
// See docs/Надёжность и безопасность.md ("Rules").
QStringList manifestAllowedPlaceholders();

// How a locator reads an environment's packages by running a command that
// prints JSON, e.g. `npm ls -g --json --depth=0`.
struct ListJsonSpec {
    QStringList command;
    // Optional path to the container holding the packages ("dependencies").
    // Empty means the parsed document is the container itself.
    QString arrayPath;
    QString nameKey = QStringLiteral("name");
    QString versionKey = QStringLiteral("version");
    // Some managers exit non-zero while printing valid JSON (npm ls). Only the
    // codes listed here count as success.
    QList<int> okExitCodes{ 0 };
};

// How a locator reads an environment's packages by parsing files, without
// executing foreign code, e.g. node_modules/*/package.json.
struct ListFilesSpec {
    QString dir; // relative to the environment root
    QString manifest = QStringLiteral("package.json");
    QString nameKey = QStringLiteral("name");
    QString versionKey = QStringLiteral("version");
};

// Where a locator looks for environments and how it lists their packages.
struct LocatorSpec {
    Scope scope = Scope::Project;
    // scope system/user: derive the environment from the tool found on PATH.
    bool fromPath = false;
    // scope project: the marker file that makes a directory an environment.
    QString markerFile;
    // Optional extra directory that must exist next to the marker.
    QString requireDir;
    // Optional tool paths inside the environment root, in priority order
    // (e.g. ["bin/pip", "Scripts/pip.exe"]).
    QStringList toolsInRoot;

    std::optional<ListJsonSpec> listJson;
    std::optional<ListFilesSpec> listFiles;

    bool hasListSpec() const { return listJson.has_value() || listFiles.has_value(); }
};

// How to find and version the manager executable.
struct ToolSpec {
    QStringList executables;
    QStringList versionCmd;
    QString versionRegex;
};

// An external process provider declared with `[provider] exec` instead of
// `[tool]` and `[[locator]]`. The host runs this command and speaks JSON-RPC
// 2.0 over stdio; all IProvider operations become RPC calls. Relative program
// paths resolve against the plugin's own directory.
struct ProcessProviderSpec {
    QString program;         // executable path or bare name (searched on PATH)
    QStringList args;        // fixed arguments, e.g. ["vcpkg_provider.py"]
};

// A declarative plugin descriptor parsed from plugin.toml. Reading it needs no
// plugin code, which is what lets a new CLI manager be added by dropping a
// single manifest.
// See docs/Плагины и JSON-RPC.md ("The plugin.toml manifest").
class PluginManifest {
public:
    // Parses and validates a manifest. Returns nullopt and fills `error` when
    // the manifest is malformed or incompatible, so the host can mark the
    // plugin Rejected instead of failing the whole scan.
    static std::optional<PluginManifest> parse(const QString& text,
                                               const QString& sourcePath,
                                               QString* error);

    const PluginId& pluginId() const { return m_pluginId; }
    int apiVersion() const { return m_apiVersion; }
    const QString& name() const { return m_name; }
    const ManagerKind& kind() const { return m_kind; }
    const QStringList& capabilities() const { return m_capabilities; }
    bool hasCapability(Capability c) const;
    const ToolSpec& tool() const { return m_tool; }
    const QVector<LocatorSpec>& locators() const { return m_locators; }
    const QString& sourcePath() const { return m_sourcePath; }

    // A manifest defines either the declarative `[tool]`+`[[locator]]` shape
    // or an external `[provider] exec` process. Exactly one of the two is set.
    const ProcessProviderSpec& processProvider() const { return m_processProvider; }
    bool isProcessProvider() const { return m_isProcessProvider; }

private:
    PluginId m_pluginId;
    int m_apiVersion = 0;
    QString m_name;
    ManagerKind m_kind{ PluginId(), Ecosystem::Python };
    QStringList m_capabilities;
    ToolSpec m_tool;
    QVector<LocatorSpec> m_locators;
    ProcessProviderSpec m_processProvider;
    bool m_isProcessProvider = false;
    QString m_sourcePath;
};
