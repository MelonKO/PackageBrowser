#pragma once

#include "domain/EnvironmentId.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"

#include <QJsonValue>
#include <QString>
#include <QVector>

namespace pb {

// Navigate a dot-separated path inside a JSON document. An empty path returns
// the document itself; a missing segment yields an undefined value.
QJsonValue jsonAtPath(const QJsonValue& root, const QString& path);

// Read packages out of a decoded manager output. The container is either an
// array of package objects (pip list --format=json) or an object keyed by
// package name (npm ls reports its "dependencies" that way). Returns false when
// the container is neither, so the caller can report ParseFailed.
// See docs/Плагины и JSON-RPC.md ("The plugin.toml manifest").
bool extractInstallations(const QJsonValue& container,
                          const QString& nameKey,
                          const QString& versionKey,
                          Ecosystem ecosystem,
                          const EnvironmentId& envId,
                          QVector<Installation>& out);

} // namespace pb
