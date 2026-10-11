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
#include <QStandardPaths>
#include <QTextStream>

#include <optional>

namespace {

const PluginId kPluginId(QStringLiteral("builtin.pip"));

ManagerKind kind() { return ManagerKind(kPluginId, Ecosystem::Python); }

// Locate an executable by name on PATH, preferring the first hit.
QString findOnPath(const QString& exeName)
{
    const QStringList paths = qEnvironmentVariable("PATH")
                                  .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString& dir : paths) {
        const QFileInfo fi(QDir(dir).filePath(exeName));
        if (fi.exists() && fi.isFile() && fi.isExecutable())
            return fi.canonicalFilePath();
    }
    // Windows fallback: append .exe.
    if (QFileInfo::exists(exeName + QLatin1String(".exe"))) {
        const QStringList paths2 = qEnvironmentVariable("PATH")
                                       .split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (const QString& dir : paths2) {
            const QFileInfo fi(QDir(dir).filePath(exeName + QLatin1String(".exe")));
            if (fi.exists() && fi.isFile())
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
    const QString sitePackages = QDir(venvRoot)
        .filePath(QLatin1String("Lib/site-packages")); // Windows layout
    const QString sitePackagesPosix = QDir(venvRoot)
        .filePath(QLatin1String("lib/python3/site-packages"));
    const QStringList candidates = { sitePackages, sitePackagesPosix };

    for (const QString& candidate : candidates) {
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
            QTextStream ts(&file);
            const QString first = ts.readLine();
            file.close();
            // METADATA starts with "Name: <name>" then "Version: <version>".
            if (!first.startsWith(QLatin1String("Name: ")))
                continue;
            const QString name = first.mid(6).trimmed();
            const QString versionLine = ts.readLine();
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
    for (const QString& r : trustedRoots) {
        if (r.isEmpty())
            continue;
        const QString canonicalTrusted = QFileInfo(r).canonicalFilePath();
        if (!canonicalTrusted.isEmpty()
            && canonicalRoot.startsWith(canonicalTrusted))
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
    }
    return result;
}

ProviderResult<QString> BuiltinPipProvider::fingerprint(const Environment& env)
{
    ProviderResult<QString> result;
    const QString fp = QString::number(static_cast<qulonglong>(env.root().length()));
    result.value = fp;
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
    QString pipPath;
    if (!env.toolId().isEmpty() && env.toolId().startsWith(QLatin1String("builtin.pip:venv:"))) {
        // venv pip executable, platform dependent.
        const QString candidate = QDir(env.root()).filePath(QLatin1String("Scripts/pip.exe"));
        pipPath = QFileInfo::exists(candidate) ? candidate : pipPath;
        if (pipPath.isEmpty()) {
            const QString candidatePosix = QDir(env.root()).filePath(QLatin1String("bin/pip"));
            if (QFileInfo::exists(candidatePosix))
                pipPath = candidatePosix;
        }
    } else {
        // System pip: locate the tool by its path that was stored in the environment.
        // Fallback to searching PATH.
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