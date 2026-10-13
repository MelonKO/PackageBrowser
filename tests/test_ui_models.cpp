#include <QtTest/QtTest>

#include "app/ui/EnvironmentTreeModel.h"
#include "app/ui/InstallationTableModel.h"
#include "app/ui/PackageFilterProxy.h"
#include "app/ui/PluginListModel.h"

#include "domain/EnvironmentId.h"
#include "domain/ManagerKind.h"
#include "domain/PluginId.h"
#include "plugin/PluginHost.h"
#include "settings/AppSettings.h"

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>

static void writeFile(const QString& path, const char* data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data, qstrlen(data));
}

class UiModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void treeStructure();
    void tableColumns();
    void filterHidesRows();
    void pluginListTogglesEnabledState();
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

void UiModelsTest::pluginListTogglesEnabledState()
{
    // Host with built-in pip (always) plus one process provider loaded from a
    // temp manifest with [provider] exec.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(QDir(dir.filePath(QStringLiteral("vcpkg"))).filePath(QStringLiteral("plugin.toml")),
              "[plugin]\n"
              "id = \"builtin.vcpkg\"\n"
              "api = 1\n"
              "name = \"vcpkg\"\n"
              "ecosystem = \"cpp\"\n"
              "capabilities = [\"detect\", \"locate\", \"list\"]\n"
              "[provider]\n"
              "exec = [\"python\", \"vcpkg_provider.py\"]\n");

    // Route QSettings to a test-only scope so write and read hit the same key
    // and the developer's real package-browser config is left untouched.
    QCoreApplication::setOrganizationName(QStringLiteral("pb-test-org"));
    QCoreApplication::setApplicationName(QStringLiteral("pb-test-app"));
    AppSettings settings;
    settings.setDisabledPluginIds({}); // clean slate before the toggle

    PluginHost host;
    host.load(dir.path());
    QVERIFY(host.providers().size() >= 2);

    PluginListModel model(&host, &settings);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QCOMPARE(model.rowCount(), host.providers().size());

    // Find the vcpkg row and disable it via the model.
    int vcpkgRow = -1;
    for (int i = 0; i < model.rowCount(); ++i) {
        const QString id = model.data(model.index(i, 0), int(PluginListModel::Role::PluginId))
                               .toString();
        if (id == QLatin1String("builtin.vcpkg")) {
            vcpkgRow = i;
            break;
        }
    }
    QVERIFY(vcpkgRow >= 0);

    QVERIFY(model.data(model.index(vcpkgRow, 0), int(PluginListModel::Role::Enabled)).toBool());
    QVERIFY(model.setData(model.index(vcpkgRow, 0), false, Qt::EditRole));
    QVERIFY(!model.data(model.index(vcpkgRow, 0), int(PluginListModel::Role::Enabled)).toBool());
    QVERIFY(!model.isEnabled(QStringLiteral("builtin.vcpkg")));

    // Re-enable.
    QVERIFY(model.setData(model.index(vcpkgRow, 0), true, Qt::EditRole));
    QVERIFY(model.isEnabled(QStringLiteral("builtin.vcpkg")));
}

QTEST_MAIN(UiModelsTest)
#include "test_ui_models.moc"