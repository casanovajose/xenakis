juce_add_gui_app(xenakis_standalone
    PRODUCT_NAME "Xenakis"
    COMPANY_NAME "Xenakis"
    BUNDLE_ID "org.xenakis.standalone")

target_sources(xenakis_standalone PRIVATE Main.cpp)
target_compile_definitions(xenakis_standalone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="Xenakis"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(xenakis_standalone PRIVATE
    xen_xplugins
    xen_kernel
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
