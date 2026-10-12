#include <QtTest/QtTest>

#include "trust/TrustPolicy.h"

#include <QDir>
#include <QTemporaryDir>

// Trust gating decides whether foreign code inside a project is executed, so
// the boundary rules are pinned by tests.
class TrustPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void untrustedWithoutRoots();
    void trustsRootAndDescendants();
    void respectsDirectoryBoundary();
    void ignoresUnresolvableRoots();
    void statusForProjectFollowsTrust();
};

void TrustPolicyTest::untrustedWithoutRoots()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString app = work.filePath(QStringLiteral("app"));
    QVERIFY(QDir().mkpath(app));

    const TrustPolicy policy;
    QVERIFY(!policy.isTrusted(app));
    QCOMPARE(policy.statusForProject(app), EnvStatus::Untrusted);
}

void TrustPolicyTest::trustsRootAndDescendants()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString app = work.filePath(QStringLiteral("app"));
    const QString nested = QDir(app).filePath(QStringLiteral("deeper/.venv"));
    QVERIFY(QDir().mkpath(nested));

    const TrustPolicy policy(QStringList{ app });
    QVERIFY(policy.isTrusted(app));
    QVERIFY(policy.isTrusted(nested)); // a genuine descendant is trusted too
}

void TrustPolicyTest::respectsDirectoryBoundary()
{
    // Regression: a plain string prefix would treat "apple" as inside "app".
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString app = work.filePath(QStringLiteral("app"));
    const QString apple = work.filePath(QStringLiteral("apple"));
    QVERIFY(QDir().mkpath(app));
    QVERIFY(QDir().mkpath(apple));

    const TrustPolicy policy(QStringList{ app });
    QVERIFY(policy.isTrusted(app));
    QVERIFY(!policy.isTrusted(apple));
}

void TrustPolicyTest::ignoresUnresolvableRoots()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString app = work.filePath(QStringLiteral("app"));
    QVERIFY(QDir().mkpath(app));

    // A trusted root that does not exist on disk trusts nothing.
    const TrustPolicy policy(QStringList{ work.filePath(QStringLiteral("missing")) });
    QVERIFY(!policy.isTrusted(app));

    // Nor does a path that cannot be resolved.
    const TrustPolicy emptyRoot(QStringList{ QString() });
    QVERIFY(!emptyRoot.isTrusted(app));
}

void TrustPolicyTest::statusForProjectFollowsTrust()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString app = work.filePath(QStringLiteral("app"));
    QVERIFY(QDir().mkpath(app));

    TrustPolicy policy;
    QCOMPARE(policy.statusForProject(app), EnvStatus::Untrusted);
    policy.setTrustedRoots(QStringList{ app });
    QCOMPARE(policy.statusForProject(app), EnvStatus::Ok);
}

QTEST_MAIN(TrustPolicyTest)
#include "test_trust.moc"
