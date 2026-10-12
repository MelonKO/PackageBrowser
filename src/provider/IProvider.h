#pragma once

#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"

#include <optional>

#include <QString>
#include <QStringList>
#include <QVector>

// A typed provider error, mapped to an environment status rather than thrown
// as an exception. See docs/Плагины и JSON-RPC.md ("Contract rules").
enum class ProviderError {
    None,
    ToolNotFound,
    EnvBroken,
    Timeout,
    ParseFailed,
    Untrusted,
    Unsupported
};

// Result of a provider operation; carries a typed error and a partial flag.
template <typename T>
struct ProviderResult {
    T value{};
    ProviderError error = ProviderError::None;
    bool complete = true; // false when the data is partial (honest "I don't know")

    bool ok() const { return error == ProviderError::None; }
};

// Capabilities a plugin explicitly declares; operations outside the list
// are never called and the UI hides the corresponding buttons.
enum class Capability {
    Detect,
    Locate,
    Fingerprint,
    List
};

// The provider contract: four pure read operations. They never mutate the
// environment, so they are safe to repeat, time out, and run in parallel.
// See docs/Плагины и JSON-RPC.md ("Provider contract").
class IProvider {
public:
    virtual ~IProvider() = default;

    virtual const PluginId& pluginId() const = 0;
    virtual const ManagerKind& kind() const = 0;
    virtual bool hasCapability(Capability c) const = 0;

    // Find manager executables (on PATH and known locations).
    virtual ProviderResult<QVector<ToolInstance>> detect() = 0;

    // Derive the environments reachable from a given tool or scan root.
    virtual ProviderResult<QVector<Environment>> locate(const ToolInstance& tool) = 0;
    virtual ProviderResult<QVector<Environment>> locateAt(const QString& scanRoot) = 0;

    // Cheap string that changes when the environment's content changes.
    virtual ProviderResult<QString> fingerprint(const Environment& env) = 0;

    // List packages (with dependencies only if explicitly requested and supported).
    virtual ProviderResult<QVector<Installation>>
    list(const Environment& env, bool withDependencies = false) = 0;

    // Hints for the scanner. A marker file makes a directory an environment
    // root, so the walk must not descend into it; a skip dir is never walked
    // for nested environments (e.g. node_modules). Aggregated across providers.
    virtual QStringList projectMarkers() const { return {}; }
    virtual QStringList skipDirs() const { return {}; }
};