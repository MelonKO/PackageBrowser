#include "provider/BuiltinPipProvider.h"

#include "normalization/Pep503Normalizer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

#include <optional>

namespace {

const PluginId kPluginId(QStringLiteral("builtin.pip"));

ManagerKind kind() { return ManagerKind(kPluginId, Ecosystem::Python); }

// Locate an executable by name on PATH. On Windows, prefer resolving from the
// same directory (handles both bare names and .exe extensions) so that
// reparse-point shims in WindowsApps are found like any other executable.
QString findOnPath(const QString& exeName)
{
    const QStringList paths = qEnvironmentVariable("PATH")
                                  .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    const char* exts[] = { "", ".exe" }; // ... only relevant on Windows
    for (const QString& dir : paths) {
        for (const char* ext : exts) {
            const QFileInfo fi(QDir(dir).filePath(exeName + QLatin1String(ext)));
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                return fi.canonicalFilePath();
        }
    }
    return QString();
}

// Run a command with a sane timeout and return stdout on success.
std::optional<QByteArray> runProcess(const QString& exe, const QStringList& args)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(exe, args);
    if (!proc.waitForStarted(3000))
        return std::nullopt;
    if (!proc.waitForFinished(15000)) {
        // Terminate a hung child rather than leaving it running.
        proc.kill();
        proc.waitForFinished(3000);
        return std::nullopt;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return std::nullopt;
    return proc.readAllStandardOutput();
}

QString normalizedKey(const QString& name) { return Pep503Normalizer::normalize(name); }

// Parse the JSON array emitted by `pip list --format=json`.
QVector<Installation> parsePipJson(const QByteArray& json, const EnvironmentId& envId)
{
    QVector<Installation> out;
    auto doc = QJsonDocument::fromJson(json);
    if (doc.isNull() || !doc.isArray())
        return out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QLatin1String("name")).toString();
        const QString version = o.value(QLatin1String("version")).toString();
        if (name.isEmpty())
            continue;
        out.append(Installation(envId, normalizedKey(name), name, version));
    }
    return out;
}

// Read a venv's packages by parsing *.dist-info/METADATA files (no execution).
QVector<Installation> readVenvFromFiles(const QString& venvRoot, const EnvironmentId& envId)
{
    QVector<Installation> out;
    QStringList sitePackagesCandidates;
    sitePackagesCandidates << QDir(venvRoot).filePath(QLatin1String("Lib/site-packages")); // Windows
    // POSIX venvs use lib/python3.<minor>/site-packages; match any python3.x.
    const QDir libDir(QDir(venvRoot).filePath(QLatin1String("lib")));
    const QStringList pyDirs = libDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
        .filter(QRegularExpression(QStringLiteral("^python3\\.")));
    for (const QString& py : pyDirs)
        sitePackagesCandidates << libDir.filePath(py + QLatin1String("/site-packages"));

    for (const QString& candidate : sitePackagesCandidates) {
        const QDir dir(candidate);
        if (!dir.exists())
            continue;
        const QStringList distDirs = dir.entryList({ QStringLiteral("*.dist-info") },
                                                   QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& d : distDirs) {
            const QString metaPath = dir.filePath(d + QLatin1String("/METADATA"));
            QFile file(metaPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            // Core Metadata is an RFC-822-style header block: fields may appear
            // in any order, so scan all header lines and pick out Name/Version.
            QTextStream ts(&file);
            QString name;
            QString version;
            const QString lowerNamePrefix = QStringLiteral("name:");
            const QString lowerVersionPrefix = QStringLiteral("version:");
            while (!ts.atEnd()) {
                const QString line = ts.readLine();
                if (line.isEmpty() || line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('\t')))
                    break; // blank line or folded continuation ends the header block
                if (name.isEmpty() && line.startsWith(lowerNamePrefix, Qt::CaseInsensitive))
                    name = line.mid(5).trimmed();
                else if (version.isEmpty() && line.startsWith(lowerVersionPrefix, Qt::CaseInsensitive))
                    version = line.mid(8).trimmed();
            }
            file.close();
            if (name.isEmpty())
                continue;
            out.append(Installation(envId, normalizedKey(name), name, version, metaPath));
        }
        break; // use the first existing layout
    }
    return out;
}

} // namespace

BuiltinPipProvider::BuiltinPipProvider() = default;

const PluginId& BuiltinPipProvider::pluginId() const { return kPluginId; }

const ManagerKind& BuiltinPipProvider::kind() const { static const auto k = ::kind(); return k; }

bool BuiltinPipProvider::hasCapability(Capability c) const
{
    switch (c) {
    case Capability::Detect:
    case Capability::Locate:
    case Capability::Fingerprint:
    case Capability::List:
        return true;
    }
    return false;
}

ProviderResult<QVector<ToolInstance>> BuiltinPipProvider::detect()
{
    ProviderResult<QVector<ToolInstance>> result;
    QString pipPath = findOnPath(QStringLiteral("pip"));
    if (pipPath.isEmpty())
        pipPath = findOnPath(QStringLiteral("pip3"));

    if (pipPath.isEmpty()) {
        result.error = ProviderError::ToolNotFound;
        return result;
    }

    QString version;
    if (auto out = runProcess(pipPath, { QStringLiteral("--version") }); out) {
        const QRegularExpression re(QStringLiteral("pip (\\S+)"));
        const auto m = re.match(QString::fromUtf8(*out));
        if (m.hasMatch())
            version = m.captured(1);
    }

    // Tool id: hash of canonical path (deduplicates pip vs pip3).
    const QString id = QString::fromUtf8(QCryptographicHash::hash(
        (kPluginId.value() + QLatin1Char('\0') + pipPath).toUtf8(),
        QCryptographicHash::Sha1).toHex().left(12));
    result.value.append(ToolInstance(kind(), id, pipPath, version));
    return result;
}

ProviderResult<QVector<Environment>> BuiltinPipProvider::locate(const ToolInstance& tool)
{
    ProviderResult<QVector<Environment>> result;
    if (!tool.kind().pluginId().isValid() || tool.path().isEmpty()) {
        result.error = ProviderError::EnvBroken;
        return result;
    }

    // For a system pip, the environment root is the directory containing the
    // tool (its site-packages live under the interpreter); scope = System.
    QFileInfo fi(tool.path());
    const QString root = fi.dir().path();
    const auto envId = EnvironmentId::fromHashed(kPluginId.value(), root);
    m_envToolPaths.insert(envId.value(), tool.path());
    result.value.append(Environment(envId, kind(), Scope::System, root, tool.id()));
    return result;
}

ProviderResult<QVector<Environment>> BuiltinPipProvider::locateAt(const QString& scanRoot)
{
    ProviderResult<QVector<Environment>> result;
    const QDir dir(scanRoot);
    if (!dir.exists()) {
        result.error = ProviderError::EnvBroken;
        return result;
    }

    // Does this directory carry a venv marker? If so, it is a project environment.
    if (dir.exists(QLatin1String("pyvenv.cfg"))) {
        const QString envRoot = dir.canonicalPath();
        const auto envId = EnvironmentId::fromHashed(kPluginId.value(), envRoot);
        const QString toolId = QStringLiteral("builtin.pip:venv:%1").arg(envId.value());
        // Record the candidate tool path: building it executes nothing, and
        // list() uses it only when the environment is trusted.
        const QString windowsTool = QDir(envRoot).filePath(QLatin1String("Scripts/pip.exe"));
        m_envToolPaths.insert(envId.value(),
                              QFileInfo::exists(windowsTool)
                                  ? windowsTool
                                  : QDir(envRoot).filePath(QLatin1String("bin/pip")));
        // Safe default: no trust information reaches the provider, so a project
        // environment is read by parsing files. The scanner upgrades it to Ok
        // when its root is trusted (see TrustPolicy).
        result.value.append(Environment(envId, kind(), Scope::Project, envRoot, toolId,
                                        QString(), EnvStatus::Untrusted));
    }
    return result;
}

ProviderResult<QString> BuiltinPipProvider::fingerprint(const Environment& env)
{
    ProviderResult<QString> result;
    // Cheap disk-based fingerprint: hash the sorted set of *.dist-info names
    // under all site-packages layouts reachable from the environment's root.
    // Never executes anything and changes when the installed set changes.
    QStringList names;
    const QDir rootDir(env.root());
    const QStringList candidates = {
        rootDir.filePath(QLatin1String("Lib/site-packages")),
        rootDir.filePath(QLatin1String("lib/site-packages")),
        rootDir.filePath(QLatin1String("lib/python3/site-packages")),
        rootDir.filePath(QLatin1String("site-packages")),
    };
    for (const QString& candidate : candidates) {
        const QDir dir(candidate);
        if (!dir.exists())
            continue;
        names << dir.entryList({ QStringLiteral("*.dist-info") },
                               QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }
    // Also cover versioned POSIX layouts under lib/python3.*.
    const QDir libDir(rootDir.filePath(QLatin1String("lib")));
    const QStringList pyDirs = libDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
        .filter(QRegularExpression(QStringLiteral("^python3\\.")));
    for (const QString& py : pyDirs) {
        const QDir sp(libDir.filePath(py + QLatin1String("/site-packages")));
        if (sp.exists())
            names << sp.entryList({ QStringLiteral("*.dist-info") },
                                  QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }
    names.sort();
    const QByteArray joined = names.join(QLatin1Char('\n')).toUtf8();
    result.value = QString::fromUtf8(
        QCryptographicHash::hash(joined, QCryptographicHash::Sha1).toHex());
    return result;
}

ProviderResult<QVector<Installation>> BuiltinPipProvider::list(const Environment& env, bool withDependencies)
{
    ProviderResult<QVector<Installation>> result;
    result.complete = true;
    (void)withDependencies;

    if (env.scope() == Scope::Project && env.status() == EnvStatus::Untrusted) {
        // Untrusted: read files without executing the venv's tool.
        result.value = readVenvFromFiles(env.root(), env.id());
        return result;
    }

    // Trusted (or system): run pip to list packages.
    QString pipPath = m_envToolPaths.value(env.id().value());
    if (pipPath.isEmpty() && env.scope() == Scope::Project) {
        // Trusted venv: the tool must be the venv's own pip; never fall back to
        // a system pip from PATH, otherwise we list the wrong environment.
        const QString candidate = QDir(env.root()).filePath(QLatin1String("Scripts/pip.exe"));
        if (QFileInfo::exists(candidate))
            pipPath = candidate;
        else {
            const QString candidatePosix = QDir(env.root()).filePath(QLatin1String("bin/pip"));
            if (QFileInfo::exists(candidatePosix))
                pipPath = candidatePosix;
        }
    } else if (pipPath.isEmpty()) {
        // System pip: fall back to searching PATH only for System-scope envs.
        pipPath = findOnPath(QStringLiteral("pip"));
        if (pipPath.isEmpty())
            pipPath = findOnPath(QStringLiteral("pip3"));
    }

    if (pipPath.isEmpty()) {
        result.error = ProviderError::ToolNotFound;
        return result;
    }

    auto out = runProcess(pipPath, { QStringLiteral("list"), QStringLiteral("--format=json") });
    if (!out) {
        result.error = ProviderError::EnvBroken;
        result.complete = false;
        return result;
    }
    result.value = parsePipJson(*out, env.id());
    return result;
}