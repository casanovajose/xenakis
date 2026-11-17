# M5 acceptance

`Xenakis.exe` builds with MSVC 19.16 (Visual Studio 2017 Build Tools) and JUCE 8.0.17. MinGW is not a JUCE host; the headless player stays on Clang.

The window is the frame: tool buttons, transport bar, layers, time-grid canvas, inspector, and OSC monitor. Keyboard: Space plays or pauses, Enter stops or finishes a path, V/P/C/T select tools, Delete removes the selection, Ctrl+Z/Y undo and redo, Ctrl+S saves.

Drawing a path, adding a cursor and triggers, and pressing play call the same commands the headless tests already check. Save and open use the `.xenproj` reader. A reload of that score still crosses a trigger at u 0.5 forward and then back. The OSC monitor lists `osc.sent` lines. This session did not click through the window with a mouse; the executable links and the command sequence underneath it was tested.
