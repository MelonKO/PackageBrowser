# Core of the application: domain model and services.
# UI (Qt Widgets) is added on later roadmap stages.

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Core QUIET)
if(NOT Qt6_FOUND OR NOT Qt6Core_FOUND)
    message(FATAL_ERROR "Qt6 Core not found")
endif()

qt_standard_project_setup()

# Aggregation of all core targets. The skeleton library is the target to which
# CatalogService, Scanner, Cache and Plugin Host will be added on roadmap stages 1-3.
set(PB_CORE_SOURCES
    core.cpp
    domain/EnvironmentId.cpp
    domain/Environment.cpp
    domain/Ecosystem.cpp
    domain/PluginId.cpp
    normalization/Pep503Normalizer.cpp
)

set(PB_CORE_HEADERS
    domain/EnvironmentId.h
    domain/Environment.h
    domain/Installation.h
    domain/ManagerKind.h
    domain/PluginId.h
    domain/Scope.h
    domain/ToolInstance.h
    provider/IProvider.h
    normalization/Pep503Normalizer.h
)

add_library(pb_core STATIC ${PB_CORE_SOURCES})

target_include_directories(pb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pb_core PRIVATE Qt6::Core)

target_compile_options(pb_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)