#pragma once

#include <QString>
#include <QVector>

// Read-only description of a loaded provider, used by the plugins screen.
// The worker thread builds it after loading providers and ships it to the UI
// as a value snapshot, so the UI never touches the live host across threads.
struct PluginInfo {
    QString id;          // dotted plugin id, e.g. "builtin.vcpkg"
    bool isProcess = false; // external JSON-RPC process provider
    QString state;       // "ready" | "failed" | "quarantined"
    bool enabled = true; // not in the user's disabled list
};