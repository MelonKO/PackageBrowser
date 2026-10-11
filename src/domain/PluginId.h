#pragma once

#include <QString>

#include <QtGlobal>

// Stable identifier of a plugin (a manager kind descriptor), e.g. "builtin.pip".
// The core addresses providers and environments through this id.
class PluginId {
public:
    PluginId() = default;
    explicit PluginId(QString value) : m_value(std::move(value)) {}

    const QString& value() const { return m_value; }
    bool isValid() const { return !m_value.isEmpty(); }

    bool operator==(const PluginId& o) const { return m_value == o.m_value; }
    bool operator!=(const PluginId& o) const { return !(*this == o); }

private:
    QString m_value;
};

uint qHash(const PluginId& p, uint seed = 0);