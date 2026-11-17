add_library(xen_kernel STATIC
    src/Document.cpp
    src/EventBus.cpp
    src/Command.cpp
    src/PropertyRegistry.cpp
    src/ComponentRegistry.cpp
    src/Registry.cpp
    src/UiSlots.cpp
    src/Snapshot.cpp
    src/ControlClock.cpp
    src/XPluginRegistry.cpp
    src/Engine.cpp
)

if(WIN32)
    target_sources(xen_kernel PRIVATE src/PreciseSleep_win.cpp)
else()
    target_sources(xen_kernel PRIVATE src/PreciseSleep_posix.cpp)
endif()

target_include_directories(xen_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(xen_kernel PUBLIC cxx_std_20)
target_link_libraries(xen_kernel PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
    target_link_libraries(xen_kernel PUBLIC winmm)
endif()

if(MSVC)
    target_compile_options(xen_kernel PRIVATE /W4 /permissive-)
else()
    target_compile_options(xen_kernel PRIVATE -Wall -Wextra -Wpedantic)
endif()
