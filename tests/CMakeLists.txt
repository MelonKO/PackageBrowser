find_package(Qt6 COMPONENTS Core Test QUIET)

if(NOT Qt6_FOUND OR NOT Qt6Test_FOUND)
    message(STATUS "Qt6 not found - tests are not built")
    return()
endif()

qt_standard_project_setup()

add_executable(pb_core_tests test_core.cpp)
target_link_libraries(pb_core_tests PRIVATE pb_core Qt6::Test)
add_test(NAME pb_core_tests COMMAND pb_core_tests)

add_executable(pb_pep503_tests test_pep503.cpp)
target_link_libraries(pb_pep503_tests PRIVATE pb_core Qt6::Test)
add_test(NAME pb_pep503_tests COMMAND pb_pep503_tests)

add_executable(pb_domain_tests test_domain.cpp)
target_link_libraries(pb_domain_tests PRIVATE pb_core Qt6::Test)
add_test(NAME pb_domain_tests COMMAND pb_domain_tests)