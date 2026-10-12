#include "process/ProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace pb {

QString findOnPath(const QString& exeName)
{
    const QStringList paths = qEnvironmentVariable("PATH")
                                  .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    const char* exts[] = { "", ".exe" }; // the suffix only matters on Windows
    for (const QString& dir : paths) {
        for (const char* ext : exts) {
            const QFileInfo fi(QDir(dir).filePath(exeName + QLatin1String(ext)));
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
