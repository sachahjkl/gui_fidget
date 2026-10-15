# Worked example: gui_todo (a "rich" app on the kit)

Source: `assets/todo/` (`src/main.c` ≈ 1,200 lines, builds to ~350 KB with the extracted kit).
Everything below was **verified with real input** (typed text, mouse clicks, double-click, right-click,
theme switch, search) and screenshots on a 175 % monitor; the same code is what the user runs at 150 %.

Contents: feature → pattern map · text fields (the hard part) · focus model rules · layout pitfalls ·
colors/alpha · DPI & window size · persistence · testing recipe

## Feature → pattern map

| Feature | How it is done (search `main.c` for the symbol) |
|---|---|
| Task rows with hover actions | keyed row box `###task_<id>`; hover from **last frame's rect**: `contains_2f32(ui_box_from_key(row_key)->rect, ui_mouse())` → `ui_anim(...)` → icon `transparency`; row overlay via `ui_box_equip_custom_draw(row, app_row_draw, data)` |
| Animated checkbox | custom draw box; `done_t = ui_anim(key, done?1:0)` scales a filled rounded rect + icon-font check glyph |
| Priority badge | tag per level (`prio_high text` in theme) via `ui_push_tag`; border tinted with `ui_set_next_border_color` |
| Strike-through done tasks | `DR_FStrList` with `.strikethrough_thickness`, `ui_box_equip_display_fstrs(box, &fstrs)` |
| Filter tabs with animated underline | custom draw using `ui_anim` width; selected tab = no `weak` tag, others pushed `weak` |
| Progress bar | `ui_anim` on done/total; custom draw (track + fill, radius = h/2) |
| Scrollable list | box with `Clip|ViewScrollY|ViewClampY|AllowOverflowY`, children = fixed-height rows, `ui_signal_from_box(list)` **after** children |
| Context menu | one shared `UI_CtxMenu(key)`; `ui_ctx_menu_open(key, row->key, mouse - row->rect.p0)` on right-click; app stores `ctx_task_id` |
| Tooltips | `if(ui_hovering(sig)) UI_Tooltip { ui_label(...); }` |
| Theme cross-fade | two `UI_Theme`s; pass `&app.themes[light]` to `ui_begin_build` each frame – cached colors ease toward the new target automatically |
| Live search | `str8_find_needle(title, 0, query, StringMatchFlag_CaseInsensitive)` on every frame (cheap) |
| Persistence | TSV via `write_data_to_file_path` on every mutation (`app.dirty` flag), `user_program_config_data_path` |

## Text fields (ui_line_edit) — what the engine does NOT do for you

`ui_line_edit(&cursor,&mark,buf,cap,&size,pre_edit_value,"Placeholder###id")` only *consumes* `UI_Event`s.
The host app must create them (see `app_ui_events_from_wm_events`):

```c
// printable chars: WM_EventKind_Text -> UI_EventKind_Text      (skip '\n': Enter arrives as a Text '\n'!)
// Enter  -> Press event with slot = UI_EventActionSlot_Accept  (commits: ui_committed(sig))
// Esc    -> Press event with slot = UI_EventActionSlot_Cancel  (default-nav root unfocuses the field)
// Left/Right  -> Navigate, unit Char (Ctrl: Word), delta (±1,0),
//                flags = shift ? KeepMark|ExplicitDirectional : PickSelectSide|ZeroDeltaOnSelect|ExplicitDirectional
// Home/End    -> Navigate, unit Line (Shift: KeepMark)
// Backspace/Delete -> Edit, flags Delete|ZeroDeltaOnSelect, unit Char/Word, delta ∓1/±1
// Ctrl+A -> Navigate Whole -1 then Navigate Whole +1 with KeepMark ; Ctrl+C -> Edit Copy|KeepMark
// Ctrl+X -> Edit Copy|Delete ; Ctrl+V -> Text event with UI_EventFlag_Paste and the clipboard string
```
Rules learned the hard way:
1. **The line edit sizes itself from the style stacks.** Wrap the call: `UI_PrefWidth(ui_pct(1,0)) UI_PrefHeight(ui_pct(1,0)) ui_line_edit(...)`;
   otherwise you get the default 300×40 px box that ignores your layout.
2. **Single-click focus**: the widget only enters edit mode on double-click or keyboard activation. For normal fields:
   `if(!was_active && ui_pressed(sig)) { ui_set_auto_focus_active_key(key); cursor = mark = size; }` where
   `key = ui_key_from_string(ui_active_seed_key(), id_only)` is computed *at the same parent* as the call
   (`"Label###id"` hashes only `"###id"`).
3. **The box that contains the line edit needs `UI_BoxFlag_FocusActive`** (build it with `ui_set_next_focus_active(UI_FocusKind_On)`):
   the cursor's alpha is multiplied by `box->parent->parent->focus_active_t`; without it the cursor is invisible.
4. **Placeholder** = the display part of the id string, shown when not editing and `pre_edit_value` is empty. Pass
   `str8(buf,size)` as `pre_edit_value` so typed text stays visible after the field loses focus.
5. After a commit, re-focus explicitly (`ui_set_auto_focus_active_key(key)` runs *after* the widget cleared it) for rapid entry.
6. Inline rename: set `edit_id`, copy title → edit buffer, `cursor = size; mark = 0` (select all), and focus the editor's key
   **in the same frame** (`ui_key_from_string(row->key, "###title_edit")`); render the editor when `edit_id == task.id`;
   cancel when the key is no longer `ui_is_key_auto_focus_active` after a couple of frames.
7. Keep edit buffers in *your* structs (`TextField {U8 buf[256]; U64 size, cursor, mark;}`), not in the UI.

## Focus model rules (the failure modes are silent)

- Mark **only the page/root box** as the navigation root: `ui_set_next_focus_hot(On); ui_set_next_focus_active(On);` then build it
  with `UI_BoxFlag_DefaultFocusNav`. Do **not** wrap the whole UI in `UI_Focus(UI_FocusKind_On)`: every box would be
  `FocusHot` + `KeyboardClickable`, and a single Enter press would "click" the first button in the tree.
- `ui_set_auto_focus_active_key` works anywhere below that root (it walks parents looking for `DefaultFocusNav`).
- Focus rings are *your* draw code: draw one for `b->key == nav_root->default_nav_focus_hot_key` and for boxes with
  `focus_active_t > 0.01`; the engine only maintains the state.
- The new key takes effect on the *next* `ui_begin_build` ("next" keys become current there), so a widget that must be
  active this frame can't be; plan for a one-frame delay or set it one frame earlier.

## Layout pitfalls (each one produced a wrong screenshot)

- **`ui_em()` inside a `UI_FontSize(...)` scope uses the scoped font size.** Capture `F32 em = ui_top_font_size();` first and
  use `ui_px(em*3.6f, 1)` inside smaller-font scopes (the badge pill collapsed to a sliver before this).
- **Cross-axis alignment doesn't exist**: children are placed at offset 0 on the cross axis. Center vertically with a
  column + `UI_Padding(ui_pct(1,0))` around the child (`AppVCenter` macro in the example).
- **`ui_pct(1,0)` is relative to the parent's full size, not the remaining space.** A list with `pct(1,0)` height after fixed-px
  header rows simply gets *shrunk* by strictness 0 to fit — that is the intended trick for "fill the rest".
- **A strictness-0 label next to a `ui_spacer(ui_pct(1,0))` gets crushed** (both shrink proportionally to size). Make the label
  itself `ui_pct(1,0)` (and drop the spacer) when you want "label takes what the buttons leave".
- Long text: leave strictness < 1 on text boxes so overflow truncates with `...` instead of pushing siblings out of the window.

## Colors and alpha

All theme colors are stored **linear** (`linear_from_srgba`) and blending happens in linear space, so small alphas are far brighter
than they look in sRGB: white at 0.05 over `#1c1f26` rendered as `#46474a`. Use ~0.010–0.015 for hover/pressed overlays
(raddbg uses 0.015, 0.01). Use `"floating background/text/border"`-style tag patterns for menus/tooltips (the engine pushes
the tag `floating` inside them). A missing pattern silently yields transparent black.

## DPI and initial window size

`wm_window_open(..., WM_WindowFlag_UseDefaultPosition, ...)` lets Windows choose the monitor, so sizing the window from
`wm_dpi_from_monitor(wm_primary_monitor())` is wrong on multi-monitor setups (the window opened at 96 dpi size while fonts were
scaled for 144 dpi: content clipped). Do it *after* creation:
```c
F32 dpi = wm_dpi_from_window(window);                      // the monitor it actually opened on
Vec2F32 work = wm_dim_from_monitor(wm_monitor_from_window(window));
SetWindowPos(w32_wm_window_from_handle(window)->hwnd, 0, 0, 0, (int)Min(w*dpi/96, work.x*0.92f), (int)Min(h*dpi/96, work.y*0.92f),
             SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);       // see app_fit_window_to_dpi
```
and scale fonts per frame with `wm_dpi_from_window(window)/96` (`ui_push_font_size(floor(15*scale))`); `WM_DPICHANGED` is already
handled by the window manager when the window moves between monitors.

## Testing recipe that worked (Windows, no extra tools)

- Put state in the window title (`wm_window_set_title`) so a script can read `Process.MainWindowTitle` — more reliable than OCR.
- Typing: `PostMessage(hwnd, WM_CHAR=0x102, char, 1)` and `WM_KEYDOWN/KEYUP` (0x100/0x101) go straight to the window; no global input.
- Mouse: needs a real cursor (`GetCursorPos` is polled). `SetProcessDPIAware()` in the script, `SetForegroundWindow`, verify
  `GetAncestor(WindowFromPoint(p), GA_ROOT) == hwnd` before every click, restore the cursor in `finally`, tell the user not to move.
- Screenshots: `Graphics.CopyFromScreen(windowRect)` after `SetProcessDPIAware()`. `PrintWindow` hung on this D3D flip-model window.
- **Always run a resize test**: `SetWindowPos(hwnd, 0, 0, 0, w, h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)` through
  grow/shrink steps, then ~60 rapid random sizes with 15 ms sleeps (simulates dragging an edge), and check
  `Process.HasExited` / `MainWindowTitle == "Fatal Exception"`. It caught the re-entrant `frame()` crash (see
  "frame() is re-entered during live window resize" in `SKILL.md`) that normal clicking never triggers.
- Don't name PowerShell helper functions `Move`/`Type`/`Copy` — aliases (`Move-Item`, `Get-Content`) win over functions.
- Reset the app's saved data after tests (`%APPDATA%\gui_todo\todos.tsv`).

