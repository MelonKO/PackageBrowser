cmake_minimum_required(VERSION 3.16)

project(package_browser
    VERSION 0.1.0
    DESCRIPTION "Browse packages of different managers: which environment and version"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(PACKAGE_BROWSER_BUILD_TESTS "Build tests" ON)
option(PACKAGE_BROWSER_BUILD_UI "Build UI" ON)

# Plugin manifests (plugin.toml) are parsed with toml++. Header-only, no
# compiled library, so the dependency stays a build-time fetch only.
include(FetchContent)
set(TOMLPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TOMLPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TOMLPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory(src)

if(PACKAGE_BROWSER_BUILD_UI)
    add_subdirectory(app)
endif()

if(PACKAGE_BROWSER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()