xen_define_xplugin(
    ID xen.groups
    TARGET xen_xplugin_groups
    CLASS xen::groups::GroupsXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/GroupsXPlugin.cpp"
)
