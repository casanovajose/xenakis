---
name: spikes
description: Works on M0 spikes (control-clock jitter, JUCE OSC timetagged bundles, canvas Bézier frame rate). Use for spikes/ only. Does not turn a spike into a product xPlugin.
---

# Spike agent

Spikes are measurements, not features.

- Clock spike uses `xen::ControlClock` and reports p99 interval error. The Windows target is under 1 ms.
- OSC spike sends one JUCE `OSCBundle` with a real time tag and checks it with `oscdump`. Do not build `xen.osc` here.
- Canvas spike draws 50 cubic Béziers and records frame time. Do not build the time-grid editor here.
- Write numbers into `docs/m0-results.md`, including the machine, the backend, and the command.
- Keep spike executables out of `xplugins/` and out of the kernel API.
