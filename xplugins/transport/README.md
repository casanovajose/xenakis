# Transport

Global playhead. BPM or absolute time, play, pause, stop, seek, and a loop range. The kernel clock stays tempo-less; this xPlugin turns its ticks into musical time.

## Provides

- `xen::rt::TempoSource` on the compiled snapshot (`at` is the start of the latest tick, `end` is the time after it).
- Message-thread readings through `TransportXPlugin::reading`: seconds, beats, state.
- Component `Transport` on the singleton entity `transport`.

## Enhances

`xen.osc`, when that xPlugin is registered. Transport does not include it.

## Uses

The kernel control clock. One grid unit is `gridUnit` beats in BPM mode and `gridUnit` absolute units (`ms`, `s`, or `min`) in absolute mode. Default is 1 beat or 1 second.

## Commands

| Command | Arguments |
|---|---|
| `transport.play` | Anchors the live position and starts. |
| `transport.pause` | Freezes the live position. |
| `transport.stop` | Stops at `loopIn` when the loop is on, otherwise at 0. |
| `transport.seek` | `time` in seconds, zero or greater. Does not change play state. |
| `transport.toggleLoop` | Flips `loop`. |

`timeMode`, `bpm`, `meter`, `absoluteUnit`, `gridUnit`, `loop`, `loopIn`, and `loopOut` change through `SetProperty`.

## Events

`transport.started`, `transport.paused`, `transport.stopped`, `transport.seeked`, `transport.looped` (`seconds`, `beats`, `wraps`), `transport.tempoChanged` (`bpm`, `timeMode`), `transport.beat` (`bar`, `beat`, `seconds`, `beats`, BPM mode only).

Loop and beat notices are queued on the clock and published by `pump` on the message thread.

## Saved schema (`xen.transport` version 1)

```json
"xen.transport": {
  "Transport": {
    "timeMode": "bpm",
    "bpm": 120,
    "meter": "4/4",
    "absoluteUnit": "s",
    "gridUnit": 1,
    "loop": false,
    "loopIn": 0,
    "loopOut": 4,
    "state": "stopped",
    "position": 0,
    "epoch": 0,
    "jumped": false
  }
}
```

`state` is `stopped`, `playing`, or `paused`. Loading a project leaves the transport stopped and keeps `position`. `epoch` bumps whenever play, pause, stop, or seek sets a new anchor. Seconds and beats convert with `beats * 60 / bpm` at a constant tempo.

## Real time

`compile` copies the singleton into `rt/TransportClock` and publishes the `TempoSource` node. `tick` is `noexcept` and does not allocate. While playing, position is the anchor plus elapsed steady time, so it does not drift by summing nominal periods. A loop wraps into `[loopIn, loopOut)`.
