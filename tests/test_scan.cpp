#include <QtTest/QtTest>

#include "plugin/PluginManifest.h"
#include "process/ProcessRunner.h"
#include "provider/BuiltinPipProvider.h"
#include "provider/DeclarativeProvider.h"
#include "scanner/Scanner.h"
#include "trust/TrustPolicy.h"

#include <QTemporaryDir>
#include <QDir>

class ShellScanTest : public QObject
{
    Q_OBJECT

private slots:
    void locateVenvFromScannedRoot();
    void untrustedReadsFromFiles();
    void trustedRootUpgradesStatus();
    void usesDeclarativeMarkersAndSkipDirs();
    void systemEnvNotSubjectToTrust();
    void scanRootMatchingSkipDirIsStillWalked();
    void venvInheritsFromSystemWhenIncludeSystemSitePackages();
    void managerWithoutEnvironmentsIsStillShown();
};

// Build a minimal venv layout with a *.dist-info/METADATA under site-packages.
static QString makeBareVenv(const QString& root)
{
    const QDir site(QDir(root).filePath(QStringLiteral("Lib/site-packages")));
    site.mkpath(QStringLiteral("requests.dist-info"));
    {
        QFile meta(site.filePath(QStringLiteral("requests.dist-info/METADATA")));
        if (meta.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&meta);
            // Real pip METADATA starts with Metadata-Version, then Name, Version.
            ts << "Metadata-Version: 2.1\n"
               << "Name: Requests\n"
               << "Version: 2.31.0\n";
            meta.close();
        }
    }
    {
        QFile cfg(QDir(root).filePath(QStringLiteral("pyvenv.cfg")));
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&cfg);
            ts << "home = /usr\n";
            cfg.close();
        }
    }
    return root;
}

void ShellScanTest::locateVenvFromScannedRoot()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));

    BuiltinPipProvider provider;
    Scanner scanner(QVector<IProvider*>{ &provider });
    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanRoots(QStringList{ work.filePath(QStringLiteral("app")) }, 3, {}, 50, envs);

    qWarning("scanRoots found %d env(s)", envs.size());
    for (const Environment& e : envs)
        qWarning("  -> root=%s scope=%d status=%d", qPrintable(e.root()), int(e.scope()), int(e.status()));

    QCOMPARE(envs.size(), 1);
    QCOMPARE(envs[0].scope(), Scope::Project);
    QCOMPARE(envs[0].root(), QDir(venvRoot).canonicalPath());
    // Not trusted by default -> Untrusted.
    QCOMPARE(envs[0].status(), EnvStatus::Untrusted);
}

void ShellScanTest::venvInheritsFromSystemWhenIncludeSystemSitePackages()
{
    // A venv with include-system-site-packages = true exposes the system
    // site-packages; the project environment must carry an inheritsFrom edge
    // to the base interpreter's system environment.
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));
    // A real base-interpreter prefix so the helper's existence check passes
    // (the inheritsFrom edge targets this directory as the system root).
    const QString baseInterpPath = work.filePath(QStringLiteral("Python312"));
    QVERIFY(QDir().mkpath(baseInterpPath));
    const QString baseInterp = QDir(baseInterpPath).canonicalPath();
    {
        QFile cfg(QDir(venvRoot).filePath(QStringLiteral("pyvenv.cfg")));
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&cfg);
            // Real venvs write the base interpreter's prefix (e.g. C:\Python312),
            // not a bin subdirectory; locate() derives the System root as the
            // directory containing the base pip (here the prefix itself).
            ts << "home = " << baseInterp << "\n"
               << "include-system-site-packages = true\n";
            cfg.close();
        }
    }

    BuiltinPipProvider provider;
    const auto envId = EnvironmentId::fromHashed(provider.pluginId().value(),
                                                 QDir(venvRoot).canonicalPath());
    const auto located = provider.locateAt(venvRoot);
    QVERIFY(located.ok());
    QCOMPARE(located.value.size(), 1);
    QCOMPARE(located.value[0].id().value(), envId.value());
    // The base interpreter prefix is the system environment root.
    const auto systemId = EnvironmentId::fromHashed(provider.pluginId().value(), baseInterp);
    QCOMPARE(located.value[0].inheritsFrom(), systemId.value());

    // Without the flag the edge is absent (the existing default case).
    const QString plainVenv = makeBareVenv(work.filePath(QStringLiteral("plain/.venv")));
    const auto locatedPlain = provider.locateAt(plainVenv);
    QVERIFY(locatedPlain.ok());
    QCOMPARE(locatedPlain.value.size(), 1);
    QVERIFY(locatedPlain.value[0].inheritsFrom().isEmpty());
}

void ShellScanTest::untrustedReadsFromFiles()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));

    BuiltinPipProvider provider;
    const auto envId = EnvironmentId::fromHashed(provider.pluginId().value(),
                                                 QDir(venvRoot).canonicalPath());
    const QString toolId = QStringLiteral("builtin.pip:venv:xyz");
    const Environment env(envId, provider.kind(), Scope::Project,
                          QDir(venvRoot).canonicalPath(), toolId, QString(), EnvStatus::Untrusted);

    const auto result = provider.list(env);
    QVERIFY(result.ok());
    QCOMPARE(result.value.installations.size(), 1);
    QCOMPARE(result.value.installations[0].nameNorm(), QStringLiteral("requests"));
    QCOMPARE(result.value.installations[0].nameDisplay(), QStringLiteral("Requests"));
    QCOMPARE(result.value.installations[0].version(), QStringLiteral("2.31.0"));
}

void ShellScanTest::trustedRootUpgradesStatus()
{
    // The provider reports a project environment as Untrusted; the scanner
    // upgrades it to Ok when the user trusts the root.
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString appRoot = work.filePath(QStringLiteral("app"));
    makeBareVenv(QDir(appRoot).filePath(QStringLiteral(".venv")));

    BuiltinPipProvider provider;
    Scanner scanner(QVector<IProvider*>{ &provider });
    scanner.setTrustPolicy(TrustPolicy(QStringList{ appRoot }));

    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanRoots(QStringList{ appRoot }, 3, {}, 50, envs);

    QCOMPARE(envs.size(), 1);
    QCOMPARE(envs[0].status(), EnvStatus::Ok);

    // The venv carries no real pip executable, so a trusted list() must fail
    // cleanly instead of silently falling back to parsing files.
    const auto result = provider.list(envs[0]);
    QVERIFY(!result.ok());
}

void ShellScanTest::usesDeclarativeMarkersAndSkipDirs()
{
    // A declarative provider declares its marker files and skip dirs; the
    // scanner must apply them so foreign projects never walk node_modules and
    // a directory that is itself an environment is not descended into.
    const QString manifestText = QStringLiteral(R"toml(
[plugin]
id = "testkit.npm"
api = 1
name = "testkit-npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["testkit-not-present"]

[[locator]]
scope = "project"
marker_file = "package.json"
require_dir = "node_modules"
[locator.list_files]
dir = "node_modules"
manifest = "package.json"
name = "name"
version = "version"
)toml");
    QString error;
    auto manifest = PluginManifest::parse(manifestText, QStringLiteral("t/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    DeclarativeProvider provider(*manifest);

    Scanner scanner(QVector<IProvider*>{ &provider });

    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString appRoot = work.filePath(QStringLiteral("app"));
    QVERIFY(QDir().mkpath(appRoot));
    QVERIFY(QDir().mkpath(QDir(appRoot).filePath(QStringLiteral("node_modules/pkg"))));
    QFile marker(QDir(appRoot).filePath(QStringLiteral("package.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanRoots(QStringList{ appRoot }, 3, {}, 50, envs);

    // One project environment is found (node_modules must not spawn a nested
    // walk, and the marker root is itself an environment).
    QCOMPARE(envs.size(), 1);
    QCOMPARE(envs[0].scope(), Scope::Project);
    QCOMPARE(envs[0].root(), QDir(appRoot).canonicalPath());
}

void ShellScanTest::systemEnvNotSubjectToTrust()
{
    // A system-scoped locator (from=path) yields a System environment with Ok
    // status: trust gating only ever applies to project environments, never to
    // a manager resolved from the user's PATH.
    const QString manifestText = QStringLiteral(R"toml(
[plugin]
id = "testkit.sys"
api = 1
name = "testkit-sys"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["node-not-present-on-path"]

[[locator]]
scope = "system"
from = "path"
[locator.list_json]
command = ["{exe}", "ls", "--json"]
)toml");
    QString error;
    auto manifest = PluginManifest::parse(manifestText, QStringLiteral("sys/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    DeclarativeProvider provider(*manifest);

    Scanner scanner(QVector<IProvider*>{ &provider });
    // No trusted roots: the scanner would mark a project Untrusted. A system
    // environment must stay Ok regardless.
    const TrustPolicy noTrust; // default: empty trusted roots
    scanner.setTrustPolicy(noTrust);

    // Construct a fake ToolInstance like detect() would: empty PATH executable
    // means we cannot rely on detect here, so drive locate() directly.
    const ManagerKind kind = provider.kind();
    const ToolInstance tool(kind, QStringLiteral("sys:tool"), QStringLiteral("C:/tools/node"), QStringLiteral("20.0.0"));
    const auto located = provider.locate(tool);
    QVERIFY(located.ok());
    QCOMPARE(located.value.size(), 1);
    QCOMPARE(located.value[0].scope(), Scope::System);
    QCOMPARE(located.value[0].status(), EnvStatus::Ok);
}

void ShellScanTest::scanRootMatchingSkipDirIsStillWalked()
{
    // Regression: if a user-supplied scan root's own name matches a manifest
    // skip dir (or a parent segment matches), the root must still be walked and
    // reported, instead of being silently dropped.
    const QString manifestText = QStringLiteral(R"toml(
[plugin]
id = "testkit.skip"
api = 1
name = "testkit-skip"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["node-not-present-on-path"]

[[locator]]
scope = "project"
marker_file = "package.json"
require_dir = "node_modules"
[locator.list_files]
dir = "node_modules"
manifest = "package.json"
name = "name"
version = "version"
)toml");
    QString error;
    auto manifest = PluginManifest::parse(manifestText, QStringLiteral("skip/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    DeclarativeProvider provider(*manifest);

    // A user-supplied scan root living *inside* a directory whose name matches
    // the skip dir (here: project under a real "node_modules") must still be
    // walked and reported, instead of being silently skipped because a parent
    // segment matches a skip dir.
    QTemporaryDir base;
    QVERIFY(base.isValid());
    const QString root = base.filePath(QStringLiteral("node_modules/project"));
    QVERIFY(QDir().mkpath(root));
    QFile marker(QDir(root).filePath(QStringLiteral("package.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("node_modules"))));

    Scanner scanner(QVector<IProvider*>{ &provider });
    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanRoots(QStringList{ root }, 3, {}, 50, envs);

    // The root itself is walked even though a parent segment is a skip dir.
    QCOMPARE(envs.size(), 1);
}

namespace {
// A minimal detected-but-empty provider: reports a tool on PATH but its
// locate() derives no environments.
class EmptyToolProvider : public IProvider {
public:
    PluginId id;
    ManagerKind managerKind{ id, Ecosystem::Cpp };
    ProviderResult<QVector<ToolInstance>> detect() override
    {
        const QString path = pb::findOnPath(QStringLiteral("cmd"));
        if (path.isEmpty())
            return ProviderResult<QVector<ToolInstance>>{ {}, ProviderError::ToolNotFound, true };
        ProviderResult<QVector<ToolInstance>> r;
        r.value.append(ToolInstance(managerKind, QStringLiteral("empty:tool"), path));
        return r;
    }
    ProviderResult<QVector<Environment>> locate(const ToolInstance&) override { return {}; }
    ProviderResult<QVector<Environment>> locateAt(const QString&) override { return {}; }
    ProviderResult<QString> fingerprint(const Environment&) override { return {}; }
    ProviderResult<ListResult> list(const Environment&, bool) override { return {}; }
    const PluginId& pluginId() const override { return id; }
    const ManagerKind& kind() const override { return managerKind; }
    bool hasCapability(Capability) const override { return true; }
};
} // namespace

void ShellScanTest::managerWithoutEnvironmentsIsStillShown()
{
    // A manager whose executable is on PATH but which yields no environments
    // must still appear in the Global section (as a placeholder environment),
    // so the user sees the manager instead of it being silently hidden.
    EmptyToolProvider provider;
    Scanner scanner(QVector<IProvider*>{ &provider });
    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanGlobals(envs, tools);

    // `cmd` is present on Windows PATH; the provider detects it but derives no
    // environments, and the scanner must surface a system placeholder for it.
    // On POSIX `cmd` may be absent, in which case detect() fails and nothing
    // is expected.
    if (tools.isEmpty())
        return; // executable not on PATH here: nothing to show, that's fine
    QVERIFY(!envs.isEmpty());
    QCOMPARE(envs[0].scope(), Scope::System);
}

QTEST_MAIN(ShellScanTest)
#include "test_scan.moc"