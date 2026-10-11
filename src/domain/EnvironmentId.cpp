#include "domain/EnvironmentId.h"

#include <QHashFunctions>

#include <QCryptographicHash>

#include <QByteArray>

// Stable hashing independent of qHash(QString) overloads.
uint qHash(const EnvironmentId& id, uint seed)
{
    const QByteArray data = id.value().toUtf8();
    return qHashBits(data.constData(), data.size(), seed);
}

QString EnvironmentId::shortHash(const QString& pluginId, const QString& canonicalRoot)
{
    const QByteArray data = (pluginId + QLatin1Char('\0') + canonicalRoot).toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(12);
}