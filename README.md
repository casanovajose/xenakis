# Xenakis

Visual sequencer: paths, cursors, triggers, and OSC, on a small kernel and compiled xPlugins.

M0 through M5 are in this tree: the kernel, groups, transport, paths, OSC, and the standalone frame. The installer is not. Decisions are in [docs/decisions.md](docs/decisions.md). Measurements are in [docs/m0-results.md](docs/m0-results.md) and [docs/m2-results.md](docs/m2-results.md).

## Build

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/xplugin lint
```

Headless (no JUCE, no `ui/` targets):

```text
cmake -S . -B build -DXEN_HEADLESS=ON -DXEN_BUILD_GUI=OFF -DXEN_BUILD_OSC_SPIKE=OFF
```

`xenakis version` and `xenakis load piece.xenproj` read a project. `xenakis run piece.xenproj --no-audio` plays it. The standalone window is `Xenakis.exe` from a GUI build (`-DXEN_HEADLESS=OFF -DXEN_BUILD_GUI=ON`).

## Layout

`kernel/` does not name an xPlugin. Each feature is `xplugins/<name>/` with `xplugin.cmake`. `python tools/xplugin new <folder>` scaffolds one. Agents should read [AGENTS.md](AGENTS.md) before editing.

JUCE 8 is AGPLv3 (or a commercial licence) and is fetched by CMake. It is not vendored here.
