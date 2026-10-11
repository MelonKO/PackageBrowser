cmake_minimum_required(VERSION 3.16)

project(package_browser
    VERSION 0.1.0
    DESCRIPTION "Просмотр пакетов разных менеджеров: из какого окружения и какой версии"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Тип сборки" FORCE)
endif()

option(PACKAGE_BROWSER_BUILD_TESTS "Собирать тесты" ON)

add_subdirectory(src)

if(PACKAGE_BROWSER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()