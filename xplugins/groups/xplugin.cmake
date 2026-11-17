xen_define_xplugin(
    ID xen.groups
    TARGET xen_xplugin_groups
    CLASS xen::groups::GroupsXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.cpp"
)

if(TARGET xen_tests)
    target_sources(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests/GroupsTests.cpp")
    target_include_directories(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
