#pragma once

#include "domain/EnvironmentId.h"

#include <QString>

// A dependency between two packages inside a single environment: fromName
// requires toName (optionally with a version constraint). Identified by the
// triple (envId, fromName, toName). See docs/Доменная модель.md.
class DependencyEdge {
public:
    DependencyEdge() = default;
    DependencyEdge(EnvironmentId envId,
                   QString fromName,
                   QString toName,
                   QString constraint = QString())
        : m_envId(std::move(envId))
        , m_fromName(std::move(fromName))
        , m_toName(std::move(toName))
        , m_constraint(std::move(constraint))
    {}

    const EnvironmentId& envId() const { return m_envId; }

    // Depender package name, normalized by the ecosystem's rules.
    const QString& fromName() const { return m_fromName; }

    // Dependency package name, normalized by the ecosystem's rules.
    const QString& toName() const { return m_toName; }

    // Version constraint as reported by the manager (e.g. ">=1.2", "~1.3");
    // may be empty (honest "I don't know").
    const QString& constraint() const { return m_constraint; }

private:
    EnvironmentId m_envId;
    QString m_fromName;
    QString m_toName;
    QString m_constraint;
};