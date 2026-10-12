#include <QtTest/QtTest>

#include "cache/Cache.h"
#include "catalog/CatalogService.h"
#include "normalization/Pep503Normalizer.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>

class CatalogServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void findPackageByNormalizedName();
    void untrustedCommandOnlyProjectStaysUntrusted();
    void findPackageNormalizesPerEcosystem();
};

void CatalogServiceTest::initTestCase()
{
    // QSettings needs a stable org/app to be writable in the tests.
    QCoreApplication::setOrganizationName(QStringLiteral("package-browser-test"));
    QCoreApplication::setApplicationName(QStringLiteral("package-browser-test"));
}

static void writeFile(const QString& path, const char* data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data, qstrlen(data));
}

void CatalogServiceTest::findPackageByNormalizedName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CatalogService svc;
    QVERIFY(svc.initialize(dir.filePath(QStringLiteral("cat.db")), QString()));

    // No scan roots configured and no system pip guaranteed: with an empty
    // environment list, finding a package yields nothing.
    QVERIFY(svc.findPackage(QStringLiteral("requests")).isEmpty());

    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("Foo_Bar.Baz")),
             QStringLiteral("foo-bar-baz"));
}

void CatalogServiceTest::untrustedCommandOnlyProjectStaysUntrusted()
{
    // A command-only declarative plugin (no file-based fallback) running inside
    // an untrusted project must keep EnvStatus::Untrusted, not be downgraded to
    // Error: the meaning is "packages are visible, execution is refused".
    const QString manifestText = QStringLiteral(R"toml(
[plugin]
id = "testkit.cmd-only"
api = 1
name = "cmd-only"
ecosystem = "python"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["not-a-real-executable"]

[[locator]]
scope = "project"
marker_file = "pkg.json"
[locator.list_json]
command = ["{exe}", "list", "--json"]
)toml");

    QTemporaryDir base;
    QVERIFY(base.isValid());
    // A project that is NOT in the trusted roots: untrusted.
    const QString projectRoot = base.filePath(QStringLiteral("project"));
    QVERIFY(QDir().mkpath(projectRoot));
    QFile marker(QDir(projectRoot).filePath(QStringLiteral("pkg.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    const QString pluginDir = base.filePath(QStringLiteral("plugins"));
    writeFile(pluginDir + QStringLiteral("/cmd/plugin.toml"),
              manifestText.toUtf8().constData());

    const QString dbPath = base.filePath(QStringLiteral("cat.db"));
    {
        CatalogService svc;
        QVERIFY(svc.initialize(dbPath, pluginDir));
        svc.settings().setScanRoots(QStringList{ projectRoot });
        svc.settings().setTrustedRoots({}); // nothing trusted
        // rescan() runs synchronously; the command-only provider refuses to
        // execute inside the untrusted project.
        svc.rescan();
    }

    // Read back from the cache (separate connection to the same file).
    Cache cache(dbPath);
    QVERIFY(cache.open());
    const QVector<Environment> envs = cache.environments();
    // The system pip (if present) also lands in the cache; locate the project
    // environment by its canonical root.
    const Environment* project = nullptr;
    const QString canonicalProject = QDir(projectRoot).canonicalPath();
    for (const Environment& e : envs)
        if (e.root() == canonicalProject)
            project = &e;
    QVERIFY(project != nullptr);
    QCOMPARE(project->scope(), Scope::Project);
    // The distinguishing regression: Untrusted, not Error.
    QCOMPARE(project->status(), EnvStatus::Untrusted);
}

void CatalogServiceTest::findPackageNormalizesPerEcosystem()
{
    // A package installed in a JavaScript environment is normalized with JS
    // rules (@scope/ preserved, case-folded). findPackage must therefore match
    // "@Scope/Tool" against the stored "@scope/tool".
    const QString manifestText = QStringLiteral(R"toml(
[plugin]
id = "testkit.js"
api = 1
name = "js"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["never-run-js"]

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

    QTemporaryDir base;
    QVERIFY(base.isValid());
    const QString projectRoot = base.filePath(QStringLiteral("project"));
    QVERIFY(QDir().mkpath(projectRoot));
    QFile marker(QDir(projectRoot).filePath(QStringLiteral("package.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    // The declarative list_files reads node_modules/<name>/package.json on one
    // level (it does not recurse into scoped @scope/name), so use a plain name:
    // the point of this test is the per-ecosystem JS normalization, which drops
    // the Python-only PEP503 folding and case-folds the query ("Left-Pad" is
    // stored as "left-pad" without becoming "left-pad" via a hyphen collapse).
    writeFile(QDir(projectRoot).filePath(QStringLiteral("node_modules/Left-Pad/package.json")),
              "{\"name\":\"Left-Pad\",\"version\":\"2.0.0\"}");
    // The project root is trusted only if listed: trust it so the env is Ok,
    // though reading is file-based anyway; key point is the name normalization.
    const QString pluginDir = base.filePath(QStringLiteral("plugins"));
    writeFile(pluginDir + QStringLiteral("/js/plugin.toml"),
              manifestText.toUtf8().constData());

    CatalogService svc;
    const QString dbPath = base.filePath(QStringLiteral("cat.db"));
    QVERIFY(svc.initialize(dbPath, pluginDir));
    svc.settings().setScanRoots(QStringList{ projectRoot });
    svc.settings().setScanMaxDepth(3);
    svc.rescan();

    // Look the package up with a differently-cased spelling: JS normalization is
    // case-folding, so this must match the stored "left-pad".
    const QVector<Installation> found = svc.findPackage(QStringLiteral("LEFT-PAD"));
    QCOMPARE(found.size(), 1);
    QCOMPARE(found[0].nameNorm(), QStringLiteral("left-pad"));
    QCOMPARE(found[0].version(), QStringLiteral("2.0.0"));
}

QTEST_MAIN(CatalogServiceTest)
#include "test_catalog.moc"