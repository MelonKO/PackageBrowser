#include <QtTest/QtTest>

#include "plugin/PluginManifest.h"
#include "provider/DeclarativeProvider.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

// A manager described only by its manifest must be fully usable, including the
// offline file-based path that keeps foreign project code from running.
class DeclarativeProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void readsProjectEnvironmentFromFiles();
    void fingerprintChangesWhenPackagesChange();
    void declaresCapabilitiesItCanHonour();
    void refusesToRunCommandInUntrustedProject();
    void reportsToolNotFoundWhenExecutableMissing();
    void exposesScannerHints();

private:
    static QString projectManifest();
    static DeclarativeProvider makeProvider(QString* error = nullptr);
};

// Manifest of a fictional CLI manager. Its project environments are read from
// files, so the whole test runs offline without any executable present.
static QString projectManifestText()
{
    return QStringLiteral(R"toml(
[plugin]
id = "testkit.pkg"
api = 1
name = "testkit"
ecosystem = "javascript"
capabilities = ["detect", "locate", "fingerprint", "list"]

[tool]
executables = ["testkit-not-a-real-executable"]
version_cmd = ["{exe}", "--version"]
version_regex = "^(?P<version>\\S+)"

[[locator]]
scope = "project"
marker_file = "pkg.json"
require_dir = "vendor"
[locator.list_files]
dir = "vendor"
manifest = "desc.json"
name = "name"
version = "version"
)toml");
}

QString DeclarativeProviderTest::projectManifest() { return projectManifestText(); }

DeclarativeProvider DeclarativeProviderTest::makeProvider(QString* error)
{
    QString localError;
    auto manifest = PluginManifest::parse(projectManifest(), QStringLiteral("testkit/plugin.toml"),
                                        &localError);
    if (!manifest) {
        if (error)
            *error = localError;
        return DeclarativeProvider(PluginManifest());
    }
    if (error)
        error->clear();
    return DeclarativeProvider(*manifest);
}

// Build <root>/pkg.json plus <root>/vendor/<package>/desc.json.
static void writeProject(const QString& root, const QString& package, const QString& version)
{
    QDir().mkpath(root);
    QFile marker(QDir(root).filePath(QStringLiteral("pkg.json")));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&marker) << "{\n  \"name\": \"demo\"\n}\n";
        marker.close();
    }
    const QDir packageDir(QDir(root).filePath(QStringLiteral("vendor/") + package));
    QDir().mkpath(packageDir.path());
    QFile desc(packageDir.filePath(QStringLiteral("desc.json")));
    if (desc.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&desc) << "{\n  \"name\": \"" << package << "\",\n  \"version\": \"" << version
                           << "\"\n}\n";
        desc.close();
    }
}

void DeclarativeProviderTest::readsProjectEnvironmentFromFiles()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString projectRoot = work.filePath(QStringLiteral("app"));
    writeProject(projectRoot, QStringLiteral("Left-Pad"), QStringLiteral("1.3.0"));

    QString error;
    DeclarativeProvider provider = makeProvider(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const auto located = provider.locateAt(projectRoot);
    QVERIFY(located.ok());
    QCOMPARE(located.value.size(), 1);
    QCOMPARE(located.value[0].scope(), Scope::Project);
    QCOMPARE(located.value[0].root(), QDir(projectRoot).canonicalPath());
    // The provider never assumes trust; the scanner upgrades the status.
    QCOMPARE(located.value[0].status(), EnvStatus::Untrusted);

    const auto listed = provider.list(located.value[0]);
    QVERIFY(listed.ok());
    QCOMPARE(listed.value.size(), 1);
    QCOMPARE(listed.value[0].nameNorm(), QStringLiteral("left-pad"));
    QCOMPARE(listed.value[0].nameDisplay(), QStringLiteral("Left-Pad"));
    QCOMPARE(listed.value[0].version(), QStringLiteral("1.3.0"));
    QVERIFY(listed.value[0].location().endsWith(QStringLiteral("desc.json")));
}

void DeclarativeProviderTest::fingerprintChangesWhenPackagesChange()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString projectRoot = work.filePath(QStringLiteral("app"));
    writeProject(projectRoot, QStringLiteral("left-pad"), QStringLiteral("1.3.0"));

    DeclarativeProvider provider = makeProvider();
    const auto located = provider.locateAt(projectRoot);
    QVERIFY(located.ok() && located.value.size() == 1);

    const auto before = provider.fingerprint(located.value[0]);
    QVERIFY(before.ok());

    writeProject(projectRoot, QStringLiteral("right-pad"), QStringLiteral("1.0.0"));

    const auto after = provider.fingerprint(located.value[0]);
    QVERIFY(after.ok());
    QVERIFY(before.value != after.value);
}

void DeclarativeProviderTest::declaresCapabilitiesItCanHonour()
{
    DeclarativeProvider provider = makeProvider();
    QVERIFY(provider.hasCapability(Capability::Detect));
    QVERIFY(provider.hasCapability(Capability::Locate));
    QVERIFY(provider.hasCapability(Capability::List));
    // Files are read directly, so a cheap fingerprint exists.
    QVERIFY(provider.hasCapability(Capability::Fingerprint));

    // A manifest whose packages are read by running a command cannot offer a
    // cheap fingerprint: the capability is dropped instead of faked.
    const QString commandOnly = QStringLiteral(R"toml(
[plugin]
id = "testkit.cmd"
api = 1
name = "testkit-cmd"
ecosystem = "javascript"
capabilities = ["detect", "locate", "fingerprint", "list"]

[tool]
executables = ["testkit-not-a-real-executable"]

[[locator]]
scope = "project"
marker_file = "pkg.json"
[locator.list_json]
command = ["{exe}", "ls", "--json"]
)toml");
    auto manifest = PluginManifest::parse(commandOnly, QStringLiteral("cmd/plugin.toml"), nullptr);
    QVERIFY(manifest.has_value());
    DeclarativeProvider commandProvider(*manifest);
    QVERIFY(!commandProvider.hasCapability(Capability::Fingerprint));
    QVERIFY(commandProvider.hasCapability(Capability::List));
}

void DeclarativeProviderTest::refusesToRunCommandInUntrustedProject()
{
    const QString commandOnly = QStringLiteral(R"toml(
[plugin]
id = "testkit.cmd"
api = 1
name = "testkit-cmd"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["testkit-not-a-real-executable"]

[[locator]]
scope = "project"
marker_file = "pkg.json"
[locator.list_json]
command = ["{exe}", "ls", "--json"]
ok_exit_codes = [0, 1]
)toml");
    auto manifest = PluginManifest::parse(commandOnly, QStringLiteral("cmd/plugin.toml"), nullptr);
    QVERIFY(manifest.has_value());
    DeclarativeProvider provider(*manifest);

    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString projectRoot = work.filePath(QStringLiteral("app"));
    QDir().mkpath(projectRoot);
    QFile marker(QDir(projectRoot).filePath(QStringLiteral("pkg.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    const auto located = provider.locateAt(projectRoot);
    QVERIFY(located.ok());
    QCOMPARE(located.value.size(), 1);
    QCOMPARE(located.value[0].status(), EnvStatus::Untrusted);

    // Listing would execute a tool inside an untrusted project, and this
    // locator offers no file-based fallback, so it must refuse.
    const auto listed = provider.list(located.value[0]);
    QVERIFY(!listed.ok());
    QCOMPARE(listed.error, ProviderError::Untrusted);
}

void DeclarativeProviderTest::reportsToolNotFoundWhenExecutableMissing()
{
    DeclarativeProvider provider = makeProvider();
    const auto detected = provider.detect();
    QVERIFY(!detected.ok());
    QCOMPARE(detected.error, ProviderError::ToolNotFound);
}

void DeclarativeProviderTest::exposesScannerHints()
{
    DeclarativeProvider provider = makeProvider();
    QCOMPARE(provider.projectMarkers(), QStringList({ QStringLiteral("pkg.json") }));
    QCOMPARE(provider.skipDirs(), QStringList({ QStringLiteral("vendor") }));
}

QTEST_MAIN(DeclarativeProviderTest)
#include "test_declarative_provider.moc"
