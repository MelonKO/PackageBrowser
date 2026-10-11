# UI executable (Qt Widgets). Core lives in pb_core.

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Widgets Sql QUIET)
if(NOT Qt6_FOUND OR NOT Qt6Widgets_FOUND)
    message(FATAL_ERROR "Qt6 Widgets not found - cannot build the UI")
endif()

qt_standard_project_setup()

add_executable(package_browser
    main.cpp
    ui/EnvironmentTreeModel.cpp
    ui/EnvironmentTreeModel.h
    ui/InstallationTableModel.cpp
    ui/InstallationTableModel.h
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PackageFilterProxy.cpp
    ui/PackageFilterProxy.h
)

target_include_directories(package_browser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(package_browser PRIVATE pb_core Qt6::Widgets Qt6::Sql)