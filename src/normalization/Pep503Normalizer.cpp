#include "normalization/Pep503Normalizer.h"

QString Pep503Normalizer::normalize(const QString& raw)
{
    QString out;
    out.reserve(raw.size());
    bool started = false;   // whether any real character has been emitted
    bool prevSep = false;   // a separator run seen since the last real character

    for (const QChar c : raw) {
        if (c == QLatin1Char('-') || c == QLatin1Char('_') || c == QLatin1Char('.')) {
            // Record a separator run; do not emit it yet (handles leading runs).
            prevSep = true;
            continue;
        }
        // A separator run between two real characters becomes one '-'.
        if (prevSep && started)
            out.append(QLatin1Char('-'));
        prevSep = false;
        started = true;
        out.append(c.toLower());
    }

    // A trailing separator run is never emitted, so no trailing '-' remains.
    return out;
}