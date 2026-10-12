#pragma once

#include "domain/Environment.h"

#include <QString>
#include <QStringList>

// Whether running a tool found inside a project directory is allowed. Roots are
// marked trusted by the user; a project environment outside them is read by
// parsing files only and gets EnvStatus::Untrusted.
// See docs/Надёжность и безопасность.md ("Rules") and docs/Обнаружение окружений.md.
class TrustPolicy {
public:
    TrustPolicy() = default;
    explicit TrustPolicy(const QStringList& trustedRoots);

    void setTrustedRoots(const QStringList& roots);
    const QStringList& trustedRoots() const { return m_roots; }

    // True when `path` is a trusted root or a genuine descendant of one.
    // Comparison uses canonical paths and a directory boundary, so that
    // "work/app" does not trust "work/apple". Unresolvable paths are untrusted.
    bool isTrusted(const QString& path) const;

    // Status a project environment rooted at `root` should carry.
    EnvStatus statusForProject(const QString& root) const;

private:
    QStringList m_roots;
};
