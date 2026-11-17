# Paths

2D polylines and cubic Béziers, cursors, and triggers. Time along a path is arc length. An armed cursor is `cursorTime = transportTime − delay`.

## Requires

`xen.transport`, for the `TempoSource` contract. This folder does not include that xPlugin. BPM, meter, and grid unit are read from the `Transport` JSON described in the transport README.

## Enhances

`xen.groups` and `xen.osc` when they are registered. Group mute is read from `Group` / `GroupMember` JSON. A muted path stays in the snapshot; its events are marked `muted`.

## Provides

- Components `Path`, `Cursor`, and `Trigger`.
- `PositionTap` for the cursor position stream.
- Events `trigger.fired`, `cursor.started`, `cursor.passEnded`, `cursor.stopped`.

## Commands

| Command | Arguments |
|---|---|
| `AddPath` | `name`, `points` (array of `{x, y}` or a cubic `{x, y, c1x, c1y, c2x, c2y}`). |
| `EditPathPoints` | `path`, `points`, optional `closed`. |
| `DeletePath` | `path` or `entity`. |
| `AddCursor` | `path`, `name`. |
| `MoveCursor` | `cursor`, `u` (writes `start`). |
| `AddTrigger` | `path`, `u`, `name`. |
| `MoveTrigger` | `trigger`, `u`. |
| `Delete` | `entity`. Removes this xPlugin's component. |
| `cursor.arm` / `cursor.disarm` | `cursor`. |

The host puts a path in a group with `MoveToGroup`. Paths does not write `xen.groups` components.

`pattern` is a string such as `1`, `1 0`, `1 -1`, or `1 -1 0`. `0` ends the pattern. Without it, the pattern repeats. `timingMode` is `grid` or `absolute`. Absolute `timingUnit` is `ms`, `s`, `min`, `beats`, or `bars`.

## Saved schema (`xen.paths` version 1)

```json
"xen.paths": {
  "Path": { "name": "Line", "closed": false, "points": [{ "x": 0, "y": 0 }, { "x": 8, "y": 0 }], "look": {} },
  "Cursor": {
    "name": "C1", "path": "P1", "timingMode": "grid", "timingSpeed": 1, "timingDuration": 2,
    "timingUnit": "s", "pattern": "1", "delay": 0, "start": 0, "loopIn": 0, "loopOut": 1,
    "speedFactor": 1, "armed": true, "stream": false
  },
  "Trigger": { "name": "T1", "path": "P1", "u": 0.5, "cursors": "all", "direction": "both", "enabled": true }
}
```

`u`, `start`, `loopIn`, and `loopOut` are fractions of arc length. Grid pass duration is `arcLength(loopIn → loopOut) × gridUnitSeconds / (speed × speedFactor)`. One grid unit is one beat at the transport tempo, unless the transport says otherwise.

## Real time

`compile` builds an arc-length table and a cursor description. `tick` is `noexcept` and does not allocate. A trigger inside the u span of a tick fires at the interpolated transport time. Seeking sets the jumped flag and does not replay triggers.
