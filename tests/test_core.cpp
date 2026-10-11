#include <QtTest/QtTest>

class CoreSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void sanity();
};

void CoreSmokeTest::sanity()
{
    QVERIFY(true);
}

QTEST_MAIN(CoreSmokeTest)

#include "test_core.moc"