#include <QtTest/QtTest>

#include "catalog/CatalogService.h"
#include "normalization/Pep503Normalizer.h"

#include <QTemporaryDir>

class CatalogServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void findPackageByNormalizedName();
};

void CatalogServiceTest::findPackageByNormalizedName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CatalogService svc;
    QVERIFY(svc.initialize(dir.filePath(QStringLiteral("cat.db"))));

    // No scan roots configured and no system pip guaranteed: with an empty
    // environment list, finding a package yields nothing.
    QVERIFY(svc.findPackage(QStringLiteral("requests")).isEmpty());

    QCOMPARE(Pep503Normalizer::normalize(QStringLiteral("Foo_Bar.Baz")),
             QStringLiteral("foo-bar-baz"));
}

QTEST_MAIN(CatalogServiceTest)
#include "test_catalog.moc"