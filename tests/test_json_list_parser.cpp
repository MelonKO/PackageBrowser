#include <QtTest/QtTest>

#include "normalization/NameNormalizer.h"
#include "provider/JsonListParser.h"

#include <QJsonDocument>

// Manager output shapes are read generically, so both container layouts that
// real managers emit must work: an array of package objects (pip) and an object
// keyed by package name (npm ls "dependencies").
class JsonListParserTest : public QObject
{
    Q_OBJECT

private slots:
    void readsArrayContainerFromPipStyleOutput();
    void readsObjectContainerFromNpmStyleOutput();
    void navigatesArrayPath();
    void missingArrayPathYieldsUndefined();
    void rejectsScalarContainer();
    void normalizesNamesPerEcosystem();
};

static QJsonValue parseJson(const char* json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json));
    if (doc.isArray())
        return QJsonValue(doc.array());
    return QJsonValue(doc.object());
}

void JsonListParserTest::readsArrayContainerFromPipStyleOutput()
{
    const QJsonValue document = parseJson(R"([{"name":"Requests","version":"2.31.0"},
                                          {"name":"urllib3","version":"2.2.1"}])");
    QVector<Installation> out;
    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"), QStringLiteral("/venv"));

    QVERIFY(pb::extractInstallations(document, QStringLiteral("name"), QStringLiteral("version"),
                                     Ecosystem::Python, envId, out));
    QCOMPARE(out.size(), 2);
    QCOMPARE(out[0].nameNorm(), QStringLiteral("requests"));
    QCOMPARE(out[0].nameDisplay(), QStringLiteral("Requests"));
    QCOMPARE(out[0].version(), QStringLiteral("2.31.0"));
    QCOMPARE(out[1].nameNorm(), QStringLiteral("urllib3"));
}

void JsonListParserTest::readsObjectContainerFromNpmStyleOutput()
{
    // npm ls -g --json --depth=0 reports dependencies as an object keyed by name.
    const QJsonValue document = parseJson(R"({"name":"global","dependencies":{
        "left-pad":{"version":"1.3.0"},
        "@scope/tool":{"version":"2.0.0"}
    }})");
    const QJsonValue container = pb::jsonAtPath(document, QStringLiteral("dependencies"));
    QVERIFY(container.isObject());

    QVector<Installation> out;
    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.npm"), QStringLiteral("/prefix"));
    QVERIFY(pb::extractInstallations(container, QString(), QStringLiteral("version"),
                                     Ecosystem::JavaScript, envId, out));
    QCOMPARE(out.size(), 2);

    // The key is the package name, and a scoped name keeps its scope.
    // QJsonObject iterates keys in sorted order, so compare by name, not index.
    QHash<QString, QString> versions;
    for (const Installation& inst : out)
        versions.insert(inst.nameNorm(), inst.version());
    QCOMPARE(versions.size(), 2);
    QCOMPARE(versions.value(QStringLiteral("left-pad")), QStringLiteral("1.3.0"));
    QCOMPARE(versions.value(QStringLiteral("@scope/tool")), QStringLiteral("2.0.0"));
}

void JsonListParserTest::navigatesArrayPath()
{
    const QJsonValue document = parseJson(R"({"a":{"b":[1,2,3]}})");
    const QJsonValue found = pb::jsonAtPath(document, QStringLiteral("a.b"));
    QVERIFY(found.isArray());
    QCOMPARE(found.toArray().size(), 3);

    // An empty path returns the document itself.
    QVERIFY(pb::jsonAtPath(document, QString()).isObject());
}

void JsonListParserTest::missingArrayPathYieldsUndefined()
{
    const QJsonValue document = parseJson(R"({"dependencies":{}})");
    QVERIFY(pb::jsonAtPath(document, QStringLiteral("missing.path")).isUndefined());
}

void JsonListParserTest::rejectsScalarContainer()
{
    QVector<Installation> out;
    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.npm"), QStringLiteral("/x"));
    // A scalar where a container is expected must be reported, not treated as
    // an empty package list.
    QVERIFY(!pb::extractInstallations(QJsonValue(QStringLiteral("nope")), QStringLiteral("name"),
                                      QStringLiteral("version"), Ecosystem::JavaScript, envId, out));
}

void JsonListParserTest::normalizesNamesPerEcosystem()
{
    QCOMPARE(normalizeName(Ecosystem::Python, QStringLiteral("Foo_Bar.Baz")), QStringLiteral("foo-bar-baz"));
    QCOMPARE(normalizeName(Ecosystem::JavaScript, QStringLiteral("  Left-Pad ")), QStringLiteral("left-pad"));
    QCOMPARE(normalizeName(Ecosystem::JavaScript, QStringLiteral("@Scope/Tool")), QStringLiteral("@scope/tool"));
}

QTEST_MAIN(JsonListParserTest)
#include "test_json_list_parser.moc"
