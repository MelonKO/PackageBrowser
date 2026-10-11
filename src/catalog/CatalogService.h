#pragma once

#include "domain/Environment.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/ToolInstance.h"
#include "provider/BuiltinPipProvider.h"
#include "scanner/Scanner.h"
#include "settings/AppSettings.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class Cache;

// Facade of the core and the single contact point for the UI. Reads come from
// the cache; scanning runs in the background with stale-while-revalidate.
// See docs/UI на Qt.md and docs/Хранилище и кэш.md.
class CatalogService : public QObject {
    Q_OBJECT

public:
    explicit CatalogService(QObject* parent = nullptr);
    ~CatalogService() override;

    CatalogService(const CatalogService&) = delete;
    CatalogService& operator=(const CatalogService&) = delete;

    // One-time initialization: open the cache and load settings.
    bool initialize(const QString& dbPath);

    // Public read API (UI calls these and connects to signals).
    QVector<Environment> listEnvironments() const;
    QVector<Installation> listInstallations(const EnvironmentId& envId) const;
    QVector<Installation> findPackage(const QString& name) const;

    // Starts a synchronous scan (unit-test friendly). UI runs it in a worker.
    void rescan();

    // Settings passthrough.
    const AppSettings& settings() const { return m_settings; }
    AppSettings& settings() { return m_settings; }

signals:
    void environmentsChanged();
    void installationsChanged(const QString& envId);
    void scanProgress(int done, int total);
    void scanFinished();

private:
    void normalizeAndStoreInstallations(const Environment& env);

    AppSettings m_settings;
    Cache* m_cache = nullptr;
    BuiltinPipProvider m_pip;
    Scanner m_scanner;

    QVector<Environment> m_environments; // in-memory view used by the UI
    QVector<ToolInstance> m_tools;
};