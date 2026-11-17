juce_add_gui_app(xenakis_standalone
    PRODUCT_NAME "Xenakis"
    COMPANY_NAME "Xenakis"
    BUNDLE_ID "org.xenakis.standalone")

target_sources(xenakis_standalone PRIVATE Main.cpp)
target_include_directories(xenakis_standalone PRIVATE
    "${CMAKE_SOURCE_DIR}/shells/include"
    "${CMAKE_SOURCE_DIR}/xplugins/transport"
    "${CMAKE_SOURCE_DIR}/xplugins/transport/ui"
    "${CMAKE_SOURCE_DIR}/xplugins/paths/ui"
    "${CMAKE_SOURCE_DIR}/xplugins/groups/ui"
    "${CMAKE_SOURCE_DIR}/xplugins/osc/ui")
target_compile_definitions(xenakis_standalone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="Xenakis"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
get_property(xen_ui_libs GLOBAL PROPERTY XEN_UI_LIBS)
target_link_libraries(xenakis_standalone PRIVATE
    xen_xplugins
    xen_kernel
    ${xen_ui_libs}
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
