#include <QtTest/QtTest>

#include "normalization/Pep503Normalizer.h"

class Pep503NormalizerTest : public QObject
{
    Q_OBJECT

private slots:
    void lowercases();
    void collapsesSeparators();
    void preservesDashes();
    void handlesLeadingAndTrailingSeparators();
};

void Pep503NormalizerTest::lowercases()
{
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("Requests")), QStringLiteral("requests"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("URLLIB3")), QStringLiteral("urllib3"));
}

void Pep503NormalizerTest::collapsesSeparators()
{
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo_bar")), QStringLiteral("foo-bar"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo.bar")), QStringLiteral("foo-bar"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo--bar")), QStringLiteral("foo-bar"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo_._-bar")), QStringLiteral("foo-bar"));
}

void Pep503NormalizerTest::preservesDashes()
{
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo-bar")), QStringLiteral("foo-bar"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("a-b-c")), QStringLiteral("a-b-c"));
}

void Pep503NormalizerTest::handlesLeadingAndTrailingSeparators()
{
    // PEP 503 strips leading/trailing separator runs.
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("-foo")), QStringLiteral("foo"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("foo-")), QStringLiteral("foo"));
    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("--foo_")), QStringLiteral("foo"));
}

QTEST_MAIN(Pep503NormalizerTest)
#include "test_pep503.moc"