# Xenakis — agent guide

Xenakis is a visual sequencer (paths, cursors, triggers, OSC) built as a small kernel plus compiled xPlugins. Inspiration: IanniX. This file is the source of truth for agents working in this repo.

## Milestone fence

M0 through M5 are in the tree: kernel, `xen.groups`, `xen.transport`, `xen.paths`, `xen.osc`, and the standalone frame. Do not start the Windows installer (M6) unless the user asks. `xenakis run piece.xenproj --no-audio` is the headless player.

## Architecture

- Dependencies point one way: `kernel` ← `contracts` ← `xplugins` ← `shells`.
- The kernel does not mention any xPlugin by name, id, or header. Composition happens only in the generated registration TU.
- Each xPlugin lives in `xplugins/<folder>/` with `xplugin.cmake`, implements `xen::XPlugin` (`info`, `contribute`, `activate`, `deactivate`, `compile`, `migrate`), and includes only `kernel/` and `contracts/`.
- An xPlugin writes only its own components. It registers properties in the property registry. Readers may read another xPlugin's component JSON; they may not include its headers.
- `rt/` code is `noexcept` and allocation-free. Snapshot objects are built on the message thread inside `compile`.
- `ui/` is a separate CMake target, omitted when `XEN_HEADLESS=ON`, and never linked into the CLI or kernel tests.
- C++20. `requires` is a keyword, so `XPluginInfo::required` is the hard-dependency list the essay calls `requires`.
- No platform `#ifdef` in shared headers. The control clock's sleep backend is selected by CMake (see `docs/deviations.md`).

## Layout

- `kernel/` — document, commands and undo, events, property registry, xPlugin registry, control clock, compile and snapshot swap, real-time channel, `.xenproj`, UI slot ids.
- `contracts/` — shared interface headers only. No feature contracts until the xPlugin that provides them.
- `xplugins/groups/` — containment, `visible`, `mute`. Always on.
- `xplugins/transport/` — playhead, `TempoSource`.
- `xplugins/paths/` — paths, cursors, triggers.
- `xplugins/osc/` — UDP OSC output.
- `shells/cli`, `shells/standalone` — hosts. The standalone frame is not an xPlugin.
- `tools/xplugin` — `new`, `lint`, `list`.
- `spikes/` — M0 clock, OSC bundle, canvas. Not product features.
- `tests/` — kernel Catch2 tests.

## Commands agents should run

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python tools/xplugin lint
```

Scaffold a plugin with `python tools/xplugin new <folder>` and do not commit the scaffold unless it is a real feature.

## Product rules already decided

Grid time is arc length. One grid unit is one beat (BPM) or one second (absolute). Armed cursors follow the transport: `cursorTime = transportTime − delay`. OSC addresses and the `.xenproj` shape are in `docs/decisions.md`. Groups mute is data: a muted group stays in the snapshot; later OSC drops its events.
