#include <QtTest/QtTest>

#include "app/ui/EnvironmentTreeModel.h"
#include "app/ui/InstallationTableModel.h"
#include "app/ui/PackageFilterProxy.h"

#include "domain/EnvironmentId.h"
#include "domain/ManagerKind.h"
#include "domain/PluginId.h"

#include <QApplication>
#include <QAbstractItemModelTester>

class UiModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void treeStructure();
    void tableColumns();
    void filterHidesRows();
};

void UiModelsTest::treeStructure()
{
    const ManagerKind pip(PluginId(QStringLiteral("builtin.pip")), Ecosystem::Python);
    const auto sysId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("C:/python"));
    const auto venvId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                  QStringLiteral("C:/work/app/.venv"));
    const QVector<Environment> envs{
        Environment(sysId, pip, Scope::System, QStringLiteral("C:/python")),
        Environment(venvId, pip, Scope::Project, QStringLiteral("C:/work/app/.venv")),
    };

    EnvironmentTreeModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.setEnvironments(envs);

    // Root has two sections: Global and Projects.
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0)).toString(), QStringLiteral("Global"));
    QCOMPARE(model.data(model.index(1, 0)).toString(), QStringLiteral("Projects"));

    // Global contains the system environment.
    const QModelIndex global = model.index(0, 0);
    QCOMPARE(model.rowCount(global), 1);

    // Projects has one group (app) containing the venv.
    const QModelIndex projects = model.index(1, 0);
    QCOMPARE(model.rowCount(projects), 1);
    const QModelIndex group = model.index(0, 0, projects);
    QCOMPARE(model.rowCount(group), 1);
    const QModelIndex venvItem = model.index(0, 0, group);
    const Environment* env = model.environmentAt(venvItem);
    QVERIFY(env);
    QCOMPARE(env->id(), venvId);
}

void UiModelsTest::tableColumns()
{
    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("C:/work/app/.venv"));
    InstallationTableModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.setInstallations({
        Installation(envId, QStringLiteral("requests"), QStringLiteral("Requests"),
                     QStringLiteral("2.31.0")),
    });

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Name"));
    QCOMPARE(model.data(model.index(0, 0)).toString(), QStringLiteral("Requests"));
    QCOMPARE(model.data(model.index(0, 1)).toString(), QStringLiteral("2.31.0"));
}

void UiModelsTest::filterHidesRows()
{
    const auto envId = EnvironmentId::fromHashed(QStringLiteral("builtin.pip"),
                                                 QStringLiteral("C:/work/app/.venv"));
    InstallationTableModel source;
    source.setInstallations({
        Installation(envId, QStringLiteral("requests"), QStringLiteral("Requests"),
                     QStringLiteral("2.31.0")),
        Installation(envId, QStringLiteral("urllib3"), QStringLiteral("urllib3"),
                     QStringLiteral("2.0.7")),
    });

    PackageFilterProxy proxy;
    proxy.setSourceModel(&source);
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setFilterText(QStringLiteral("requests"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("Requests"));

    proxy.setFilterText(QString());
    QCOMPARE(proxy.rowCount(), 2);
}

QTEST_MAIN(UiModelsTest)
#include "test_ui_models.moc"