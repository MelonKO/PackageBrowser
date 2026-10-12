#include "process/ProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace pb {

QString findOnPath(const QString& exeName)
{
    const QStringList paths = qEnvironmentVariable("PATH")
                                  .split(QDir::listSeparator(), Qt::SkipEmptyParts);

    // On Windows an executable may be a PATHEXT script (.cmd, .ps1, ...) with no
    // matching .exe (e.g. npm/yarn shims). Enumerate PATHEXT so those are found
    // like any other executable; elsewhere no extension is involved.
    QStringList suffixes;
    suffixes << QString(); // the bare name itself
#ifdef Q_OS_WIN
    suffixes << qEnvironmentVariable("PATHEXT")
                   .split(QLatin1Char(';'), Qt::SkipEmptyParts);
#endif

    for (const QString& dir : paths) {
        for (const QString& ext : suffixes) {
            const QFileInfo fi(QDir(dir).filePath(exeName + ext));
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                return fi.canonicalFilePath();
        }
    }
    return QString();
}

ProcessResult runProcess(const QString& exe, const QStringList& args, const QString& workingDir)
{
    ProcessResult result;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    if (!workingDir.isEmpty())
        proc.setWorkingDirectory(workingDir);
    proc.start(exe, args);
    if (!proc.waitForStarted(3000))
        return result;
    result.started = true;

    if (!proc.waitForFinished(15000)) {
        proc.kill();
        proc.waitForFinished(3000);
        return result;
    }

    result.finished = proc.exitStatus() == QProcess::NormalExit;
    result.exitCode = proc.exitCode();
    result.stdOut = proc.readAllStandardOutput();
    result.stdErr = proc.readAllStandardError();
    return result;
}

} // namespace pb
