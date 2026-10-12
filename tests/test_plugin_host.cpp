#include <QtTest/QtTest>

#include "plugin/PluginHost.h"
#include "provider/BuiltinPipProvider.h"
#include "provider/DeclarativeProvider.h"
#include "provider/ProcessProvider.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>

// The host aggregates the built-in pip provider with one DeclarativeProvider
// per valid plugin.toml, and exposes the scanner hints (markers, skip dirs) the
// walk needs so it stops hardcoding pyvenv.cfg / node_modules.
class PluginHostTest : public QObject
{
    Q_OBJECT

private slots:
    void alwaysProvidesBuiltinPip();
    void loadsAManifestProvider();
    void aggregatesScannerHints();
    void skipsDisabledProvider();
    void ignoresMalformedManifestWithoutAborting();
    void loadIsIdempotent();
    void loadsOnlyFilesNamedPluginToml();
    void loadsProcessProviderFromManifest();
};

static void writeFile(const QString& path, const char* data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data, qstrlen(data));
}

void PluginHostTest::alwaysProvidesBuiltinPip()
{
    PluginHost host;
    host.load(QStringLiteral("does-not-exist"));
    QCOMPARE(host.providers().size(), 1);
    QVERIFY(dynamic_cast<BuiltinPipProvider*>(host.providers().at(0)) != nullptr);
}

void PluginHostTest::loadsAManifestProvider()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath(QStringLiteral("npm/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["npm"]

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

    PluginHost host;
    host.load(dir.path());
    QVERIFY(host.providers().size() >= 2);

    const auto* npm = dynamic_cast<DeclarativeProvider*>(host.providers().at(1));
    QVERIFY(npm != nullptr);
    QCOMPARE(npm->pluginId().value(), QStringLiteral("builtin.npm"));
    QVERIFY(npm->hasCapability(Capability::List));
}

void PluginHostTest::aggregatesScannerHints()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath(QStringLiteral("npm/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["npm"]

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

    PluginHost host;
    host.load(dir.path());

    const QStringList markers = host.projectMarkers();
    QVERIFY(markers.contains(QStringLiteral("package.json")));

    const QStringList skip = host.skipDirs();
    QVERIFY(skip.contains(QStringLiteral("node_modules")));
}

void PluginHostTest::skipsDisabledProvider()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath(QStringLiteral("npm/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["npm"]

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

    PluginHost host;
    host.load(dir.path(), { QStringLiteral("builtin.npm") });
    // Only the built-in pip provider remains.
    QCOMPARE(host.providers().size(), 1);
}

void PluginHostTest::ignoresMalformedManifestWithoutAborting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Broken TOML: the host must skip it, keeping the built-in pip provider.
    writeFile(dir.filePath(QStringLiteral("bad/plugin.toml")), "[plugin\nbad");
    writeFile(dir.filePath(QStringLiteral("good/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.good"
api = 1
name = "good"
ecosystem = "python"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["good-tool"]

[[locator]]
scope = "project"
marker_file = "good.json"
[locator.list_files]
dir = "pkg"
manifest = "info.json"
name = "name"
version = "version"
)toml");

    PluginHost host;
    host.load(dir.path());
    QVERIFY(host.providers().size() >= 2);
    const auto* good = dynamic_cast<DeclarativeProvider*>(host.providers().at(1));
    QVERIFY(good != nullptr);
    QCOMPARE(good->pluginId().value(), QStringLiteral("builtin.good"));
}

void PluginHostTest::loadIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.filePath(QStringLiteral("npm/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["npm"]

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

    PluginHost host;
    // Calling load() twice must not accumulate providers or leak the previous set.
    host.load(dir.path());
    const int firstSize = host.providers().size();
    QVERIFY(firstSize >= 2);
    host.load(dir.path());
    QCOMPARE(host.providers().size(), firstSize);
    // The built-in pip provider is still present exactly once.
    int pipCount = 0;
    for (IProvider* p : host.providers())
        if (dynamic_cast<BuiltinPipProvider*>(p))
            ++pipCount;
    QCOMPARE(pipCount, 1);
}

void PluginHostTest::loadsOnlyFilesNamedPluginToml()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A plugin in its own folder...
    writeFile(dir.filePath(QStringLiteral("npm/plugin.toml")),
              R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "list"]

[tool]
executables = ["npm"]

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
    // ...and an unrelated .toml (e.g. editor metadata) that must NOT be loaded.
    writeFile(dir.filePath(QStringLiteral("misc/notes.toml")), "[something]\nkey = 1");

    PluginHost host;
    host.load(dir.path());
    // pip provider + exactly the npm declarative provider.
    QCOMPARE(host.providers().size(), 2);
    const auto* npm = dynamic_cast<DeclarativeProvider*>(host.providers().at(1));
    QVERIFY(npm != nullptr);
    QCOMPARE(npm->pluginId().value(), QStringLiteral("builtin.npm"));
}

void PluginHostTest::loadsProcessProviderFromManifest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A manifest declaring `[provider] exec` must be loaded as a ProcessProvider,
    // not a DeclarativeProvider.
    writeFile(QDir(dir.filePath(QStringLiteral("vcpkg"))).filePath(QStringLiteral("plugin.toml")),
              "[plugin]\n"
              "id = \"builtin.vcpkg\"\n"
              "api = 1\n"
              "name = \"vcpkg\"\n"
              "ecosystem = \"cpp\"\n"
              "capabilities = [\"detect\", \"locate\", \"list\"]\n"
              "[provider]\n"
              "exec = [\"python\", \"vcpkg_provider.py\"]\n");

    PluginHost host;
    host.load(dir.path());
    // builtin.pip + the vcpkg process provider.
    QCOMPARE(host.providers().size(), 2);
    QCOMPARE(host.processProviders().size(), 1);
    QCOMPARE(host.processProviders().first()->pluginId().value(),
             QStringLiteral("builtin.vcpkg"));
}

QTEST_MAIN(PluginHostTest)
#include "test_plugin_host.moc"