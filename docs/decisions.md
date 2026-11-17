# Locked decisions

These answer the open questions in the first-release plan. Later milestones follow them.

1. **Language and libraries.** C++20, JUCE 8 (`juce_osc` for OSC), CMake with CPM, Catch2. Native standalone. Windows is the first machine that has to be right. CI builds Windows, macOS, and Linux.
2. **Grid mode.** Time is arc length along the path. One grid unit is one beat in BPM mode and one second in absolute mode.
3. **Absolute mode.** `ms`, `s`, and `min` are wall-clock. `beats` and `bars` follow tempo.
4. **Cursors.** An armed cursor is slaved to the transport: `cursorTime = transportTime − delay`. There is no per-cursor play in the MVP.
5. **OSC.** UDP, more than one destination, the `/xen/...` addresses from plan section 6, coordinates in grid units, cursor position stream off unless that cursor asks for it, timetagged bundles.
6. **Projects.** `.xenproj` JSON save and load is in scope. The format id is `xenproj/1`.
7. **Paths.** 2D polylines and cubic Béziers only.
8. **Clock.** Internal clock only. No Ableton Link, MIDI clock, or OSC transport in.
9. **Groups.** Containment, `visible`, and `mute`. A muted group stays in the snapshot. Later OSC drops its events.

JUCE 8 is AGPLv3, or a commercial JUCE licence. This repo fetches JUCE at configure time and does not vendor it. M0/M1 do not ship an installer. The ASIO SDK is not linked, because playback uses the internal control clock.
