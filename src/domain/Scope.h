#pragma once

// Уровень видимости окружения (см. "Доменная модель"): System / User / Project.
enum class Scope {
    System,
    User,
    Project
};