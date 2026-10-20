#pragma once

#include "domain/DependencyEdge.h"
#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"

#include <QString>
#include <QStringList>
#include <QVector>

// SQLite disposable cache: UI always reads from here, the disk is the source
// of truth. The schema is recreated when PRAGMA user_version mismatches.
// See docs/Хранилище и кэш.md.
class Cache {
public:
    explicit Cache(const QString& dbPath);
    ~Cache();

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    // Returns false if the database could not be opened or initialized.
    bool open();

    void upsertTool(const ToolInstance& tool);
    QVector<ToolInstance> tools() const;
    void removeTool(const QString& toolId);

    void upsertEnvironment(const Environment& env);
    QVector<Environment> environments() const;
    void markEnvironmentMissing(const EnvironmentId& id);
    void updateEnvironmentStatus(const EnvironmentId& id, EnvStatus status);

    void replaceEnvironment(const EnvironmentId& envId,
                            const QVector<Installation>& packages,
                            const QVector<DependencyEdge>& dependencies);
    QVector<Installation> installations(const EnvironmentId& envId) const;
    QVector<DependencyEdge> dependencies(const EnvironmentId& envId) const;
    // All installations across every environment (used by "find package").
    QVector<Installation> allInstallations() const;

    int schemaVersion() const;

private:
    void ensureSchema();

    QString m_dbPath;
    class Impl;
    Impl* d;
};