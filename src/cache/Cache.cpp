#include "cache/Cache.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

constexpr int kSchemaVersion = 1;

QString scopeToDb(Scope s)
{
    switch (s) {
    case Scope::System: return QStringLiteral("system");
    case Scope::User: return QStringLiteral("user");
    case Scope::Project: return QStringLiteral("project");
    }
    return QString();
}

Scope scopeFromDb(const QString& s)
{
    if (s == QLatin1String("user")) return Scope::User;
    if (s == QLatin1String("project")) return Scope::Project;
    return Scope::System;
}

QString statusToDb(EnvStatus s)
{
    switch (s) {
    case EnvStatus::Ok: return QStringLiteral("ok");
    case EnvStatus::Stale: return QStringLiteral("stale");
    case EnvStatus::Error: return QStringLiteral("error");
    case EnvStatus::Missing: return QStringLiteral("missing");
    case EnvStatus::Untrusted: return QStringLiteral("untrusted");
    }
    return QString();
}

EnvStatus statusFromDb(const QString& s)
{
    if (s == QLatin1String("stale")) return EnvStatus::Stale;
    if (s == QLatin1String("error")) return EnvStatus::Error;
    if (s == QLatin1String("missing")) return EnvStatus::Missing;
    if (s == QLatin1String("untrusted")) return EnvStatus::Untrusted;
    return EnvStatus::Ok;
}

ManagerKind kindFromDb(const QString& pluginId, const QString& ecosystem)
{
    Ecosystem e = Ecosystem::Python;
    if (ecosystem == QLatin1String("javascript")) e = Ecosystem::JavaScript;
    else if (ecosystem == QLatin1String("cpp")) e = Ecosystem::Cpp;
    return ManagerKind(PluginId(pluginId), e);
}

QString ecosystemToDb(Ecosystem e)
{
    switch (e) {
    case Ecosystem::Python: return QStringLiteral("python");
    case Ecosystem::JavaScript: return QStringLiteral("javascript");
    case Ecosystem::Cpp: return QStringLiteral("cpp");
    }
    return QString();
}

} // namespace

class Cache::Impl {
public:
    QString uniqueConnectionName;
    QSqlDatabase db;
};

Cache::Cache(const QString& dbPath)
    : m_dbPath(dbPath)
{
    d = new Impl;
}

Cache::~Cache()
{
    if (d) {
        const QString name = d->uniqueConnectionName;
        if (d->db.isValid())
            d->db.close();
        if (!name.isEmpty())
            QSqlDatabase::removeDatabase(name);
        delete d;
    }
}

bool Cache::open()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        return false;

    static int counter = 0;
    d->uniqueConnectionName = QStringLiteral("pb_cache_%1_%2")
                                  .arg(QCoreApplication::applicationPid())
                                  .arg(++counter);
    d->db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), d->uniqueConnectionName);
    d->db.setDatabaseName(m_dbPath);
    if (!d->db.open())
        return false;

    ensureSchema();
    return true;
}

int Cache::schemaVersion() const
{
    QSqlQuery q(d->db);
    q.exec(QStringLiteral("PRAGMA user_version"));
    if (q.next())
        return q.value(0).toInt();
    return -1;
}

void Cache::ensureSchema()
{
    const auto exec = [this](const QString& sql) {
        QSqlQuery q(d->db);
        q.exec(sql);
    };

    // If a stale schema version is present, drop the disposable tables and
    // recreate from scratch. The disk is the source of truth, so no data is lost.
    const int v = schemaVersion();
    if (v != kSchemaVersion) {
        exec(QStringLiteral("DROP TABLE IF EXISTS installation"));
        exec(QStringLiteral("DROP TABLE IF EXISTS dependency"));
        exec(QStringLiteral("DROP TABLE IF EXISTS environment"));
        exec(QStringLiteral("DROP TABLE IF EXISTS tool_instance"));
    }

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tool_instance ("
        "  id TEXT PRIMARY KEY,"
        "  plugin_id TEXT NOT NULL,"
        "  ecosystem TEXT NOT NULL,"
        "  real_path TEXT NOT NULL,"
        "  version TEXT,"
        "  provided_by_env TEXT"
        ")"));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS environment ("
        "  id TEXT PRIMARY KEY,"
        "  plugin_id TEXT NOT NULL,"
        "  ecosystem TEXT NOT NULL,"
        "  scope TEXT NOT NULL CHECK (scope IN ('system','user','project')),"
        "  root TEXT NOT NULL,"
        "  tool_id TEXT REFERENCES tool_instance(id),"
        "  inherits_from TEXT REFERENCES environment(id),"
        "  fingerprint TEXT,"
        "  scanned_at INTEGER,"
        "  status TEXT NOT NULL"
        ")"));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS installation ("
        "  env_id TEXT NOT NULL REFERENCES environment(id) ON DELETE CASCADE,"
        "  name_norm TEXT NOT NULL,"
        "  name_display TEXT NOT NULL,"
        "  version TEXT,"
        "  location TEXT,"
        "  is_direct INTEGER,"
        "  PRIMARY KEY (env_id, name_norm)"
        ")"));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS dependency ("
        "  env_id TEXT NOT NULL, from_name TEXT NOT NULL,"
        "  to_name TEXT NOT NULL, constraint_text TEXT,"
        "  PRIMARY KEY (env_id, from_name, to_name)"
        ")"));

    QSqlQuery(d->db).exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
}

void Cache::upsertTool(const ToolInstance& tool)
{
    QSqlQuery q(d->db);
    q.prepare(QStringLiteral(
        "INSERT INTO tool_instance (id, plugin_id, ecosystem, real_path, version, provided_by_env) "
        "VALUES (:id, :plugin_id, :ecosystem, :path, :version, :provided_by_env) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  real_path=excluded.real_path, version=excluded.version,"
        "  plugin_id=excluded.plugin_id, ecosystem=excluded.ecosystem,"
        "  provided_by_env=excluded.provided_by_env"));
    q.bindValue(QStringLiteral(":id"), tool.id());
    q.bindValue(QStringLiteral(":plugin_id"), tool.kind().pluginId().value());
    q.bindValue(QStringLiteral(":ecosystem"), ecosystemToDb(tool.kind().ecosystem()));
    q.bindValue(QStringLiteral(":path"), tool.path());
    q.bindValue(QStringLiteral(":version"), tool.version().isEmpty() ? QVariant(QVariant::String) : QVariant(tool.version()));
    q.bindValue(QStringLiteral(":provided_by_env"), tool.providedByEnv().isEmpty() ? QVariant(QVariant::String) : QVariant(tool.providedByEnv()));
    q.exec();
}

void Cache::removeTool(const QString& toolId)
{
    QSqlQuery q(d->db);
    q.prepare(QStringLiteral("DELETE FROM tool_instance WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), toolId);
    q.exec();
}

void Cache::upsertEnvironment(const Environment& env)
{
    QSqlQuery q(d->db);
    q.prepare(QStringLiteral(
        "INSERT INTO environment (id, plugin_id, ecosystem, scope, root, tool_id, inherits_from, status) "
        "VALUES (:id, :plugin_id, :ecosystem, :scope, :root, :tool_id, :inherits_from, :status) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  root=excluded.root, scope=excluded.scope, tool_id=excluded.tool_id,"
        "  inherits_from=excluded.inherits_from, status=excluded.status,"
        "  plugin_id=excluded.plugin_id, ecosystem=excluded.ecosystem"));
    q.bindValue(QStringLiteral(":id"), env.id().value());
    q.bindValue(QStringLiteral(":plugin_id"), env.kind().pluginId().value());
    q.bindValue(QStringLiteral(":ecosystem"), ecosystemToDb(env.kind().ecosystem()));
    q.bindValue(QStringLiteral(":scope"), scopeToDb(env.scope()));
    q.bindValue(QStringLiteral(":root"), env.root());
    q.bindValue(QStringLiteral(":tool_id"), env.toolId().isEmpty() ? QVariant(QVariant::String) : QVariant(env.toolId()));
    q.bindValue(QStringLiteral(":inherits_from"), env.inheritsFrom().isEmpty() ? QVariant(QVariant::String) : QVariant(env.inheritsFrom()));
    q.bindValue(QStringLiteral(":status"), statusToDb(env.status()));
    q.exec();
}

void Cache::markEnvironmentMissing(const EnvironmentId& id)
{
    updateEnvironmentStatus(id, EnvStatus::Missing);
}

void Cache::updateEnvironmentStatus(const EnvironmentId& id, EnvStatus status)
{
    QSqlQuery q(d->db);
    q.prepare(QStringLiteral("UPDATE environment SET status = :status WHERE id = :id"));
    q.bindValue(QStringLiteral(":status"), statusToDb(status));
    q.bindValue(QStringLiteral(":id"), id.value());
    q.exec();
}

void Cache::replaceInstallations(const EnvironmentId& envId, const QVector<Installation>& packages)
{
    if (!d->db.transaction())
        return;

    bool ok = true;
    QSqlQuery del(d->db);
    del.prepare(QStringLiteral("DELETE FROM installation WHERE env_id = :env_id"));
    del.bindValue(QStringLiteral(":env_id"), envId.value());
    ok = ok && del.exec();

    if (ok) {
        QSqlQuery ins(d->db);
        ins.prepare(QStringLiteral(
            "INSERT INTO installation (env_id, name_norm, name_display, version, location, is_direct) "
            "VALUES (:env_id, :name_norm, :name_display, :version, :location, :is_direct)"));
        for (const Installation& p : packages) {
            ins.bindValue(QStringLiteral(":env_id"), envId.value());
            ins.bindValue(QStringLiteral(":name_norm"), p.nameNorm());
            ins.bindValue(QStringLiteral(":name_display"), p.nameDisplay());
            ins.bindValue(QStringLiteral(":version"), p.version().isEmpty() ? QVariant(QVariant::String) : QVariant(p.version()));
            ins.bindValue(QStringLiteral(":location"), p.location().isEmpty() ? QVariant(QVariant::String) : QVariant(p.location()));
            if (p.isDirect().has_value())
                ins.bindValue(QStringLiteral(":is_direct"), p.isDirect().value() ? 1 : 0);
            else
                ins.bindValue(QStringLiteral(":is_direct"), QVariant(QVariant::Int));
            if (!ins.exec()) {
                ok = false;
                break;
            }
        }
    }

    if (ok)
        d->db.commit();
    else
        d->db.rollback();
}

QVector<Installation> Cache::installations(const EnvironmentId& envId) const
{
    QVector<Installation> out;
    QSqlQuery q(d->db);
    q.prepare(QStringLiteral(
        "SELECT env_id, name_norm, name_display, version, location, is_direct "
        "FROM installation WHERE env_id = :env_id ORDER BY name_norm"));
    q.bindValue(QStringLiteral(":env_id"), envId.value());
    if (!q.exec()) return out;
    while (q.next()) {
        std::optional<bool> direct;
        if (!q.value(5).isNull())
            direct = q.value(5).toInt() != 0;
        const QString envIdValue = q.value(0).toString();
        out.append(Installation(EnvironmentId::fromValue(envIdValue), q.value(1).toString(),
                                q.value(2).toString(), q.value(3).toString(),
                                q.value(4).toString(), direct));
    }
    return out;
}

QVector<Installation> Cache::allInstallations() const
{
    QVector<Installation> out;
    QSqlQuery q(d->db);
    q.exec(QStringLiteral(
        "SELECT env_id, name_norm, name_display, version, location, is_direct "
        "FROM installation ORDER BY name_norm"));
    while (q.next()) {
        std::optional<bool> direct;
        if (!q.value(5).isNull())
            direct = q.value(5).toInt() != 0;
        out.append(Installation(EnvironmentId::fromValue(q.value(0).toString()),
                                q.value(1).toString(), q.value(2).toString(),
                                q.value(3).toString(), q.value(4).toString(), direct));
    }
    return out;
}

QVector<ToolInstance> Cache::tools() const
{
    QVector<ToolInstance> out;
    QSqlQuery q(d->db);
    q.exec(QStringLiteral(
        "SELECT id, plugin_id, ecosystem, real_path, version, provided_by_env "
        "FROM tool_instance ORDER BY id"));
    while (q.next()) {
        const QString pluginId = q.value(1).toString();
        const QString id = q.value(0).toString();
        const ManagerKind kind = kindFromDb(pluginId, q.value(2).toString());
        out.append(ToolInstance(kind, id, q.value(3).toString(), q.value(4).toString(),
                                q.value(5).toString()));
    }
    return out;
}

QVector<Environment> Cache::environments() const
{
    QVector<Environment> out;
    QSqlQuery q(d->db);
    q.exec(QStringLiteral(
        "SELECT id, plugin_id, ecosystem, scope, root, tool_id, inherits_from, status "
        "FROM environment ORDER BY root"));
    while (q.next()) {
        const QString id = q.value(0).toString();
        const QString root = q.value(4).toString();
        const ManagerKind kind = kindFromDb(q.value(1).toString(), q.value(2).toString());
        out.append(Environment(EnvironmentId::fromValue(id), kind, scopeFromDb(q.value(3).toString()),
                               root, q.value(5).toString(), q.value(6).toString(),
                               statusFromDb(q.value(7).toString())));
    }
    return out;
}