# Test suite for heatshrink. Built with CTest.

set(HS_TEST_INCLUDES ${PROJECT_SOURCE_DIR}/vendor)

add_executable(test_heatshrink_dynamic
    test_heatshrink_dynamic.c
    test_heatshrink_dynamic_theft.c)
target_include_directories(test_heatshrink_dynamic PRIVATE ${HS_TEST_INCLUDES})
target_link_libraries(test_heatshrink_dynamic PRIVATE heatshrink_dynamic)
hs_set_warnings(test_heatshrink_dynamic)

add_executable(test_heatshrink_static test_heatshrink_static.c)
target_include_directories(test_heatshrink_static PRIVATE ${HS_TEST_INCLUDES})
target_link_libraries(test_heatshrink_static PRIVATE heatshrink_static)
hs_set_warnings(test_heatshrink_static)

add_test(NAME heatshrink_dynamic COMMAND test_heatshrink_dynamic)
add_test(NAME heatshrink_static COMMAND test_heatshrink_static)

set_tests_properties(heatshrink_dynamic heatshrink_static PROPERTIES
    TIMEOUT 600)

if(TARGET heatshrink_cmd)
    add_test(NAME cli_roundtrip
        COMMAND ${CMAKE_COMMAND}
            -DHEATSHRINK=$<TARGET_FILE:heatshrink_cmd>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/cli_roundtrip
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_roundtrip.cmake)
    set_tests_properties(cli_roundtrip PROPERTIES TIMEOUT 300)
endif()
