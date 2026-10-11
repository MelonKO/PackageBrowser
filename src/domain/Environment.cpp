#include "domain/Environment.h"

QString envStatusToString(EnvStatus s)
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