---
name: xplugin
description: Scaffolds and checks a Xenakis xPlugin. Use when adding a feature plugin, running tools/xplugin, editing xplugins/, or wiring xplugin.cmake, contribute, compile, or migrate.
---

# xPlugin workflow

## Add

```text
python tools/xplugin new <folder>
```

`<folder>` matches `[a-z][a-z0-9_]*`. The tool creates `xplugins/<folder>/` from `tools/templates/xplugin/` with id `xen.<folder>`.

Then:

1. Register component types, properties, and commands in `contribute`.
2. Save only this plugin's JSON under `components["xen.<folder>"]`.
3. Bump `info().version` and implement `migrate` when saved data changes.
4. Keep `rt/` `noexcept` and free of heap traffic.
5. Gate any `ui/` library with `if(NOT XEN_HEADLESS)`.

## Check

```text
python tools/xplugin lint
cmake --build build
ctest --test-dir build --output-on-failure
```

Lint fails if a plugin includes or links a sibling, or if `ui/` is compiled into the core target. A negative sample lives in `tools/testdata/cross_include`.

## Rules that are easy to break

- Do not include another plugin to read its data. Read its component JSON, documented in that plugin's README.
- Do not static-initialize plugins. `xplugin.cmake` feeds the generated `createXPlugins()`.
- `XPluginInfo::required` is the hard dependency list (`requires` is a C++20 keyword).
- Groups stays always on. New plugins declare `required` or `enhances`; they do not edit groups internals.
