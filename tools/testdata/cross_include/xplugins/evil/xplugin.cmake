xen_define_xplugin(
    ID xen.evil
    TARGET xen_xplugin_evil
    CLASS xen::evil::EvilXPlugin
    HEADER "${CMAKE_CURRENT_LIST_DIR}/Evil.cpp"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/Evil.cpp"
)
