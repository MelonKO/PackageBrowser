#include <QtTest/QtTest>

#include "plugin/PluginManifest.h"

#include <QString>

// Validation of plugin.toml: a rejected manifest must say why, because a
// silently half-parsed descriptor would break "a manager is one file".
class ManifestTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesNpmManifest();
    void okExitCodesDefaultToZero();
    void rejectsMissingPluginTable();
    void rejectsMissingDottedId();
    void rejectsUnsupportedApi();
    void rejectsUnknownCapability();
    void rejectsUnknownEcosystem();
    void rejectsProjectLocatorWithoutMarker();
    void rejectsSystemLocatorWithoutFromPath();
    void rejectsUnknownPlaceholderInCommand();
    void rejectsLocatorWithoutListSpec();
    void rejectsLocatorWithBothListSpecs();
};

static QString validManifest()
{
    return QStringLiteral(R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "fingerprint", "list"]

[tool]
executables = ["npm"]
version_cmd = ["{exe}", "--version"]

[[locator]]
scope = "project"
marker_file = "package.json"
require_dir = "node_modules"
[locator.list_files]
dir = "node_modules"
manifest = "package.json"
name = "name"
version = "version"

[[locator]]
scope = "system"
from = "path"
[locator.list_json]
command = ["{exe}", "ls", "-g", "--json", "--depth=0"]
ok_exit_codes = [0, 1]
)toml");
}

static std::optional<PluginManifest> parseOrFail(const QString& text, QString* error = nullptr)
{
    QString localError;
    auto manifest = PluginManifest::parse(text, QStringLiteral("test.toml"), &localError);
    if (!manifest && error)
        *error = localError;
    return manifest;
}

void ManifestTest::parsesNpmManifest()
{
    QString error;
    auto manifest = parseOrFail(validManifest(), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    QCOMPARE(manifest->pluginId().value(), QStringLiteral("builtin.npm"));
    QCOMPARE(manifest->apiVersion(), 1);
    QCOMPARE(manifest->name(), QStringLiteral("npm"));
    QCOMPARE(manifest->kind().ecosystem(), Ecosystem::JavaScript);
    QCOMPARE(manifest->capabilities().size(), 4);
    QVERIFY(manifest->hasCapability(Capability::List));
    QCOMPARE(manifest->tool().executables, QStringList{ QStringLiteral("npm") });

    QCOMPARE(manifest->locators().size(), 2);
    const LocatorSpec& project = manifest->locators().at(0);
    QCOMPARE(project.scope, Scope::Project);
    QCOMPARE(project.markerFile, QStringLiteral("package.json"));
    QCOMPARE(project.requireDir, QStringLiteral("node_modules"));
    QVERIFY(project.listFiles.has_value());
    QVERIFY(!project.listJson.has_value());
    QCOMPARE(project.listFiles->dir, QStringLiteral("node_modules"));

    const LocatorSpec& system = manifest->locators().at(1);
    QCOMPARE(system.scope, Scope::System);
    QVERIFY(system.fromPath);
    QVERIFY(system.listJson.has_value());
    QCOMPARE(system.listJson->command.size(), 5);
    QCOMPARE(system.listJson->okExitCodes, QList<int>({ 0, 1 }));
}

void ManifestTest::okExitCodesDefaultToZero()
{
    auto manifest = parseOrFail(validManifest());
    QVERIFY(manifest.has_value());
    // The project locator declares no ok_exit_codes: success means exit 0 only.
    QCOMPARE(manifest->locators().at(0).listFiles->manifest, QStringLiteral("package.json"));
}

void ManifestTest::rejectsMissingPluginTable()
{
    QString error;
    QVERIFY(!parseOrFail(QStringLiteral("[[locator]]\nscope = \"system\"\n"), &error));
    QVERIFY(error.contains(QStringLiteral("[plugin]")));
}

void ManifestTest::rejectsMissingDottedId()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("id = \"builtin.npm\""), QStringLiteral("id = \"npm\""));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("dotted")));
}

void ManifestTest::rejectsUnsupportedApi()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("api = 1"), QStringLiteral("api = 2"));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("api")));
}

void ManifestTest::rejectsUnknownCapability()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("\"fingerprint\", "), QStringLiteral("\"plan\", "));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("plan")));
}

void ManifestTest::rejectsUnknownEcosystem()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("ecosystem = \"javascript\""), QStringLiteral("ecosystem = \"ruby\""));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("ruby")));
}

void ManifestTest::rejectsProjectLocatorWithoutMarker()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("marker_file = \"package.json\"\n"), QString());
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("marker_file")));
}

void ManifestTest::rejectsSystemLocatorWithoutFromPath()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("from = \"path\"\n"), QString());
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("from")));
}

void ManifestTest::rejectsUnknownPlaceholderInCommand()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("\"{exe}\", \"ls\""), QStringLiteral("\"{exec}\", \"ls\""));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("{exec}")));
}

void ManifestTest::rejectsLocatorWithoutListSpec()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("[locator.list_files]\ndir = \"node_modules\"\nmanifest = \"package.json\"\nname = \"name\"\nversion = \"version\"\n"),
                   QString());
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("list_json or list_files")));
}

void ManifestTest::rejectsLocatorWithBothListSpecs()
{
    QString broken = validManifest();
    broken.replace(QStringLiteral("[locator.list_files]"),
                   QStringLiteral("[locator.list_json]\ncommand = [\"{exe}\", \"ls\"]\n[locator.list_files]"));
    QString error;
    QVERIFY(!parseOrFail(broken, &error));
    QVERIFY(error.contains(QStringLiteral("exactly one")));
}

QTEST_MAIN(ManifestTest)
#include "test_manifest.moc"
