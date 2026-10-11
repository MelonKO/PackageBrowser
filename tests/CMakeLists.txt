find_package(Qt6 COMPONENTS Core Test QUIET)

if(NOT Qt6_FOUND OR NOT Qt6Test_FOUND)
    message(STATUS "Qt6 не найден — тесты не собираются")
    return()
endif()

qt_standard_project_setup()

add_executable(pb_core_tests test_core.cpp)
target_link_libraries(pb_core_tests PRIVATE pb_core Qt6::Test)
add_test(NAME pb_core_tests COMMAND pb_core_tests)