# Deviations from the plan and the essay

- **`requires` is spelled `required`.** C++20 uses `requires` as a keyword, so `XPluginInfo::required` is the hard-dependency list.
- **The control clock's sleep backend lives in the kernel**, in `PreciseSleep_win.cpp` or `PreciseSleep_posix.cpp`, selected by CMake. The plan says platform code stays in `shells/`. The kernel owns the tempo-less tick, so the backend cannot move to a shell without the kernel calling back into one. Shared headers still have no platform `#ifdef`.
- **`.xenproj` is parsed with nlohmann/json.** The kernel and the headless tests do not link JUCE. JUCE 8 stays in the standalone shell and in the OSC and canvas spikes. `juce_osc` is what the OSC spike sends.
- **`tools/xplugin remove` is not in M1.** The milestone asks for `new` and `lint`. `list` is included because it is how a human sees what lint will check.
- **No feature contracts yet.** `TempoSource` arrives with `xen.transport` (M2). `contracts/` only carries a version constant so the directory and the include rule exist.
- **UI slots are ids, not widgets.** The kernel registry can name a slot. The frame, toolbar, time grid, and panels are M5. The standalone shell opens a dark window and lists the registered xPlugins.
- **`xenakis run` is refused.** Headless playback is M2. `xenakis load` reads a project and prints its entities.
- **Snapshot reclamation is explicit.** The clock publishes an acknowledgement after each tick. The message thread frees older snapshots only after that, or after the clock thread has been joined. `std::atomic<std::shared_ptr>` is not used, because its typical implementation takes a lock.
- **Groups has no build switch.** It is always on, as the essay says. Other xPlugins get `XEN_HEADLESS` for their `ui/` target only.
- **The top-level CMake project enables C as well as C++.** JUCE's `juceaide` tool is built as part of configuring the GUI and OSC targets, and that tool needs a C compiler.
