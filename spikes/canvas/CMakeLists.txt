juce_add_gui_app(xen_spike_canvas
    PRODUCT_NAME "XenakisCanvasSpike"
    COMPANY_NAME "Xenakis")
target_sources(xen_spike_canvas PRIVATE Main.cpp)
target_compile_definitions(xen_spike_canvas PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(xen_spike_canvas PRIVATE
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
