#include <QtTest/QtTest>

#include "process/ProcessRunner.h"

#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

// findOnPath must resolve executables the way the OS does: on Windows this
// includes PATHEXT scripts (.cmd/.bat/.ps1) that have no .exe twin (npm/yarn
// shims), in addition to the bare name and .exe found before.
class ProcessRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void findsBareExecutableByName();
    void findsPathextScriptWithoutExe();
};

void ProcessRunnerTest::findsBareExecutableByName()
{
    // On POSIX an executable is marked by its permission bits; on Windows the
    // notion of "executable" is extension-based, so the bare-name check with an
    // executable bit only makes sense off-Windows.
#ifndef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString exe = dir.filePath(QStringLiteral("tool"));
    QFile f(exe);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\necho hi\n");
    f.close();
    QVERIFY(QFile::setPermissions(exe, QFile::ReadOwner | QFile::ExeOwner));

    // Put the directory on PATH for this process and look up the bare name.
    const QString prefix = dir.path() + QDir::listSeparator();
    const QString oldPath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("PATH"));
    qputenv("PATH", (prefix + oldPath).toUtf8());

    const QString found = pb::findOnPath(QStringLiteral("tool"));
    QVERIFY(!found.isEmpty());
    QCOMPARE(QDir::cleanPath(found), QDir::cleanPath(dir.filePath(QStringLiteral("tool"))));
#else
    QSKIP("bare executable bit is not a Windows notion");
#endif
}

void ProcessRunnerTest::findsPathextScriptWithoutExe()
{
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString exe = dir.filePath(QStringLiteral("sometool.cmd"));
    QFile f(exe);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("@echo off\n");
    f.close();

    // Add the temp dir to PATH and PATHEXT for this process, then look up the
    // bare name: it must resolve to the .cmd script even though no .exe exists.
    const QString pathSep = QDir::listSeparator();
    const QStringList oldPath = QProcessEnvironment::systemEnvironment().value(
                                    QStringLiteral("PATH")).split(pathSep, Qt::SkipEmptyParts);
    qputenv("PATH", (dir.path() + pathSep + oldPath.join(pathSep)).toUtf8());

    const QStringList oldPathext = qEnvironmentVariable("PATHEXT")
                                       .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QStringList newPathext = oldPathext;
    if (!newPathext.contains(QStringLiteral(".cmd"), Qt::CaseInsensitive))
        newPathext << QStringLiteral(".CMD");
    qputenv("PATHEXT", newPathext.join(QLatin1Char(';')).toUtf8());

    const QString found = pb::findOnPath(QStringLiteral("sometool"));
    QVERIFY(!found.isEmpty());
    QVERIFY(found.endsWith(QStringLiteral("sometool.cmd"), Qt::CaseInsensitive));
#endif
}

QTEST_MAIN(ProcessRunnerTest)
#include "test_process_runner.moc"