#include "normalization/Pep503Normalizer.h"

QString Pep503Normalizer::normalize(const QString& raw)
{
    QString out;
    out.reserve(raw.size());
    bool prevSep = false;

    for (const QChar c : raw) {
        if (c == QLatin1Char('-') || c == QLatin1Char('_') || c == QLatin1Char('.')) {
            if (!prevSep) {
                out.append(QLatin1Char('-'));
                prevSep = true;
            }
            continue;
        }
        out.append(c.toLower());
        prevSep = false;
    }
    return out;
}