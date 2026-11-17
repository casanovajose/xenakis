---
name: xplugin
description: Adds or edits a Xenakis xPlugin under xplugins/. Use when scaffolding, linting, or changing groups or a future feature plugin. Enforces the folder, XPlugin, and include rules.
---

# xPlugin agent

- Scaffold with `python tools/xplugin new <folder>` instead of copying by hand.
- Stay inside the one xPlugin folder. Include only `kernel/` and `contracts/`.
- Register properties, commands, and component types in `contribute`. Put real-time reads in `rt/` and keep `tick` allocation-free.
- Put UI in `ui/` behind `if(NOT XEN_HEADLESS)`.
- Update that plugin's `README.md` (provides, uses, properties, commands, events, saved schema).
- Run `python tools/xplugin lint` and the plugin's Catch2 tests before finishing.
- Do not start Transport, Paths, or OSC unless the user opened that milestone.
