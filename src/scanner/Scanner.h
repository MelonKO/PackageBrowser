#pragma once

#include "domain/Environment.h"
#include "domain/ToolInstance.h"
#include "provider/IProvider.h"

#include <QStringList>
#include <QVector>

// Discovers environments: finds tools (detect), derives their global
// environments (locate), and walks user-specified roots to find project-local
// environments (locateAt). De-duplicates by canonical path.
// See docs/Обнаружение окружений.md.
class Scanner {
public:
    explicit Scanner(IProvider* provider);

    // Detects tools on PATH and their global environments.
    void scanGlobals(QVector<Environment>& envs, QVector<ToolInstance>& tools);

    // Walks the given roots (BFS, depth-limited) collecting project environments.
    void scanRoots(const QStringList& roots,
                   int maxDepth,
                   const QStringList& ignoreRoots,
                   int maxEnvironments,
                   QVector<Environment>& envs);

private:
    IProvider* m_provider;
};