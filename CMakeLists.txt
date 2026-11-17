cmake_minimum_required(VERSION 3.24)
project(Xenakis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XEN_BUILD_TESTS "Build Catch2 tests" ON)
option(XEN_BUILD_SPIKES "Build the clock spike" ON)
option(XEN_BUILD_GUI "Build the standalone shell and the canvas spike" ON)
option(XEN_BUILD_OSC_SPIKE "Build the JUCE OSC spike" ON)
option(XEN_HEADLESS "Omit xPlugin ui targets" OFF)

find_package(Threads REQUIRED)
include(cmake/Dependencies.cmake)

add_subdirectory(kernel)
add_subdirectory(contracts)

if(XEN_BUILD_TESTS)
    enable_testing()
    if(NOT Catch2_SOURCE_DIR)
        message(FATAL_ERROR "Catch2 was not fetched")
    endif()
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
    include(Catch)
    add_subdirectory(tests)
endif()

include(cmake/XenXPlugins.cmake)
file(GLOB xen_plugin_manifests CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/xplugins/*/xplugin.cmake")
foreach(manifest IN LISTS xen_plugin_manifests)
    include("${manifest}")
endforeach()
xen_generate_xplugin_registration()
if(TARGET xen_tests)
    target_link_libraries(xen_tests PRIVATE xen_xplugins)
endif()

add_subdirectory(shells)
if(XEN_BUILD_SPIKES OR XEN_BUILD_GUI OR XEN_BUILD_OSC_SPIKE)
    add_subdirectory(spikes)
endif()

if(XEN_BUILD_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME xplugin_lint COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/xplugin" lint)
    add_test(NAME kernel_boundaries COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/check_kernel_boundaries.py")
    add_test(NAME lint_rejects_cross_include COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/check_lint_fails.py")
    add_test(NAME cli_version COMMAND xenakis version)
    set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "xenakis 0\\.1\\.0-m5")
    add_test(
        NAME cli_run
        COMMAND xenakis run "${CMAKE_SOURCE_DIR}/tests/fixtures/transport.xenproj" --no-audio
                --commands "play;wait 0.05;pause;seek 1;stop")
    set_tests_properties(cli_run PROPERTIES PASS_REGULAR_EXPRESSION "command=stop state=stopped seconds=0\\.000000 beats=0\\.000000")
endif()
