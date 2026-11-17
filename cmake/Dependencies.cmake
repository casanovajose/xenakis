set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    message(STATUS "Downloading CPM.cmake ${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${CPM_DOWNLOAD_LOCATION}"
        STATUS CPM_DOWNLOAD_STATUS
        TLS_VERIFY ON)
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_CODE)
    if(NOT CPM_DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR "CPM.cmake download failed: ${CPM_DOWNLOAD_STATUS}")
    endif()
endif()
include("${CPM_DOWNLOAD_LOCATION}")

CPMAddPackage("gh:nlohmann/json@3.11.3")

if(XEN_BUILD_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.8.0")
endif()

if(XEN_BUILD_GUI OR XEN_BUILD_OSC_SPIKE)
    CPMAddPackage(
        NAME JUCE
        GITHUB_REPOSITORY juce-framework/JUCE
        GIT_TAG 8.0.17-security
        OPTIONS "JUCE_BUILD_EXTRAS OFF" "JUCE_BUILD_EXAMPLES OFF"
    )
endif()
