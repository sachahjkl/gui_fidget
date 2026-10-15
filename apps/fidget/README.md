# gui_fidget

A widget playground on the RAD Debugger UI stack. `run.bat` (here or at the repo root) builds and launches it.

- **Command palette** (Ctrl+K / Ctrl+Shift+P): fuzzy search with highlighted matches, keyboard and mouse
- **Menu bar** (File / View / Theme / Help), context menu, dropdowns
- **Controls**: 4 button kinds, icon buttons, switches, checkboxes, radios, segmented control, chips, sliders, range slider,
  stepper (hold to repeat, Shift = x10), rating, progress bars, rich tooltips, toasts, modal dialog
- **Inputs**: validated text fields, live fuzzy search, tag input, small form
- **Data**: 20,000-row virtualized table (sort, resize columns, filter, keyboard navigation) and a file tree
- **Playground**: physics balls, confetti, fidget spinner, live accent color picker, draggable sticky notes
- 6 cross-fading themes (Midnight, Graphite, Daylight, Nord, Dracula, Amber), dark title bar to match
- View menu: animations (**follow the OS setting by default**, or always on / always off), slow motion, UI box outlines, status bar
- Settings (animation mode, theme, status bar) are saved to `%APPDATA%\gui_fidget\settings.txt` (plain `key=value` lines)

Shortcuts: Ctrl+K palette, Ctrl+1..4 pages, Ctrl+T next theme, Ctrl+B status bar, Ctrl+J animations on/off.

## Source layout

    src/main.c          includes, OS->UI events, draw walk, frame loop, entry point
    src/fw_app.h        types, command table (X-macro), app state
    src/fw_theme.c      palettes -> flat tag/color theme
    src/fw_settings.c   persisted settings + OS animation preference
    src/fw_widgets.c    widget library
    src/fw_physics.c    balls + confetti
    src/fw_commands.c   toasts, command dispatch, command palette, modal
    src/fw_pages.c      menu bar, status bar, shell, the 4 pages

Test flags: `--page=N --theme=N --palette --palette_query=txt --modal --menu=N --toast --title_state --anim=system|on|off --cmd=N`.
