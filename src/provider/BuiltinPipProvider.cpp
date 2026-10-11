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
    if (!proc.waitForFinished(15000))
        return std::nullopt;
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return std::nullopt;
    return proc.readAllStandardOutput();
}

QString normalizedKey(const QString& name) { return Pep503Normalizer::normalize(name); }

// Parse the JSON lines emitted by `pip list --format=json`.
QVector<Installation> parsePipJson(const QByteArray& json, const EnvironmentId& envId)
{
    QVector<Installation> out;
    // Minimal robust parse that tolerates trailing commas.
    QString errors;
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
            // METADATA starts with "Name: <name>" then "Version: <version>".
            // Read both lines while the stream is still attached to the device.
            QTextStream ts(&file);
            const QString first = ts.readLine();
            const QString versionLine = ts.readLine();
            file.close();
            if (!first.startsWith(QLatin1String("Name: ")))
                continue;
            const QString name = first.mid(6).trimmed();
            QString version;
            if (versionLine.startsWith(QLatin1String("Version: ")))
                version = versionLine.mid(9).trimmed();
            if (name.isEmpty())
                continue;
            out.append(Installation(envId, normalizedKey(name), name, version, metaPath));
        }
        break; // use the first existing layout
    }
    return out;
}

bool isTrusted(const QString& root, const QStringList& trustedRoots)
{
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    if (canonicalRoot.isEmpty())
        return false;
    for (const QString& r : trustedRoots) {
        if (r.isEmpty())
            continue;
        const QString canonicalTrusted = QFileInfo(r).canonicalFilePath();
        if (canonicalTrusted.isEmpty())
            continue;
        // Same directory, or a genuine descendant (separator boundary), so
        // that C:/work/app does not treat C:/work/apple as trusted.
        if (canonicalRoot == canonicalTrusted
            || (canonicalRoot.startsWith(canonicalTrusted + QLatin1Char('/'))))
            return true;
    }
    return false;
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

void BuiltinPipProvider::setTrustedRoots(const QStringList& roots)
{
    m_trustedRoots = roots;
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
        const QString toolPath = QDir(envRoot).filePath(QLatin1String("Scripts/pip.exe"));
        const EnvStatus status = isTrusted(envRoot, m_trustedRoots) ? EnvStatus::Ok : EnvStatus::Untrusted;
        result.value.append(
            Environment(envId, kind(), Scope::Project, envRoot, toolId, QString(), status));
        if (status == EnvStatus::Ok) {
            m_envToolPaths.insert(envId.value(),
                                  QFileInfo::exists(QDir(envRoot).filePath(QLatin1String("Scripts/pip.exe")))
                                      ? QDir(envRoot).filePath(QLatin1String("Scripts/pip.exe"))
                                      : QDir(envRoot).filePath(QLatin1String("bin/pip")));
        }
    }
    return result;
}

ProviderResult<QString> BuiltinPipProvider::fingerprint(const Environment& env)
{
    ProviderResult<QString> result;
    // Cheap disk-based fingerprint: hash the sorted set of *.dist-info names
    // under the site-packages layouts. Changes when the installed set changes,
    // and does not execute anything.
    QStringList names;
    for (const QString& candidate : { QDir(env.root()).filePath(QLatin1String("Lib/site-packages")),
                                      QDir(env.root()).filePath(QLatin1String("lib/python3/site-packages")) }) {
        const QDir dir(candidate);
        if (!dir.exists())
            continue;
        names << dir.entryList({ QStringLiteral("*.dist-info") },
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