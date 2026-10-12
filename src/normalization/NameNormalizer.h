#pragma once

#include "domain/ManagerKind.h"

#include <QString>

// Name normalization rules belong to the ecosystem, not to the core, because
// the same spelling can mean different packages in different managers.
// See docs/Доменная модель.md ("Identity and name normalization").
QString normalizeName(Ecosystem ecosystem, const QString& raw);
