xen_define_xplugin(
    ID xen.transport
    TARGET xen_xplugin_transport
    CLASS xen::transport::TransportXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/TransportXPlugin.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/TransportXPlugin.cpp"
)

if(NOT XEN_HEADLESS AND XEN_BUILD_GUI)
    add_library(xen_xplugin_transport_ui STATIC "${CMAKE_CURRENT_LIST_DIR}/ui/TransportBar.cpp")
    target_link_libraries(xen_xplugin_transport_ui PUBLIC xen_xplugin_transport juce::juce_gui_basics)
    target_include_directories(xen_xplugin_transport_ui PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ui"
        "${CMAKE_SOURCE_DIR}/shells/include")
    target_compile_features(xen_xplugin_transport_ui PUBLIC cxx_std_20)
    set_property(GLOBAL APPEND PROPERTY XEN_UI_LIBS xen_xplugin_transport_ui)
endif()

if(TARGET xen_tests)
    target_sources(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests/TransportTests.cpp")
    target_include_directories(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
