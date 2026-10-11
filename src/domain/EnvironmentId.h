#pragma once

#include <QString>

// Identifier of an environment: hash of (plugin id, canonical root).
// Moving the directory yields a new environment; the old one is marked missing.
// See docs/Доменная модель.md ("Identity and name normalization").
class EnvironmentId {
public:
    static EnvironmentId fromHashed(const QString& pluginId, const QString& canonicalRoot)
    {
        return EnvironmentId(shortHash(pluginId, canonicalRoot));
    }

    // Rebuild an id from its stored representation (e.g. loaded from the cache).
    static EnvironmentId fromValue(const QString& value)
    {
        return EnvironmentId(value);
    }

    const QString& value() const { return m_value; }
    bool isValid() const { return !m_value.isEmpty(); }

    bool operator==(const EnvironmentId& other) const { return m_value == other.m_value; }
    bool operator!=(const EnvironmentId& other) const { return !(*this == other); }

private:
    explicit EnvironmentId(QString value) : m_value(std::move(value)) {}

    static QString shortHash(const QString& pluginId, const QString& canonicalRoot);

    QString m_value;
};

uint qHash(const EnvironmentId& id, uint seed = 0);