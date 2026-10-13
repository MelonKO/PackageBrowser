#pragma once

#include "domain/EnvironmentId.h"
#include "domain/ManagerKind.h"
#include "domain/Scope.h"

#include <QString>

// Visibility level of an environment's status within the cache.
enum class EnvStatus {
    Ok,
    Stale,
    Error,
    Missing,
    Untrusted
};

QString envStatusToString(EnvStatus s);

// A place where installed packages live: scope, root, and the linked tool.
// See docs/Доменная модель.md and docs/Хранилище и кэш.md.
class Environment {
public:
    Environment() = default;
    Environment(EnvironmentId id,
                ManagerKind kind,
                Scope scope,
                QString root,
                QString toolId = QString(),
                QString inheritsFrom = QString(),
                EnvStatus status = EnvStatus::Ok,
                bool placeholder = false)
        : m_id(std::move(id))
        , m_kind(std::move(kind))
        , m_scope(scope)
        , m_root(std::move(root))
        , m_toolId(std::move(toolId))
        , m_inheritsFrom(std::move(inheritsFrom))
        , m_status(status)
        , m_placeholder(placeholder)
    {}

    const EnvironmentId& id() const { return m_id; }
    const ManagerKind& kind() const { return m_kind; }
    Scope scope() const { return m_scope; }
    const QString& root() const { return m_root; }
    const QString& toolId() const { return m_toolId; }
    const QString& inheritsFrom() const { return m_inheritsFrom; }
    EnvStatus status() const { return m_status; }
    void setStatus(EnvStatus s) { m_status = s; }

    // True for a synthetic "manager is installed" environment created when a
    // provider's executable exists but yields no real environments. It exists
    // for display only and must not be persisted to the cache as a real,
    // authoritative package store.
    bool isPlaceholder() const { return m_placeholder; }

private:
    EnvironmentId m_id;
    ManagerKind m_kind;
    Scope m_scope;
    QString m_root;
    QString m_toolId;
    QString m_inheritsFrom;
    EnvStatus m_status = EnvStatus::Ok;
    bool m_placeholder = false;
};