# M0 spike results

Measured on this machine: Windows build 10.0.26200. The control clock was built with Clang 23.1.5 (MinGW target, the Windows sleep backend). The JUCE spikes were built with MSVC 19.16.27045, the newest compiler installed here. JUCE 8.0.17's `juceaide` does not compile on that compiler until two `int`-to-`uint32` conversions in the fetched `juce_FontOptions.h` are cast. That cast is only in the local build tree. CI compiles the unmodified tag on a current Visual Studio.

## Control clock

Command: `xen_spike_clock --seconds 8 --period-ms 5 --warmup 20 --strict`

Backend: `CreateWaitableTimerEx` high-resolution, `timeBeginPeriod(1)`, then a short spin.

| | |
|---|---|
| Period | 5 ms |
| Run | 8 s, 1600 intervals |
| Warmup skipped | 20 intervals |
| p50 absolute interval error | 0.20 µs |
| p99 absolute interval error | 291 µs |
| Max absolute interval error | 3184 µs |
| p99 under 1 ms | yes |

The criterion is the 99th percentile, not the single worst tick. p99 is 291 µs.

## OSC timetagged bundle

`liblo` 0.32 `oscdump` received one JUCE `OSCBundle` sent to `127.0.0.1` with a 10 ms time tag. The tag is not the OSC "immediate" sentinel.

```text
eea62767.522d0e56 /xen/ping ss "xenakis" "0.1.0-m1"
```

`eea62767.522d0e56` is the NTP time tag. Arguments are the two strings from plan section 6's test message.

## Canvas, 50 cubic Béziers

`XenakisCanvasSpike` drew 50 cubic Béziers into a 1280×720 image for 180 frames, then presented the same paths in a window.

| | |
|---|---|
| Offscreen paint p50 | 1.48 ms |
| Offscreen paint p99 | 2.15 ms (about 464 fps) |
| Offscreen paint max | 2.52 ms |
| Presented interval median | 1.99 ms (about 502 fps) |
| Presented interval p99 | 266 ms |

Paint time stays under a 60 fps budget. The presented median is far above 60 fps. The presented p99 is one hitch while the window was coming up, not the cost of the paths.
