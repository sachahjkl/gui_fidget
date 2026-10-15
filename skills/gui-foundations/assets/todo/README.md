# gui_todo

A todo app on the RAD Debugger UI stack. `run.bat` (here or `run.bat todo` at the repo root) builds and launches it.

- Add (Enter), toggle (animated checkbox), delete (trash / context menu), inline rename (double-click or pencil)
- Priorities: click the LOW / MED / HIGH badge to cycle; optional sort by priority
- Filters (All / Active / Done) with live counts, live search (Ctrl+F)
- Progress bar, hover effects, tooltips, right-click context menu
- Light / dark theme with cross-fade (palette button or Ctrl+T)
- Keyboard: Ctrl+N new task, Esc cancel, Tab / Enter navigation, text editing (arrows, Home/End, Ctrl+arrows, Ctrl+A/C/X/V)
- Auto-saved to `%APPDATA%\gui_todo\todos.tsv` (plain TSV; theme + sort mode in the header line)
- DPI aware (per monitor): window size and fonts scale with the monitor the window opens on

The whole app is `src/main.c` (~1,200 lines).
