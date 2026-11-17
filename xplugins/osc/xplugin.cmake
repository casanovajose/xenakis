xen_define_xplugin(
    ID xen.osc
    TARGET xen_xplugin_osc
    CLASS xen::osc::OscXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/OscXPlugin.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/OscXPlugin.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/UdpSender.cpp"
)

if(WIN32)
    target_link_libraries(xen_xplugin_osc PUBLIC ws2_32)
endif()

if(NOT XEN_HEADLESS AND XEN_BUILD_GUI)
    add_library(xen_xplugin_osc_ui STATIC "${CMAKE_CURRENT_LIST_DIR}/ui/OscPanel.cpp")
    target_link_libraries(xen_xplugin_osc_ui PUBLIC xen_xplugin_osc juce::juce_gui_basics)
    target_include_directories(xen_xplugin_osc_ui PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ui"
        "${CMAKE_SOURCE_DIR}/shells/include")
    target_compile_features(xen_xplugin_osc_ui PUBLIC cxx_std_20)
    set_property(GLOBAL APPEND PROPERTY XEN_UI_LIBS xen_xplugin_osc_ui)
endif()

if(TARGET xen_tests)
    target_sources(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests/OscTests.cpp")
    target_include_directories(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    if(WIN32)
        target_link_libraries(xen_tests PRIVATE ws2_32)
    endif()
endif()
