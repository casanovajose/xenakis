xen_define_xplugin(
    ID __ID__
    TARGET xen_xplugin___FOLDER__
    CLASS xen::__NS__::__CLASS__
    HEADER "${CMAKE_CURRENT_LIST_DIR}/__CLASS__.h"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/__CLASS__.cpp"
)

if(NOT XEN_HEADLESS)
    add_library(xen_xplugin___FOLDER___ui STATIC
        "${CMAKE_CURRENT_LIST_DIR}/ui/__CLASS__Ui.cpp")
    target_link_libraries(xen_xplugin___FOLDER___ui PUBLIC xen_xplugin___FOLDER__)
    target_include_directories(xen_xplugin___FOLDER___ui PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
endif()

if(TARGET xen_tests)
    target_sources(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests/smoke.cpp")
    target_include_directories(xen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
