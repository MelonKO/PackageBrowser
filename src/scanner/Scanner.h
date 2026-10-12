#pragma once

#include "domain/Environment.h"
#include "domain/ToolInstance.h"
#include "provider/IProvider.h"
#include "trust/TrustPolicy.h"

#include <QStringList>
#include <QVector>
// Discovers environments: finds tools (detect), derives their global
// environments (locate), and walks user-specified roots to find project-local
// environments (locateAt). De-duplicates by canonical path. The scanner owns
// the trust decision: a project environment whose root is trusted becomes Ok,
// otherwise it stays Untrusted and its tool is never executed.
// See docs/Обнаружение окружений.md.
class Scanner {
public:
    // The scanner drives a set of providers and aggregates their environments.
    explicit Scanner(const QVector<IProvider*>& providers);

    // Trust roots are applied to every project environment the scan finds.
    void setTrustPolicy(const TrustPolicy& policy);
    const TrustPolicy& trustPolicy() const { return m_trust; }

    // Detects tools on PATH and their global environments (all providers).
    void scanGlobals(QVector<Environment>& envs, QVector<ToolInstance>& tools);

    // Walks the given roots (BFS, depth-limited) collecting project environments.
    void scanRoots(const QStringList& roots,
                   int maxDepth,
                   const QStringList& ignoreRoots,
                   int maxEnvironments,
                   QVector<Environment>& envs);

private:
    QVector<IProvider*> m_providers;
    TrustPolicy m_trust;
};