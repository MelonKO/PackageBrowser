#include "provider/DeclarativeProvider.h"

#include "normalization/NameNormalizer.h"
#include "process/ProcessRunner.h"
#include "provider/JsonListParser.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace {

QString toolIdFor(const PluginId& pluginId, const QString& path)
{
    return QString::fromUtf8(QCryptographicHash::hash(
                                 (pluginId.value() + QLatin1Char('\0') + path).toUtf8(),
                                 QCryptographicHash::Sha1)
                                 .toHex()
                                 .left(12));
}

// Substitute the whitelisted placeholders. The manifest layer already rejected
// anything outside the whitelist, so this cannot inject arbitrary text.
// {exe} and {tool} resolve to the same value: the tool is whichever executable
// was resolved for this provider (from PATH or from the environment root).
QStringList substitute(const QStringList& tmpl, const QString& tool, const QString& root)
{
    QStringList out;
    out.reserve(tmpl.size());
    for (const QString& arg : tmpl) {
        QString s = arg;
        s.replace(QLatin1String("{exe}"), tool);
        s.replace(QLatin1String("{tool}"), tool);
        s.replace(QLatin1String("{root}"), root);
        out.append(s);
    }
    return out;
}

QString extractVersion(const QString& text, const QString& pattern)
{
    if (pattern.isEmpty())
        return QString();
    const QRegularExpression re(pattern);
    if (!re.isValid())
        return QString();
    const auto match = re.match(text);
    if (!match.hasMatch())
        return QString();
    const QString named = match.captured(QStringLiteral("version"));
    if (!named.isEmpty())
        return named.trimmed();
    if (match.lastCapturedIndex() >= 1)
        return match.captured(1).trimmed();
    return match.captured(0).trimmed();
}

} // namespace

DeclarativeProvider::DeclarativeProvider(PluginManifest manifest)
    : m_manifest(std::move(manifest))
{
}

const PluginId& DeclarativeProvider::pluginId() const { return m_manifest.pluginId(); }

const ManagerKind& DeclarativeProvider::kind() const { return m_manifest.kind(); }

bool DeclarativeProvider::hasCapability(Capability c) const
{
    if (!m_manifest.hasCapability(c))
        return false;
    switch (c) {
    case Capability::Detect:
        return !m_manifest.tool().executables.isEmpty();
    case Capability::Locate:
    case Capability::List:
        return !m_manifest.locators().isEmpty();
    case Capability::Fingerprint:
        // A cheap fingerprint exists only where packages are read from files;
        // for a command locator it would mean running the manager.
        for (const LocatorSpec& locator : m_manifest.locators())
            if (locator.listFiles.has_value())
                return true;
        return false;
    }
    return false;
}

QStringList DeclarativeProvider::projectMarkers() const
{
    QStringList out;
    for (const LocatorSpec& locator : m_manifest.locators()) {
        if (locator.scope == Scope::Project && !locator.markerFile.isEmpty()
            && !out.contains(locator.markerFile))
            out.append(locator.markerFile);
    }
    return out;
}

QStringList DeclarativeProvider::skipDirs() const
{
    QStringList out;
    for (const LocatorSpec& locator : m_manifest.locators()) {
        if (!locator.listFiles.has_value())
            continue;
        const QString first =
            locator.listFiles->dir.split(QLatin1Char('/'), Qt::SkipEmptyParts).value(0);
        if (!first.isEmpty() && !out.contains(first))
            out.append(first);
    }
    return out;
}

ProviderResult<QVector<ToolInstance>> DeclarativeProvider::detect()
{
    ProviderResult<QVector<ToolInstance>> result;
    QSet<QString> seen;
    for (const QString& exeName : m_manifest.tool().executables) {
        const QString path = pb::findOnPath(exeName);
        if (path.isEmpty() || seen.contains(path))
            continue;
        seen.insert(path);

        QString version;
        if (!m_manifest.tool().versionCmd.isEmpty()) {
            const QStringList command = substitute(m_manifest.tool().versionCmd, path, QString());
            if (!command.isEmpty()) {
                const pb::ProcessResult run = pb::runProcess(command.first(), command.mid(1));
                if (run.finished && run.exitCode == 0)
                    version = extractVersion(QString::fromUtf8(run.stdOut),
                                             m_manifest.tool().versionRegex);
            }
        }
        result.value.append(
            ToolInstance(kind(), toolIdFor(m_manifest.pluginId(), path), path, version));
    }

    if (result.value.isEmpty()) {
        result.error = ProviderError::ToolNotFound;
        return result;
    }
    return result;
}

ProviderResult<QVector<Environment>> DeclarativeProvider::locate(const ToolInstance& tool)
{
    ProviderResult<QVector<Environment>> result;
    if (tool.path().isEmpty()) {
        result.error = ProviderError::EnvBroken;
        return result;
    }

    // Environment identity is hash(plugin, canonical root), so one root yields
    // one environment: only the first global locator is used.
    for (int i = 0; i < m_manifest.locators().size(); ++i) {
        const LocatorSpec& locator = m_manifest.locators().at(i);
        if (!locator.fromPath)
            continue;

        const QString root = QFileInfo(tool.path()).dir().path();
        const auto envId = EnvironmentId::fromHashed(m_manifest.pluginId().value(), root);
        m_envToolPaths.insert(envId.value(), tool.path());
        m_envLocators.insert(envId.value(), i);
        result.value.append(Environment(envId, kind(), locator.scope, root, tool.id()));
        break;
    }
    return result;
}

ProviderResult<QVector<Environment>> DeclarativeProvider::locateAt(const QString& scanRoot)
{
    ProviderResult<QVector<Environment>> result;
    const QDir dir(scanRoot);
    if (!dir.exists()) {
        result.error = ProviderError::EnvBroken;
        return result;
    }

    for (int i = 0; i < m_manifest.locators().size(); ++i) {
        const LocatorSpec& locator = m_manifest.locators().at(i);
        if (locator.scope != Scope::Project || locator.markerFile.isEmpty())
            continue;
        if (!dir.exists(locator.markerFile))
            continue;
        if (!locator.requireDir.isEmpty() && !dir.exists(locator.requireDir))
            continue;

        const QString root = dir.canonicalPath();
        const auto envId = EnvironmentId::fromHashed(m_manifest.pluginId().value(), root);
        QString toolId = QStringLiteral("%1:project:%2")
                             .arg(m_manifest.pluginId().value(), envId.value());
        for (const QString& relative : locator.toolsInRoot) {
            const QString toolPath = dir.filePath(relative);
            if (!QFileInfo::exists(toolPath))
                continue;
            const QString canonicalTool = QFileInfo(toolPath).canonicalFilePath();
            m_envToolPaths.insert(envId.value(), canonicalTool);
            toolId = toolIdFor(m_manifest.pluginId(), canonicalTool);
            break;
        }
        m_envLocators.insert(envId.value(), i);
        // Safe default: untrusted until the scanner's trust policy upgrades it.
        result.value.append(Environment(envId, kind(), Scope::Project, root, toolId,
                                        QString(), EnvStatus::Untrusted));
    }
    return result;
}

ProviderResult<QString> DeclarativeProvider::fingerprint(const Environment& env)
{
    ProviderResult<QString> result;
    const LocatorSpec* locator = locatorFor(env);
    if (!locator || !locator->listFiles.has_value()) {
        // No cheap fingerprint without running the manager: refuse honestly, so
        // the caller re-lists instead of trusting a wrong hash.
        result.error = ProviderError::Unsupported;
        return result;
    }

    const QDir dir(QDir(env.root()).filePath(locator->listFiles->dir));
    QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    names.sort();
    result.value = QString::fromUtf8(
        QCryptographicHash::hash(names.join(QLatin1Char('\n')).toUtf8(), QCryptographicHash::Sha1)
            .toHex());
    return result;
}

ProviderResult<QVector<Installation>> DeclarativeProvider::list(const Environment& env,
                                                                bool withDependencies)
{
    (void)withDependencies; // the MVP list is flat
    ProviderResult<QVector<Installation>> result;
    const LocatorSpec* locator = locatorFor(env);
    if (!locator) {
        result.error = ProviderError::EnvBroken;
        return result;
    }
    if (locator->listFiles.has_value())
        return listFromFiles(env, *locator);
    return listFromCommand(env, *locator);
}

const LocatorSpec* DeclarativeProvider::locatorFor(const Environment& env) const
{
    if (m_envLocators.contains(env.id().value())) {
        const int index = m_envLocators.value(env.id().value());
        if (index >= 0 && index < m_manifest.locators().size())
            return &m_manifest.locators().at(index);
    }

    // Fallback for an environment restored from the cache: match on scope, and
    // for a project environment also on its marker file being present. For a
    // global (system/user) environment only from-path locators are candidates,
    // because locate() derives global environments exclusively from a tool on
    // PATH; a non-path locator of the same scope would be a wrong guess.
    for (const LocatorSpec& locator : m_manifest.locators()) {
        if (locator.scope != env.scope())
            continue;
        if (env.scope() == Scope::Project) {
            if (!locator.markerFile.isEmpty()
                && QFileInfo::exists(QDir(env.root()).filePath(locator.markerFile)))
                return &locator;
        } else if (locator.fromPath) {
            return &locator;
        }
    }
    return nullptr;
}

ProviderResult<QVector<Installation>> DeclarativeProvider::listFromFiles(const Environment& env,
                                                                        const LocatorSpec& locator)
{
    ProviderResult<QVector<Installation>> result;
    const ListFilesSpec& spec = *locator.listFiles;
    const Ecosystem ecosystem = m_manifest.kind().ecosystem();

    const QDir dir(QDir(env.root()).filePath(spec.dir));
    if (!dir.exists()) {
        result.error = ProviderError::EnvBroken;
        return result;
    }

    const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        const QString manifestPath = dir.filePath(entry + QLatin1Char('/') + spec.manifest);
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();
        file.close();

        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (!doc.isObject())
            continue;
        const QJsonObject obj = doc.object();
        const QString name = obj.value(spec.nameKey).toString();
        if (name.isEmpty())
            continue;
        result.value.append(Installation(env.id(), normalizeName(ecosystem, name), name,
                                         obj.value(spec.versionKey).toString(), manifestPath));
    }
    return result;
}

ProviderResult<QVector<Installation>> DeclarativeProvider::listFromCommand(const Environment& env,
                                                                          const LocatorSpec& locator)
{
    ProviderResult<QVector<Installation>> result;
    const ListJsonSpec& spec = *locator.listJson;

    if (env.scope() == Scope::Project && env.status() == EnvStatus::Untrusted) {
        // Running a manager inside an untrusted project executes foreign code,
        // and this locator declares no file-based fallback.
        result.error = ProviderError::Untrusted;
        return result;
    }

    QString tool = m_envToolPaths.value(env.id().value());
    if (tool.isEmpty() && !m_manifest.tool().executables.isEmpty())
        tool = pb::findOnPath(m_manifest.tool().executables.first());
    if (tool.isEmpty()) {
        result.error = ProviderError::ToolNotFound;
        return result;
    }

    const QStringList command = substitute(spec.command, tool, env.root());
    if (command.isEmpty()) {
        result.error = ProviderError::Unsupported;
        return result;
    }

    const QString workingDir = env.scope() == Scope::Project ? env.root() : QString();
    const pb::ProcessResult run = pb::runProcess(command.first(), command.mid(1), workingDir);
    if (!run.started) {
        result.error = ProviderError::ToolNotFound;
        return result;
    }
    if (!run.finished) {
        result.error = ProviderError::Timeout;
        result.complete = false;
        return result;
    }
    if (!spec.okExitCodes.contains(run.exitCode)) {
        result.error = ProviderError::EnvBroken;
        result.complete = false;
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(run.stdOut);
    if (doc.isNull()) {
        result.error = ProviderError::ParseFailed;
        result.complete = false;
        return result;
    }

    const QJsonValue document = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    const QJsonValue container = pb::jsonAtPath(document, spec.arrayPath);
    if (!pb::extractInstallations(container, spec.nameKey, spec.versionKey,
                                  m_manifest.kind().ecosystem(), env.id(), result.value)) {
        result.error = ProviderError::ParseFailed;
        result.complete = false;
        result.value.clear();
        return result;
    }
    return result;
}
