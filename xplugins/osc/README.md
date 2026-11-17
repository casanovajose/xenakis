# OSC

Sends OSC 1.0 UDP from transport, cursor, and trigger events. Nothing is sent on the clock thread. A sender thread reads a fixed queue.

## Uses

Events `trigger.fired`, `cursor.started`, `cursor.passEnded`, `cursor.stopped`, `transport.started`, `transport.paused`, `transport.stopped`, and `transport.beat`. Cursor position samples come from `PositionTap` when a cursor's `stream` is on. Timetags use `TempoSource::wallUnixAt`. This folder does not include those xPlugins. Muted events are dropped.

The packet encoder is in this xPlugin, not JUCE, so a headless build does not fetch JUCE. The bytes are OSC 1.0 bundles: `#bundle`, an NTP timetag, then the message.

## Commands

| Command | Arguments |
|---|---|
| `osc.setDestination` | `host`, `port`, `enabled`. Replaces the destination list with one entry. |
| `osc.test` | Sends `/xen/ping` with `s "xenakis"` and `s` version. |
| `osc.panic` | Stops the cursor position stream. |

`prefix` (default `/xen`), `streamRateHz` (default 30), `useBundles` (default true), and `latencyMs` (default 10) change through `SetProperty`.

## Messages

| When | Address | Arguments |
|---|---|---|
| Trigger | `/xen/trigger/<id>` | `s cursor, s path, i dir, i pass, f u, f x, f y, d time` |
| Trigger override | `oscAddress` | `oscArgs` tokens: `$cursor $path $dir $pass $u $x $y $t`, or typed literals |
| Cursor stream | `/xen/cursor/<id>` | `s path, f u, f x, f y, i pass, d time` |
| Cursor edge | `/xen/cursor/<id>/start`, `/pass`, `/stop` | `/pass` adds `i pass`. Each ends with `d time` |
| Transport | `/xen/transport` | `s state, d seconds, f beats, f bpm` |
| Beat | `/xen/transport/beat` | `i bar, i beat` |
| Test | `/xen/ping` | `s "xenakis", s version` |

Bundles are timetagged at the event's transport time plus `latencyMs`.

## Saved schema (`xen.osc` version 1)

```json
"xen.osc": {
  "OscSettings": {
    "destinations": [{ "id": "main", "host": "127.0.0.1", "port": 9001, "enabled": true }],
    "prefix": "/xen",
    "streamRateHz": 30,
    "useBundles": true,
    "latencyMs": 10
  }
}
```

The singleton entity is `osc`.
