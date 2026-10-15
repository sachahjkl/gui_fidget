# Widget cookbook (from `assets/fidget`)

Every pattern below exists, compiled and tested, in `assets/fidget/src/`. Read the code, this file lists
the traps.

## Widget conventions
- Each widget takes a stable `id` string and builds its root with `"###id"`. Hover/press/animation state is keyed by it
  and survives rebuilds. Read last frame's state with `ui_box_from_key(fw_key(id))` (`hot_t`, `active_t`).
- State lives in caller variables (`B32 *v`, `F32 *v`, `TextField`), never in the UI.
- Smooth values: `ui_anim(ui_key_from_string(key, s("t")), target, .rate = ..., .initial = ...)`.
- Custom visuals: `ui_box_equip_custom_draw(box, fn, data)`, with `data` pushed on `ui_build_arena()`
  (valid until the draw walk finishes). Resolve colors at build time with `ui_color_from_name`, not inside the draw callback.
- Press feedback: `ui_set_next_squish(prev->active_t * 0.045f)` plus the draw-walk transform (see `app_draw_ui` in `main.c`).

## Traps (each one cost time)
1. **`UI_FixedX/Y/Width/Height` are stacks.** Used as a scope they apply to *every child* built inside. For a floating
   container use `ui_set_next_fixed_*` (applies to the next box only).
2. **Theme tie-breaking.** A pattern matches when its last tag is the queried name and its other tags are in the stack. Most
   matching tags wins, ties go to the FIRST pattern in the list. A button inside a card has `[card, accent_button]`, so
   `card background` and `accent_button background` tie. List variants before containers.
3. **Overlays are built first** (toasts, palette, modal, then the page) so they draw on top and see clicks first. The scrim
   box is built *after* the panel (the panel keeps its own clicks, everything else lands on the scrim).
4. **`pct(1,0)` inside a `children_sum` parent** resolves against the nearest sized ancestor, so a separator in a menu
   stretches it to window width. Use an explicit px width (`fw_hline_px`).
5. **Menus share one context-menu root.** Dropdown, menu bar, right-click menu all use `ui_ctx_menu_open/UI_CtxMenu`.
   Hover-to-switch between menu bar entries = call `ui_ctx_menu_open` for the hovered one while another is open.
6. **Text fields**: container must carry `ui_set_next_focus_active(UI_FocusKind_On)`; focus a field programmatically with
   `ui_set_auto_focus_active_key(key)` (a `focus_request` pointer consumed next to the field is a clean pattern).
   Detect change by hashing the buffer before/after.
7. **Vertically centering** a fixed-height child: column with `Padding(pct(1,0))` before and after (`FwVCenter`).
8. **Kbd chips / badges**: size from measured text (`fnt_dim_from_tag_size_string`) + padding. `ui_text_dim` inside a
   fixed-width parent clips; give the parent the measured px width.
9. **Virtualized list/table**: scroll box with `ViewScrollY|ViewClampY|AllowOverflowY`, a top spacer of `first*row_h`, only
   rows intersecting the viewport, a bottom spacer for the rest. Take the viewport height from last frame's rect, the scroll
   offset from `view_off`. 20,000 rows = ~150 boxes. Keyboard: move the selection, then adjust `view_off_target.y`.
10. **Wheel scrolling**: call `ui_signal_from_box(scroll_box)` AFTER building the children; it consumes the events and clamps.
11. **Drag with state**: `ui_pressed` then `ui_store_drag_struct(&start)`, `ui_dragging` then `*ui_get_drag_struct(T)` +
    `ui_drag_delta()`. Used for column resize, range slider (which thumb), sticky notes, spinner.
12. **Toasts/animations need frames**: set `frames_requested = 4` whenever something time-based is alive, otherwise the
    loop sleeps and the animation freezes.
13. **Dark title bar**: `DwmSetWindowAttribute(hwnd, 20, &BOOL, 4)` when the palette changes.
14. **Scale everything from `em = ui_top_font_size()`**; sizes stored in em (column widths) survive DPI changes.

## Testing on Windows without a human
`scripts/windows-ui-testing/`: `fshot.ps1` (launch with flags, screenshot with a guard that aborts if another window covers
the app), `fw_test.ps1` (clicks, wheel, drag, keys; reads app state back from the window title via `--title_state`),
`resize_test.ps1` (resize burst).
- PowerShell 5 has no `?:` ternary. Pass numbers as `[int]` parameters (string `"-5"` compares alphabetically).
- A screenshot you `Read` may be stale: delete it before regenerating.
- Always warn the user before real-input tests; restore the cursor position afterwards.
