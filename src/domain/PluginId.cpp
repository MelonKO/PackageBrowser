#include "domain/PluginId.h"

#include <QHashFunctions>

#include <QByteArray>

uint qHash(const PluginId& p, uint seed)
{
    const QByteArray data = p.value().toUtf8();
    return qHashBits(data.constData(), data.size(), seed);
}