juce_add_console_app(xen_spike_osc PRODUCT_NAME "xen-spike-osc")
target_sources(xen_spike_osc PRIVATE main.cpp)
target_compile_definitions(xen_spike_osc PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(xen_spike_osc PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_osc
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# liblo's oscdump is the receiver named by the M0 acceptance check.
# It is a separate process, not linked into the product.
include(FetchContent)
FetchContent_Declare(liblo
    GIT_REPOSITORY https://github.com/radarsat1/liblo.git
    GIT_TAG 0.32
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(liblo)
if(NOT liblo_POPULATED)
    FetchContent_Populate(liblo)
endif()
