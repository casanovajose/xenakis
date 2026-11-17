# Deviations from the plan and the essay

- **`requires` is spelled `required`.** C++20 uses `requires` as a keyword, so `XPluginInfo::required` is the hard-dependency list.
- **The control clock's sleep backend lives in the kernel**, in `PreciseSleep_win.cpp` or `PreciseSleep_posix.cpp`, selected by CMake. The plan says platform code stays in `shells/`. The kernel owns the tempo-less tick, so the backend cannot move to a shell without the kernel calling back into one. Shared headers still have no platform `#ifdef`.
- **`.xenproj` is parsed with nlohmann/json.** The kernel and the headless tests do not link JUCE. JUCE 8 stays in the standalone shell and in the OSC and canvas spikes. `juce_osc` is what the OSC spike sends.
- **`tools/xplugin remove` is not in M1.** The milestone asks for `new` and `lint`. `list` is included because it is how a human sees what lint will check.
- **`TempoSource` and `PositionTap` live in `contracts/`.** They arrived with transport and paths. The kernel still does not include them.
- **UI slots in the kernel are still ids.** The widgets are in each xPlugin `ui/` target and in the standalone frame.
- **`xenakis run piece.xenproj --no-audio` plays.** Audio output is not in this release, so the flag is required.
- **Snapshot reclamation is explicit.** The clock publishes an acknowledgement after each tick. The message thread frees older snapshots only after that, or after the clock thread has been joined. `std::atomic<std::shared_ptr>` is not used, because its typical implementation takes a lock.
- **Groups has no build switch.** It is always on, as the essay says. Other xPlugins get `XEN_HEADLESS` for their `ui/` target only.
- **The top-level CMake project enables C as well as C++.** JUCE's `juceaide` tool is built as part of configuring the GUI and OSC targets, and that tool needs a C compiler.
- **`XPlugin` gained `afterLoad`, `onCompiled`, and `pump`.** Load replaces the document, so a plugin that owns a singleton has to recreate it. Real-time notices are queued on the clock and published on the message thread. The kernel still does not name an xPlugin.
- **`TempoSource::end` and `wallUnixAt`.** The plan's `at` is the start of the block. Paths also need the time after the tick, and OSC needs a Unix time for a transport position. Both are on the contract.
- **Headless OSC does not link JUCE.** `xen.osc` writes OSC 1.0 bundles itself. The M0 spike still proves `juce_osc`. A headless configure stays free of JUCE. `oscdump` is not installed on this machine; the fixture test decodes the UDP packet into the same `timetag address types args` line `oscdump` prints.
- **Cursor `pattern` is a string.** The property registry has no array type. `1 -1 0` is the saved form of the IanniX pattern.
- **The layers tree is assembled by the frame.** Groups and paths both contribute rows, and neither may include the other. The groups `ui/` reads the path JSON schema. The time grid, pan, and snap live in the paths canvas, which is the frame's centre slot.
