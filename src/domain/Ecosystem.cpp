#include "domain/ManagerKind.h"

QString ecosystemToString(Ecosystem e)
{
    switch (e) {
    case Ecosystem::Python: return QStringLiteral("python");
    case Ecosystem::JavaScript: return QStringLiteral("javascript");
    case Ecosystem::Cpp: return QStringLiteral("cpp");
    }
    return QString();
}