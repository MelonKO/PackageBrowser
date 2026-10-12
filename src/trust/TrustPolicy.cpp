#include "trust/TrustPolicy.h"

#include <QDir>
#include <QFileInfo>

namespace {

// Normalize a path to forward slashes, collapse "." / ".." and keep a trailing
// separator, so prefix comparison is stable across the platform's separator.
// On Windows the filesystem is case-insensitive in practice, so comparison is
// done in lower case; elsewhere (case-sensitive POSIX) the original case is
// preserved because folding it would over-trust a different directory.
QString normalizedPrefixPath(const QString& path)
{
    QString p = QDir::fromNativeSeparators(QDir::cleanPath(path));
#ifdef Q_OS_WIN
    p = p.toLower();
#endif
    if (!p.endsWith(QLatin1Char('/')))
        p += QLatin1Char('/');
    return p;
}

} // namespace

TrustPolicy::TrustPolicy(const QStringList& trustedRoots)
    : m_roots(trustedRoots)
{
}

void TrustPolicy::setTrustedRoots(const QStringList& roots)
{
    m_roots = roots;
}

bool TrustPolicy::isTrusted(const QString& path) const
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty())
        return false;

    const QString key = normalizedPrefixPath(canonical);
    for (const QString& r : m_roots) {
        if (r.isEmpty())
            continue;
        const QString trustedCanonical = QFileInfo(r).canonicalFilePath();
        if (trustedCanonical.isEmpty())
            continue;
        // Same directory or a genuine descendant: the trailing separator makes
        // this a directory boundary, not a plain string prefix.
        const QString trustedKey = normalizedPrefixPath(trustedCanonical);
        if (key == trustedKey || key.startsWith(trustedKey))
            return true;
    }
    return false;
}

EnvStatus TrustPolicy::statusForProject(const QString& root) const
{
    return isTrusted(root) ? EnvStatus::Ok : EnvStatus::Untrusted;
}
