#pragma once

#include "domain/EnvironmentId.h"

#include <optional>

#include <QString>

// A package installed in a specific environment. Identified by the pair
// (EnvironmentId, normalized name); version is an attribute, not identity.
// See docs/Доменная модель.md ("Identity and name normalization").
class Installation {
public:
    Installation() = default;
    Installation(EnvironmentId envId,
                 QString nameNorm,
                 QString nameDisplay,
                 QString version = QString(),
                 QString location = QString(),
                 std::optional<bool> isDirect = std::nullopt)
        : m_envId(std::move(envId))
        , m_nameNorm(std::move(nameNorm))
        , m_nameDisplay(std::move(nameDisplay))
        , m_version(std::move(version))
        , m_location(std::move(location))
        , m_isDirect(isDirect)
    {}

    const EnvironmentId& envId() const { return m_envId; }

    // Normalized by the ecosystem's rules (e.g. PEP 503).
    const QString& nameNorm() const { return m_nameNorm; }

    // Original spelling as reported by the manager.
    const QString& nameDisplay() const { return m_nameDisplay; }

    const QString& version() const { return m_version; }
    const QString& location() const { return m_location; }

    // Whether the package was requested explicitly; nullopt if the manager
    // does not know (honest "I don't know").
    std::optional<bool> isDirect() const { return m_isDirect; }

private:
    EnvironmentId m_envId;
    QString m_nameNorm;
    QString m_nameDisplay;
    QString m_version;
    QString m_location;
    std::optional<bool> m_isDirect;
};