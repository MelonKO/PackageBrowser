#include <QtTest/QtTest>

#include "cache/Cache.h"
#include "domain/EnvironmentId.h"
#include "domain/ManagerKind.h"
#include "domain/PluginId.h"

#include <QTemporaryDir>

class CacheTest : public QObject
{
    Q_OBJECT

private slots:
    void upsertAndReadTool();
    void upsertAndReadEnvironment();
    void replaceInstallations();
    void environmentStatusUpdate();
};

void CacheTest::upsertAndReadTool()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Cache cache(dir.filePath(QStringLiteral("cache.db")));
    QVERIFY(cache.open());

    const ManagerKind kind(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    cache.upsertTool(ToolInstance(kind, QStringLiteral("pip-1"), QStringLiteral("/usr/bin/pip"),
                                  QStringLiteral("23.2"), QString()));

    const auto tools = cache.tools();
    QCOMPARE(tools.size(), 1);
    QCOMPARE(tools[0].id(), QStringLiteral("pip-1"));
    QCOMPARE(tools[0].path(), QStringLiteral("/usr/bin/pip"));
    QCOMPARE(tools[0].version(), QStringLiteral("23.2"));
}

void CacheTest::upsertAndReadEnvironment()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Cache cache(dir.filePath(QStringLiteral("cache.db")));
    QVERIFY(cache.open());

    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("/work/app/.venv"));
    const ManagerKind kind(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    cache.upsertEnvironment(Environment(envId, kind, Scope::Project, QStringLiteral("/work/app/.venv")));

    const auto envs = cache.environments();
    QCOMPARE(envs.size(), 1);
    QCOMPARE(envs[0].id(), envId);
    QCOMPARE(envs[0].root(), QStringLiteral("/work/app/.venv"));
    QCOMPARE(envs[0].scope(), Scope::Project);
    QCOMPARE(envs[0].status(), EnvStatus::Ok);
}

void CacheTest::replaceInstallations()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Cache cache(dir.filePath(QStringLiteral("cache.db")));
    QVERIFY(cache.open());

    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("/work/app/.venv"));
    const ManagerKind kind(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    cache.upsertEnvironment(Environment(envId, kind, Scope::Project, QStringLiteral("/work/app/.venv")));

    cache.replaceEnvironment(envId, {
        Installation(envId, QStringLiteral("requests"), QStringLiteral("Requests"),
                     QStringLiteral("2.31.0")),
        Installation(envId, QStringLiteral("urllib3"), QStringLiteral("urllib3"),
                     QStringLiteral("2.0.7")),
    }, {
        DependencyEdge(envId, QStringLiteral("requests"), QStringLiteral("urllib3"),
                       QStringLiteral(">=2.0.0")),
    });

    auto pkgs = cache.installations(envId);
    QCOMPARE(pkgs.size(), 2);
    QCOMPARE(pkgs[0].nameNorm(), QStringLiteral("requests"));
    QCOMPARE(pkgs[1].nameNorm(), QStringLiteral("urllib3"));

    // Dependencies are stored alongside their environment.
    const auto deps = cache.dependencies(envId);
    QCOMPARE(deps.size(), 1);
    QCOMPARE(deps[0].fromName(), QStringLiteral("requests"));
    QCOMPARE(deps[0].toName(), QStringLiteral("urllib3"));
    QCOMPARE(deps[0].constraint(), QStringLiteral(">=2.0.0"));

    // Replacing wipes the previous set of packages and dependencies.
    cache.replaceEnvironment(envId, {
        Installation(envId, QStringLiteral("requests"), QStringLiteral("Requests"),
                     QStringLiteral("2.32.0")),
    }, {});
    pkgs = cache.installations(envId);
    QCOMPARE(pkgs.size(), 1);
    QCOMPARE(pkgs[0].version(), QStringLiteral("2.32.0"));
    QCOMPARE(cache.dependencies(envId).size(), 0);

    const auto all = cache.allInstallations();
    QCOMPARE(all.size(), 1);
}

void CacheTest::environmentStatusUpdate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Cache cache(dir.filePath(QStringLiteral("cache.db")));
    QVERIFY(cache.open());

    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("/work/app/.venv"));
    const ManagerKind kind(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    cache.upsertEnvironment(Environment(envId, kind, Scope::Project, QStringLiteral("/work/app/.venv")));

    cache.markEnvironmentMissing(envId);
    const auto envs = cache.environments();
    QCOMPARE(envs[0].status(), EnvStatus::Missing);

    cache.updateEnvironmentStatus(envId, EnvStatus::Ok);
    const auto envs2 = cache.environments();
    QCOMPARE(envs2[0].status(), EnvStatus::Ok);
}

QTEST_MAIN(CacheTest)
#include "test_cache.moc"