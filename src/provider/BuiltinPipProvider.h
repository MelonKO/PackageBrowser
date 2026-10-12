#pragma once

#include "provider/IProvider.h"

#include <QHash>
#include <QStringList>

// Built-in pip provider: a plain C++ class implementing IProvider. Read-only.
// In an environment marked trusted it runs the pip tool; otherwise it parses
// file layout (*.dist-info/METADATA) to read a venv without executing foreign
// code. Trust itself is decided outside the provider (see TrustPolicy).
// See docs/Обнаружение окружений.md and docs/Надёжность и безопасность.md.
class BuiltinPipProvider : public IProvider {
public:
    BuiltinPipProvider();

    const PluginId& pluginId() const override;
    const ManagerKind& kind() const override;
    bool hasCapability(Capability c) const override;

    ProviderResult<QVector<ToolInstance>> detect() override;
    ProviderResult<QVector<Environment>> locate(const ToolInstance& tool) override;
    ProviderResult<QVector<Environment>> locateAt(const QString& scanRoot) override;
    ProviderResult<QString> fingerprint(const Environment& env) override;
    ProviderResult<QVector<Installation>> list(const Environment& env, bool withDependencies = false) override;

private:
    // envId -> candidate tool path inside the environment, so list() does not
    // re-resolve the layout. Recording it executes nothing.
    QHash<QString, QString> m_envToolPaths;
};