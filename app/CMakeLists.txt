# UI executable (Qt Widgets). Core lives in pb_core.

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Widgets Sql QUIET)
if(NOT Qt6_FOUND OR NOT Qt6Widgets_FOUND)
    message(FATAL_ERROR "Qt6 Widgets not found - cannot build the UI")
endif()

qt_standard_project_setup()

# UI models are shared between the app and its tests, so factor them out.
add_library(pb_ui_models STATIC
    ui/EnvironmentTreeModel.cpp
    ui/EnvironmentTreeModel.h
    ui/InstallationTableModel.cpp
    ui/InstallationTableModel.h
    ui/PackageFilterProxy.cpp
    ui/PackageFilterProxy.h
)

target_include_directories(pb_ui_models PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(pb_ui_models PRIVATE Qt6::Core Qt6::Gui)

add_executable(package_browser
    main.cpp
    ui/MainWindow.cpp
    ui/MainWindow.h
)

target_include_directories(package_browser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(package_browser PRIVATE pb_core pb_ui_models Qt6::Widgets Qt6::Sql)