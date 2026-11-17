# M2 acceptance

Measured on this machine (Windows 10.0.26200), headless Release build, Clang 23.1.5 MinGW. Clock period 5 ms.

| Check | Result |
|---|---|
| Play, pause, stop, seek from tests | pass |
| `xenakis run tests/fixtures/transport.xenproj --no-audio --commands "play;wait 0.05;pause;seek 1;stop"` | pass. Seek lands on `seconds=1.000000 beats=2.000000`. Stop lands on `seconds=0.000000 beats=0.000000` |
| Seconds ↔ beats at a constant tempo | exact. 120 BPM: 2 beats = 1 s. 60 BPM: 1 beat = 1 s. `beatsToSeconds(secondsToBeats(4))` is 4 |
| Drift against the wall clock | `XEN_DRIFT_SECONDS=600`: `drift_seconds=3.00122e-07` over `elapsed=600.001` (0.0003 ms) |
| Loop range | 0.2 s loop, 0.55 s of play, position stays inside `[0, 0.2)` and the wrap count is at least 2 |

The default ctest window is 2 s with the same 1 ms limit. Set `XEN_DRIFT_SECONDS` to measure a longer run.
