#include <QtTest/QtTest>

#include <toml++/toml.hpp>

#include <QString>

// Smoke test that toml++ is wired in through FetchContent and parses the
// manifest shapes the plugin format relies on: tables, arrays of tables,
// arrays of strings, integers and booleans.
class TomlParsingTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesManifestShapes();
    void reportsSyntaxError();
};

static const char* kManifest = R"toml(
[plugin]
id = "builtin.npm"
api = 1
name = "npm"
ecosystem = "javascript"
capabilities = ["detect", "locate", "fingerprint", "list"]

[tool]
executables = ["npm", "npm.cmd"]
version_cmd = ["{exe}", "--version"]
version_regex = "^(?P<version>\\S+)"

[[locator]]
scope = "project"
marker_file = "package.json"
require_dir = "node_modules"

[[locator]]
scope = "system"
from = "path"
)toml";

void TomlParsingTest::parsesManifestShapes()
{
    toml::table tbl = toml::parse(kManifest);

    const toml::table* plugin = tbl["plugin"].as_table();
    QVERIFY(plugin != nullptr);
    QCOMPARE(QString::fromStdString((*plugin)["id"].value_or<std::string>("")),
             QStringLiteral("builtin.npm"));
    QCOMPARE((*plugin)["api"].value_or<int>(-1), 1);

    const toml::array* capabilities = (*plugin)["capabilities"].as_array();
    QVERIFY(capabilities != nullptr);
    QCOMPARE(capabilities->size(), size_t(4));
    QCOMPARE(QString::fromStdString(capabilities->at(0).value_or<std::string>("")),
             QStringLiteral("detect"));

    const toml::table* tool = tbl["tool"].as_table();
    QVERIFY(tool != nullptr);
    QCOMPARE((*tool)["executables"].as_array()->size(), size_t(2));
    QVERIFY((*tool)["version_regex"].value_or<std::string>("").find("version") != std::string::npos);

    const toml::array* locators = tbl["locator"].as_array();
    QVERIFY(locators != nullptr);
    QCOMPARE(locators->size(), size_t(2));
    const toml::table* project = locators->at(0).as_table();
    QVERIFY(project != nullptr);
    QCOMPARE(QString::fromStdString((*project)["scope"].value_or<std::string>("")),
             QStringLiteral("project"));
    QCOMPARE(QString::fromStdString(locators->at(1).as_table()->operator[]("scope").value_or<std::string>("")),
             QStringLiteral("system"));
}

void TomlParsingTest::reportsSyntaxError()
{
    // A manifest with a broken assignment must throw, not silently degrade:
    // the manifest layer maps this to a "Rejected" plugin state.
    bool threw = false;
    try {
        toml::parse(std::string_view("[locator\nscope = "), std::string_view("broken.toml"));
    } catch (const toml::parse_error&) {
        threw = true;
    }
    QVERIFY(threw);
}

QTEST_MAIN(TomlParsingTest)
#include "test_toml.moc"
