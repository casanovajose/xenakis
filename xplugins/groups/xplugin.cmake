xen_define_xplugin(
    ID xen.groups
    TARGET xen_xplugin_groups
    CLASS xen::groups::GroupsXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.cpp"
)

if(NOT XEN_HEADLESS AND XEN_BUILD_GUI)
    add_library(xen_xplugin_groups_ui STATIC "${CMAKE_CURRENT_LIST_DIR}/ui/LayersPanel.cpp")
    target_link_libraries(xen_xplugin_groups_ui PUBLIC xen_xplugin_groups juce::juce_gui_basics)
    target_include_directories(xen_xplugin_groups_ui PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ui"
        "${CMAKE_SOURCE_DIR}/shells/include")
    target_compile_features(xen_xplugin_groups_ui PUBLIC cxx_std_20)
    set_property(GLOBAL APPEND PROPERTY XEN_UI_LIBS xen_xplugin_groups_ui)
endif()

if(TARGET xen_tests)
    target_sources(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests/GroupsTests.cpp")
    target_include_directories(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
