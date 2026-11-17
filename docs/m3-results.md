# M3 acceptance

Headless Release build, Clang 23.1.5 MinGW. Grid unit is one beat. 120 BPM makes that beat 0.500 s.

| Check | Result |
|---|---|
| Grid, 120 BPM, 8-unit path, speed 1 | Pass duration is 4 exactly. A live pass ends at transport time 4. Over the same run the playhead advanced 4.05365 s while the wall clock advanced 4.05365 s |
| Absolute, duration 2 s | Pass duration is 2 exactly. The live pass ends at transport time 2. Wall and playhead both advanced 2.05022 s |
| Pattern `1 -1 0`, trigger at u 0.5 | Two fires, direction 1 then −1. The same pair is produced by the pure crossing function and by a live clock run |
| 500 passes per second | 200 slices of 5 ms, and one span of a whole second, each report 500 crossings and no extras |
| Seek | `placeAt` is a pure function of transport time. A span that starts after the trigger does not fire it; the span that contains it fires once |

A saved project reloads the same cursor pattern and trigger, and the crossing function still reports direction 1 then −1.
