# Core of the application: domain model and services.
# UI (Qt Widgets) is added on later roadmap stages.

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Core Sql QUIET)
if(NOT Qt6_FOUND OR NOT Qt6Core_FOUND OR NOT Qt6Sql_FOUND)
    message(FATAL_ERROR "Qt6 Core/Sql not found")
endif()

qt_standard_project_setup()

# Aggregation of all core targets. The skeleton library is the target to which
# CatalogService, Scanner, Cache and Plugin Host will be added on roadmap stages 1-3.
set(PB_CORE_SOURCES
    core.cpp
    cache/Cache.cpp
    catalog/CatalogService.cpp
    catalog/ScanWorker.cpp
    domain/EnvironmentId.cpp
    domain/Environment.cpp
    domain/Ecosystem.cpp
    domain/PluginId.cpp
    normalization/Pep503Normalizer.cpp
    normalization/NameNormalizer.cpp
    plugin/PluginHost.cpp
    plugin/PluginManifest.cpp
    process/ProcessRunner.cpp
    provider/BuiltinPipProvider.cpp
    provider/DeclarativeProvider.cpp
    provider/JsonListParser.cpp
    provider/ProcessProvider.cpp
    rpc/ContentLengthFraming.cpp
    rpc/JsonRpcClient.cpp
    scanner/Scanner.cpp
    settings/AppSettings.cpp
    trust/TrustPolicy.cpp
)

set(PB_CORE_HEADERS
    cache/Cache.h
    catalog/CatalogService.h
    catalog/PluginInfo.h
    catalog/ScanWorker.h
    domain/EnvironmentId.h
    domain/Environment.h
    domain/Installation.h
    domain/ManagerKind.h
    domain/PluginId.h
    domain/Scope.h
    domain/ToolInstance.h
    provider/IProvider.h
    provider/BuiltinPipProvider.h
    provider/DeclarativeProvider.h
    provider/JsonListParser.h
    provider/ProcessProvider.h
    rpc/ContentLengthFraming.h
    rpc/JsonRpcClient.h
    normalization/NameNormalizer.h
    process/ProcessRunner.h
    plugin/PluginHost.h
    plugin/PluginManifest.h
    normalization/Pep503Normalizer.h
    scanner/Scanner.h
    settings/AppSettings.h
    trust/TrustPolicy.h
)

add_library(pb_core STATIC ${PB_CORE_SOURCES})

target_include_directories(pb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pb_core PRIVATE Qt6::Core Qt6::Sql)

# toml++ is header-only; PUBLIC so tests can parse manifests too.
target_link_libraries(pb_core PUBLIC tomlplusplus::tomlplusplus)

target_compile_options(pb_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)