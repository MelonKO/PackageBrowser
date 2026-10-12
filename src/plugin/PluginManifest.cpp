#include "plugin/PluginManifest.h"

#include <toml++/toml.hpp>

#include <QRegularExpression>

#include <string>

namespace {

QString toQ(const std::string& s) { return QString::fromUtf8(s.c_str(), int(s.size())); }

QString strValue(const toml::table& t, std::string_view key)
{
    if (auto v = t[key].value<std::string>())
        return toQ(*v);
    return QString();
}

QStringList strArray(const toml::table& t, std::string_view key)
{
    QStringList out;
    if (const toml::array* a = t[key].as_array()) {
        for (const toml::node& n : *a) {
            if (auto s = n.value<std::string>())
                out << toQ(*s);
        }
    }
    return out;
}

QList<int> intArray(const toml::table& t, std::string_view key, const QList<int>& fallback)
{
    if (const toml::array* a = t[key].as_array()) {
        QList<int> out;
        for (const toml::node& n : *a) {
            if (auto i = n.value<int64_t>())
                out << int(*i);
        }
        if (!out.isEmpty())
            return out;
    }
    return fallback;
}

bool hasKey(const toml::table& t, std::string_view key) { return t.contains(key); }

bool scopeFromString(const QString& s, Scope* out)
{
    if (s == QLatin1String("system")) { *out = Scope::System; return true; }
    if (s == QLatin1String("user")) { *out = Scope::User; return true; }
    if (s == QLatin1String("project")) { *out = Scope::Project; return true; }
    return false;
}

QString scopeName(Scope s)
{
    switch (s) {
    case Scope::System: return QStringLiteral("system");
    case Scope::User: return QStringLiteral("user");
    case Scope::Project: return QStringLiteral("project");
    }
    return QString();
}

bool ecosystemFromString(const QString& s, Ecosystem* out)
{
    if (s == QLatin1String("python")) { *out = Ecosystem::Python; return true; }
    if (s == QLatin1String("javascript")) { *out = Ecosystem::JavaScript; return true; }
    if (s == QLatin1String("cpp")) { *out = Ecosystem::Cpp; return true; }
    return false;
}

QString capabilityName(Capability c)
{
    switch (c) {
    case Capability::Detect: return QStringLiteral("detect");
    case Capability::Locate: return QStringLiteral("locate");
    case Capability::Fingerprint: return QStringLiteral("fingerprint");
    case Capability::List: return QStringLiteral("list");
    }
    return QString();
}

bool capabilityFromString(const QString& s, Capability* out)
{
    if (s == QLatin1String("detect")) { *out = Capability::Detect; return true; }
    if (s == QLatin1String("locate")) { *out = Capability::Locate; return true; }
    if (s == QLatin1String("fingerprint")) { *out = Capability::Fingerprint; return true; }
    if (s == QLatin1String("list")) { *out = Capability::List; return true; }
    return false;
}

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

// Every {placeholder} inside a command argument must be on the whitelist, so a
// manifest cannot smuggle arbitrary substitution into an executed command.
bool checkTemplate(const QStringList& args, const QString& where, QString* error)
{
    static const QRegularExpression re(QStringLiteral("\\{([A-Za-z_][A-Za-z0-9_]*)\\}"));
    const QStringList allowed = manifestAllowedPlaceholders();
    for (const QString& arg : args) {
        auto matches = re.globalMatch(arg);
        while (matches.hasNext()) {
            const QString placeholder = QStringLiteral("{%1}").arg(matches.next().captured(1));
            if (!allowed.contains(placeholder)) {
                setError(error, QStringLiteral("%1: unknown placeholder %2 in command (allowed: %3)")
                                    .arg(where, placeholder, allowed.join(QStringLiteral(", "))));
                return false;
            }
        }
    }
    return true;
}

std::optional<ListJsonSpec> parseListJson(const toml::table& t, const QString& where, QString* error)
{
    ListJsonSpec spec;
    spec.command = strArray(t, "command");
    if (spec.command.isEmpty()) {
        setError(error, QStringLiteral("%1: list_json requires a non-empty command").arg(where));
        return std::nullopt;
    }
    if (!checkTemplate(spec.command, where, error))
        return std::nullopt;

    const QString arrayPath = strValue(t, "array");
    if (hasKey(t, "array") && arrayPath.isEmpty()) {
        setError(error, QStringLiteral("%1: list_json array must be a non-empty string").arg(where));
        return std::nullopt;
    }
    spec.arrayPath = arrayPath;

    const QString nameKey = strValue(t, "name");
    if (hasKey(t, "name"))
        spec.nameKey = nameKey;
    const QString versionKey = strValue(t, "version");
    if (hasKey(t, "version"))
        spec.versionKey = versionKey;
    if (spec.nameKey.isEmpty()) {
        setError(error, QStringLiteral("%1: list_json name key must not be empty").arg(where));
        return std::nullopt;
    }

    spec.okExitCodes = intArray(t, "ok_exit_codes", QList<int>{ 0 });
    return spec;
}

std::optional<ListFilesSpec> parseListFiles(const toml::table& t, const QString& where, QString* error)
{
    ListFilesSpec spec;
    spec.dir = strValue(t, "dir");
    if (spec.dir.isEmpty()) {
        setError(error, QStringLiteral("%1: list_files requires a non-empty dir").arg(where));
        return std::nullopt;
    }

    const QString manifest = strValue(t, "manifest");
    if (hasKey(t, "manifest")) {
        if (manifest.isEmpty()) {
            setError(error, QStringLiteral("%1: list_files manifest must not be empty").arg(where));
            return std::nullopt;
        }
        spec.manifest = manifest;
    }

    const QString nameKey = strValue(t, "name");
    if (hasKey(t, "name"))
        spec.nameKey = nameKey;
    const QString versionKey = strValue(t, "version");
    if (hasKey(t, "version"))
        spec.versionKey = versionKey;
    if (spec.nameKey.isEmpty()) {
        setError(error, QStringLiteral("%1: list_files name key must not be empty").arg(where));
        return std::nullopt;
    }

    return spec;
}

std::optional<LocatorSpec> parseLocator(const toml::table& t, int index, QString* error)
{
    const QString where = QStringLiteral("locator[%1]").arg(index);

    LocatorSpec spec;
    const QString scopeStr = strValue(t, "scope");
    if (scopeStr.isEmpty()) {
        setError(error, QStringLiteral("%1: scope is required").arg(where));
        return std::nullopt;
    }
    if (!scopeFromString(scopeStr, &spec.scope)) {
        setError(error, QStringLiteral("%1: unknown scope \"%2\"").arg(where, scopeStr));
        return std::nullopt;
    }

    if (spec.scope == Scope::Project) {
        spec.markerFile = strValue(t, "marker_file");
        if (spec.markerFile.isEmpty()) {
            setError(error, QStringLiteral("%1: project scope requires marker_file").arg(where));
            return std::nullopt;
        }
        if (hasKey(t, "from")) {
            setError(error, QStringLiteral("%1: project scope cannot use from").arg(where));
            return std::nullopt;
        }
    } else {
        const QString from = strValue(t, "from");
        if (from != QLatin1String("path")) {
            setError(error, QStringLiteral("%1: scope %2 requires from = \"path\"")
                                .arg(where, scopeName(spec.scope)));
            return std::nullopt;
        }
        spec.fromPath = true;
        if (hasKey(t, "marker_file")) {
            setError(error, QStringLiteral("%1: marker_file is only valid for project scope").arg(where));
            return std::nullopt;
        }
    }

    spec.requireDir = strValue(t, "require_dir");
    spec.toolsInRoot = strArray(t, "tool_in_root");
    if (spec.scope != Scope::Project && !spec.toolsInRoot.isEmpty()) {
        setError(error, QStringLiteral("%1: tool_in_root is only valid for project scope").arg(where));
        return std::nullopt;
    }

    const bool hasJson = hasKey(t, "list_json");
    const bool hasFiles = hasKey(t, "list_files");
    if (hasJson == hasFiles) {
        setError(error, QStringLiteral("%1: declare exactly one of list_json or list_files").arg(where));
        return std::nullopt;
    }
    if (hasJson) {
        const toml::table* sub = t["list_json"].as_table();
        if (!sub) {
            setError(error, QStringLiteral("%1: list_json must be a table").arg(where));
            return std::nullopt;
        }
        auto parsed = parseListJson(*sub, where, error);
        if (!parsed)
            return std::nullopt;
        spec.listJson = *parsed;
    } else {
        const toml::table* sub = t["list_files"].as_table();
        if (!sub) {
            setError(error, QStringLiteral("%1: list_files must be a table").arg(where));
            return std::nullopt;
        }
        auto parsed = parseListFiles(*sub, where, error);
        if (!parsed)
            return std::nullopt;
        spec.listFiles = *parsed;
    }

    return spec;
}

} // namespace

QStringList manifestAllowedPlaceholders()
{
    return { QStringLiteral("{exe}"), QStringLiteral("{tool}"), QStringLiteral("{root}") };
}

std::optional<PluginManifest> PluginManifest::parse(const QString& text,
                                                    const QString& sourcePath,
                                                    QString* error)
{
    toml::table tbl;
    try {
        const QByteArray utf8 = text.toUtf8();
        tbl = toml::parse(std::string_view(utf8.constData(), size_t(utf8.size())),
                          std::string_view(sourcePath.toUtf8().constData()));
    } catch (const toml::parse_error& e) {
        setError(error, QStringLiteral("%1: %2").arg(sourcePath, toQ(std::string(e.description()))));
        return std::nullopt;
    }

    const toml::table* plugin = tbl["plugin"].as_table();
    if (!plugin) {
        setError(error, QStringLiteral("%1: missing [plugin] table").arg(sourcePath));
        return std::nullopt;
    }

    PluginManifest m;
    m.m_sourcePath = sourcePath;

    const QString id = strValue(*plugin, "id");
    if (id.isEmpty() || !id.contains(QLatin1Char('.'))) {
        setError(error, QStringLiteral("%1: plugin.id must be a dotted identifier like \"builtin.pip\"")
                            .arg(sourcePath));
        return std::nullopt;
    }
    m.m_pluginId = PluginId(id);

    const auto api = (*plugin)["api"].value<int64_t>();
    if (!api) {
        setError(error, QStringLiteral("%1: plugin.api is required").arg(sourcePath));
        return std::nullopt;
    }
    if (*api < 1 || *api > kHostApiVersion) {
        setError(error, QStringLiteral("%1: plugin.api %2 is not supported by host api %3")
                            .arg(sourcePath)
                            .arg(*api)
                            .arg(kHostApiVersion));
        return std::nullopt;
    }
    m.m_apiVersion = int(*api);

    m.m_name = strValue(*plugin, "name");

    const QString ecosystemStr = strValue(*plugin, "ecosystem");
    Ecosystem ecosystem = Ecosystem::Python;
    if (ecosystemStr.isEmpty() || !ecosystemFromString(ecosystemStr, &ecosystem)) {
        setError(error, QStringLiteral("%1: unknown ecosystem \"%2\"").arg(sourcePath, ecosystemStr));
        return std::nullopt;
    }
    m.m_kind = ManagerKind(m.m_pluginId, ecosystem);

    m.m_capabilities = strArray(*plugin, "capabilities");
    if (m.m_capabilities.isEmpty()) {
        setError(error, QStringLiteral("%1: plugin.capabilities must not be empty").arg(sourcePath));
        return std::nullopt;
    }
    for (const QString& c : m.m_capabilities) {
        Capability parsed;
        // A typo in a capability name would silently disable an operation, so
        // an unknown name rejects the manifest instead of being ignored.
        if (!capabilityFromString(c, &parsed)) {
            setError(error, QStringLiteral("%1: unknown capability \"%2\"").arg(sourcePath, c));
            return std::nullopt;
        }
    }

    const toml::table* provider = tbl["provider"].as_table();
    const toml::table* tool = tbl["tool"].as_table();
    const bool hasProvider = provider != nullptr;
    const bool hasTool = tool != nullptr;

    if (hasProvider == hasTool) {
        // Exactly one of `[provider]` or `[tool]` must be declared; `[[locator]]`
        // belongs to the tool shape only.
        setError(error, QStringLiteral("%1: declare exactly one of [provider] or [tool]")
                            .arg(sourcePath));
        return std::nullopt;
    }

    if (provider) {
        m.m_isProcessProvider = true;
        const QStringList exec = strArray(*provider, "exec");
        if (exec.isEmpty()) {
            setError(error, QStringLiteral("%1: [provider] exec must be a non-empty command")
                                .arg(sourcePath));
            return std::nullopt;
        }
        m.m_processProvider.program = exec.first();
        m.m_processProvider.args = exec.mid(1);
    }

    if (tool) {
        m.m_tool.executables = strArray(*tool, "executables");
        m.m_tool.versionCmd = strArray(*tool, "version_cmd");
        m.m_tool.versionRegex = strValue(*tool, "version_regex");
        if (!checkTemplate(m.m_tool.versionCmd, QStringLiteral("tool.version_cmd"), error))
            return std::nullopt;
        if (m.hasCapability(Capability::Detect) && m.m_tool.executables.isEmpty()) {
            setError(error,
                     QStringLiteral("%1: capability detect requires tool.executables").arg(sourcePath));
            return std::nullopt;
        }
    }

    if (!m.m_isProcessProvider) {
        const toml::array* locators = tbl["locator"].as_array();
        if (!locators || locators->empty()) {
            setError(error, QStringLiteral("%1: at least one [[locator]] is required").arg(sourcePath));
            return std::nullopt;
        }
        for (size_t i = 0; i < locators->size(); ++i) {
            const toml::table* lt = locators->at(i).as_table();
            if (!lt) {
                setError(error, QStringLiteral("%1: locator[%2] must be a table").arg(sourcePath).arg(i));
                return std::nullopt;
            }
            auto parsed = parseLocator(*lt, int(i), error);
            if (!parsed)
                return std::nullopt;
            m.m_locators.append(*parsed);
        }
    }

    return m;
}

bool PluginManifest::hasCapability(Capability c) const
{
    return m_capabilities.contains(capabilityName(c));
}
