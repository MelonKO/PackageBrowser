#include <QtTest/QtTest>

#include "domain/Environment.h"
#include "domain/EnvironmentId.h"
#include "domain/Installation.h"
#include "domain/ManagerKind.h"
#include "domain/PluginId.h"

class DomainModelTest : public QObject
{
    Q_OBJECT

private slots:
    void environmentIdIsDeterministic();
    void environmentIdDistinguishesRoots();
    void installationIdentityPair();
    void managerKindEquality();
};

void DomainModelTest::environmentIdIsDeterministic()
{
    const auto a = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                             QStringLiteral("/work/app/.venv"));
    const auto b = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                             QStringLiteral("/work/app/.venv"));
    QVERIFY(a.isValid());
    QCOMPARE(a, b);
}

void DomainModelTest::environmentIdDistinguishesRoots()
{
    const auto a = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                             QStringLiteral("/work/app/.venv"));
    const auto b = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                             QStringLiteral("/work/other/.venv"));
    QVERIFY(a != b);
}

void DomainModelTest::installationIdentityPair()
{
    const auto id = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                              QStringLiteral("/work/app/.venv"));
    const Installation inst(id, QStringLiteral("requests"), QStringLiteral("Requests"),
                            QStringLiteral("2.31.0"));
    QCOMPARE(inst.envId(), id);
    QCOMPARE(inst.nameNorm(), QStringLiteral("requests"));
    QCOMPARE(inst.nameDisplay(), QStringLiteral("Requests"));
    QCOMPARE(inst.version(), QStringLiteral("2.31.0"));
    QVERIFY(!inst.isDirect().has_value());
}

void DomainModelTest::managerKindEquality()
{
    const ManagerKind a(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    const ManagerKind b(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    const ManagerKind c(PluginId(QStringLiteral("builtin.npm")), Ecosystem::JavaScript);
    QCOMPARE(a, b);
    QVERIFY(a != c);
    QCOMPARE(ecosystemToString(Ecosystem::Python), QStringLiteral("python"));
    QCOMPARE(ecosystemToString(Ecosystem::Cpp), QStringLiteral("cpp"));
}

QTEST_MAIN(DomainModelTest)
#include "test_domain.moc"