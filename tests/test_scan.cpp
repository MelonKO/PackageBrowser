#include <QtTest/QtTest>

#include "provider/BuiltinPipProvider.h"
#include "scanner/Scanner.h"

#include <QTemporaryDir>
#include <QDir>

class ShellScanTest : public QObject
{
    Q_OBJECT

private slots:
    void locateVenvFromScannedRoot();
    void untrustedReadsFromFiles();
    void trustedRunsTool();
};

// Build a minimal venv layout with a *.dist-info/METADATA under site-packages.
static QString makeBareVenv(const QString& root)
{
    const QDir site(QDir(root).filePath(QStringLiteral("Lib/site-packages")));
    site.mkpath(QStringLiteral("requests.dist-info"));
    {
        QFile meta(site.filePath(QStringLiteral("requests.dist-info/METADATA")));
        if (meta.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&meta);
            // Real pip METADATA starts with Metadata-Version, then Name, Version.
            ts << "Metadata-Version: 2.1\n"
               << "Name: Requests\n"
               << "Version: 2.31.0\n";
            meta.close();
        }
    }
    {
        QFile cfg(QDir(root).filePath(QStringLiteral("pyvenv.cfg")));
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&cfg);
            ts << "home = /usr\n";
            cfg.close();
        }
    }
    return root;
}

void ShellScanTest::locateVenvFromScannedRoot()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));

    BuiltinPipProvider provider;
    Scanner scanner(&provider);
    QVector<Environment> envs;
    QVector<ToolInstance> tools;
    scanner.scanRoots(QStringList{ work.filePath(QStringLiteral("app")) }, 3, {}, 50, envs);

    qWarning("scanRoots found %d env(s)", envs.size());
    for (const Environment& e : envs)
        qWarning("  -> root=%s scope=%d status=%d", qPrintable(e.root()), int(e.scope()), int(e.status()));

    QCOMPARE(envs.size(), 1);
    QCOMPARE(envs[0].scope(), Scope::Project);
    QCOMPARE(envs[0].root(), QDir(venvRoot).canonicalPath());
    // Not trusted by default -> Untrusted.
    QCOMPARE(envs[0].status(), EnvStatus::Untrusted);
}

void ShellScanTest::untrustedReadsFromFiles()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));

    BuiltinPipProvider provider;
    const auto envId = EnvironmentId::fromHashed(provider.pluginId().value(),
                                                 QDir(venvRoot).canonicalPath());
    const QString toolId = QStringLiteral("builtin.pip:venv:xyz");
    const Environment env(envId, provider.kind(), Scope::Project,
                          QDir(venvRoot).canonicalPath(), toolId, QString(), EnvStatus::Untrusted);

    const auto result = provider.list(env);
    QVERIFY(result.ok());
    QCOMPARE(result.value.size(), 1);
    QCOMPARE(result.value[0].nameNorm(), QStringLiteral("requests"));
    QCOMPARE(result.value[0].nameDisplay(), QStringLiteral("Requests"));
    QCOMPARE(result.value[0].version(), QStringLiteral("2.31.0"));
}

void ShellScanTest::trustedRunsTool()
{
    // Without a real pip executable in a trusted root we cannot verify running;
    // at minimum it must not crash and should report an error rather than files.
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString venvRoot = makeBareVenv(work.filePath(QStringLiteral("app/.venv")));

    BuiltinPipProvider provider;
    provider.setTrustedRoots({ work.filePath(QStringLiteral("app")) });
    const auto envId = EnvironmentId::fromHashed(provider.pluginId().value(),
                                                 QDir(venvRoot).canonicalPath());
    const QString toolId = QStringLiteral("builtin.pip:venv:%1").arg(envId.value());
    const Environment env(envId, provider.kind(), Scope::Project,
                          QDir(venvRoot).canonicalPath(), toolId, QString(), EnvStatus::Ok);

    const auto result = provider.list(env);
    // No real pip exists yet, so it fails cleanly instead of parsing files.
    QVERIFY(!result.ok());
}

QTEST_MAIN(ShellScanTest)
#include "test_scan.moc"