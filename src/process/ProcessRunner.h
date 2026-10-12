#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace pb {

// Locate an executable by name on PATH. On Windows this also matches the .exe
// suffix, so reparse-point shims in WindowsApps resolve like any other file.
QString findOnPath(const QString& exeName);

struct ProcessResult {
    bool started = false;
    bool finished = false;
    int exitCode = -1;
    QByteArray stdOut;
    QByteArray stdErr;
};

// Run a command with an argument list (never through a shell) and a timeout,
// killing a hung child instead of leaving it running. The caller decides which
// exit codes mean success, because some managers exit non-zero with valid output.
// See docs/Надёжность и безопасность.md ("Rules").
ProcessResult runProcess(const QString& exe,
                         const QStringList& args,
                         const QString& workingDir = QString());

} // namespace pb
