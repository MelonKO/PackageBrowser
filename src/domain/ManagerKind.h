#pragma once

#include "domain/PluginId.h"

#include <QString>

// Family of a language or platform; groups managers and defines the rules
// for normalizing package names. See docs/Доменная модель.md.
enum class Ecosystem {
    Python,
    JavaScript,
    Cpp
};

QString ecosystemToString(Ecosystem e);

// Type of package manager (pip, npm, vcpkg). Described by a plugin.
class ManagerKind {
public:
    ManagerKind(PluginId pluginId, Ecosystem ecosystem)
        : m_pluginId(std::move(pluginId))
        , m_ecosystem(ecosystem)
    {}

    const PluginId& pluginId() const { return m_pluginId; }
    Ecosystem ecosystem() const { return m_ecosystem; }

    bool operator==(const ManagerKind& o) const
    {
        return m_pluginId == o.m_pluginId && m_ecosystem == o.m_ecosystem;
    }

    bool operator!=(const ManagerKind& o) const { return !(*this == o); }

private:
    PluginId m_pluginId;
    Ecosystem m_ecosystem;
};