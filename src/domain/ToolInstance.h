#pragma once

#include "domain/ManagerKind.h"

#include <QString>

// A concrete executable file of a manager: its path, version, and the
// environment that provides it, if any. Example: the pip inside a venv.
// See docs/Доменная модель.md.
class ToolInstance {
public:
    ToolInstance() = default;
    ToolInstance(ManagerKind kind,
                 QString id,
                 QString path,
                 QString version = QString(),
                 QString providedByEnv = QString())
        : m_kind(std::move(kind))
        , m_id(std::move(id))
        , m_path(std::move(path))
        , m_version(std::move(version))
        , m_providedByEnv(std::move(providedByEnv))
    {}

    const ManagerKind& kind() const { return m_kind; }

    // Stable id: hash(plugin_id, real_path). De-duplicates pip vs pip3.
    const QString& id() const { return m_id; }

    const QString& path() const { return m_path; }
    const QString& version() const { return m_version; }

    // Id of the environment that provides this tool, empty if installed system-wide.
    const QString& providedByEnv() const { return m_providedByEnv; }

private:
    ManagerKind m_kind;
    QString m_id;
    QString m_path;
    QString m_version;
    QString m_providedByEnv;
};