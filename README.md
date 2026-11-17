# Xenakis

Visual sequencer: paths, cursors, triggers, and OSC, on a small kernel and compiled xPlugins.

M0 and M1 are in this tree. Transport, paths, and OSC output are later milestones. The decisions those milestones must follow are in [docs/decisions.md](docs/decisions.md). Measured spike results go in [docs/m0-results.md](docs/m0-results.md).

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

`xenakis version` and `xenakis load piece.xenproj` are the CLI. `xenakis run` waits for M2.

## Layout

`kernel/` does not name an xPlugin. Each feature is `xplugins/<name>/` with `xplugin.cmake`. `python tools/xplugin new <folder>` scaffolds one. Agents should read [AGENTS.md](AGENTS.md) before editing.

JUCE 8 is AGPLv3 (or a commercial licence) and is fetched by CMake. It is not vendored here.
