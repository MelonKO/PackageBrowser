find_package(Qt6 COMPONENTS Core Test QUIET)

if(NOT Qt6_FOUND OR NOT Qt6Test_FOUND)
    message(STATUS "Qt6 not found - tests are not built")
    return()
endif()

qt_standard_project_setup()

function(pb_add_test name source)
    add_executable(${name} ${source})
    if(WIN32)
        set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
    target_link_libraries(${name} PRIVATE pb_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

pb_add_test(pb_core_tests test_core.cpp)
pb_add_test(pb_pep503_tests test_pep503.cpp)
pb_add_test(pb_domain_tests test_domain.cpp)
pb_add_test(pb_cache_tests test_cache.cpp)
pb_add_test(pb_scan_tests test_scan.cpp)