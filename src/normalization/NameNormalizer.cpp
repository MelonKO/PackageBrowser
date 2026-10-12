#include "normalization/NameNormalizer.h"

#include "normalization/Pep503Normalizer.h"

QString normalizeName(Ecosystem ecosystem, const QString& raw)
{
    switch (ecosystem) {
    case Ecosystem::Python:
        return Pep503Normalizer::normalize(raw);
    case Ecosystem::JavaScript:
        // npm requires lowercase names and a scoped name keeps its "@scope/"
        // prefix, so only case folding and trimming are safe here.
        return raw.trimmed().toLower();
    case Ecosystem::Cpp:
        // vcpkg package names are lowercase.
        return raw.trimmed().toLower();
    }
    return raw.trimmed();
}
