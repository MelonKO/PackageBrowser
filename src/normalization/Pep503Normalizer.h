#pragma once

#include <QString>

// Name normalization rules for the Python ecosystem (PEP 503): lowercase;
// consecutive runs of '-', '_' and '.' collapse into a single '-'.
// See docs/Доменная модель.md ("Identity and name normalization").
class Pep503Normalizer {
public:
    static QString normalize(const QString& raw);
};