---
name: kernel
description: Edits the Xenakis kernel and contracts. Use for the document, commands, undo, events, property registry, xPlugin registry, control clock, snapshot swap, .xenproj, or UI slot ids. Never names an xPlugin.
---

# Kernel agent

Work only under `kernel/`, `contracts/`, and kernel tests in `tests/`.

- Do not mention `xen.groups`, `xen.paths`, `xen.transport`, `xen.osc`, or include `xplugins/`.
- Keep the public surface in `kernel/include/xen/`. Message-thread APIs may throw `std::runtime_error`. Clock APIs are `noexcept`.
- `.xenproj` is JSON, format `xenproj/1`. Unknown xPlugin blobs are preserved verbatim.
- Platform sleep code stays in `PreciseSleep_win.cpp` or `PreciseSleep_posix.cpp`, chosen by CMake. No `#ifdef` in headers.
- Do not add musical time, tempo, paths, or OSC addresses here. Those are later xPlugins.
- After a behavior change, extend Catch2 tests and run `ctest --test-dir build --output-on-failure`.
