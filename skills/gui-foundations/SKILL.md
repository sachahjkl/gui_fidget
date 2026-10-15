---
name: gui-foundations
description: Design principles, code patterns and a verified starter kit extracted from the RAD Debugger codebase (EpicGamesExt/raddebugger) for building fast-starting, pragmatic native C programs with a GPU immediate-mode UI. Use this whenever the user wants to write C in the "RAD / Ryan Fleury" style (arena allocators, unity build, layered codebase, String8, metagen tables), reuse or repackage raddebugger's base/ui/draw/render/font layers in their own app, build a native desktop tool with a retained-state immediate-mode UI (keyed boxes, size-kind layout, animated hot/active state, tag-based themes), make a UI app start instantly and idle at zero CPU, or asks how raddebugger does X (frame loop, async caches, SDF rect shader, custom title bar). Trigger even if they don't name raddebugger but say things like "make my own app with that debugger's UI", "arena-based C project", "unity build setup", or "fast native UI like raddbg".
---

# GUI Foundations

Distilled from reading the RAD Debugger source (github.com/EpicGamesExt/raddebugger, MIT, Epic Games Tools) and from
compiling + running a minimal app against its layers. The goal: let you reuse the
*fundamentals* (base layer, UI core, draw/render/font stack, async cache, build setup)
in other programs with the same qualities: tiny exe, instant start, zero idle CPU,
smooth animated UI, code that is flat and greppable.

## When to read what

| Need | Read |
|---|---|
| Start a new app right now | `assets/minimal/` + "Quick start" below |
| See a full "rich" app (text input, inline edit, ctx menu, themes, persistence, DPI) | `assets/todo/` + `references/worked-example-todo.md` — read the latter before writing any text field or focus logic |
| A large widget library (switch, slider, dropdown, tabs, command palette, toasts, modal, virtual table, color picker, physics) | `assets/fidget/` (7 files, ~3,600 lines) + `references/widget-cookbook.md` |
| Copy the right files out of the repo | `scripts/extract_kit.ps1`, `references/extraction-guide.md` |
| Memory, strings, lists, macros, naming, style | `references/base-layer.md` |
| How the UI works (keys, stacks, layout, signals, animation, themes) | `references/ui-system.md` |
| Window → events → frame loop → draw → GPU, fonts, startup/idle behaviour | `references/rendering-and-frame-loop.md` |
| Layering, unity build, build script, metagen, async artifact cache | `references/architecture-and-build.md` |

Always check the *actual* source in the user's raddebugger checkout when details matter;
it moves fast (the last pull changed 355 files). Paths below are relative to `src/`.

## The principles (and why they pay off)

1. **One translation unit per executable (unity build).** `raddbg_main.c` includes every
   layer's `.h` (pass 1) then every `.c` (pass 2). No linker maze, whole-program
   inlining, no header guards fighting, a full debug build in seconds, and "build system"
   = one `cl`/`clang` line (`build.bat`/`build.sh`). Order of includes = the dependency DAG.
2. **Layers = folders = namespaces.** `ui/` → `UI_`/`ui_`, `draw/` → `DR_`/`dr_`,
   `render/` → `R_`/`r_`. Types/enums/macros are CAPS-prefixed, functions/globals
   lowercase. Layers form a DAG; `lib_*` layers depend on nothing and can be lifted out.
   Platform/back-end choice happens in `x_inc.c` with `#if`, never with function pointers
   or virtual dispatch (`render_inc.c` picks D3D11/OpenGL/stub; `window_manager_inc.c`
   picks win32/linux/stub). Stubs make headless builds of the *same* code trivial.
3. **Arenas, not malloc.** Everything is `push_array(arena, T, n)`. Lifetimes are
   grouped: permanent arena per layer, double-buffered *per-frame* arenas, and
   thread-local **scratch** arenas (`scratch_begin/scratch_end`) for temporaries. You
   almost never free a single object. See `references/base-layer.md`.
4. **Plain data + nil sentinels + zero is valid.** Structs are POD; "no node" is a
   read-only global `*_nil_*` whose links point to itself, so tree walks need no NULL
   checks (`ui_nil_box`, `cfg_nil_node`). Handles are `struct{U64 u64[1];}` (`WM_Window`,
   `R_Handle`), never raw pointers across layers.
5. **Strings are `String8 {U8 *str; U64 size}`.** No NUL dependence, substrings are free,
   `s("lit")`, `str8f(arena, fmt, ...)`, `%S` for String8 in format strings.
6. **Intrusive list macros + hash-table-of-chains + free lists.** `DLLPushBack`,
   `SLLStackPush`, … Caches prune by `last_touched_build_index`, not by timers/GC.
7. **Scope macros with `DeferLoop`.** `UI_Parent(box) UI_PrefWidth(ui_pct(1,0)) { … }`
   pushes/pops stack values; the same trick wraps mutexes, tooltips, draw buckets.
8. **Option structs via designated-initializer macros.** `arena_alloc(.reserve_size = GB(1))`,
   `ui_anim(key, 1.f, .rate = r)`, `ac_artifact_from_key(..., .gen = g, .flags = AC_Flag_Wide)`.
   Adding a parameter never breaks call sites.
9. **Metaprogram instead of macros-for-tables.** `.mdesk` files describe tables;
   `metagen` emits enums, data arrays, stack push/pop/top functions into `generated/`
   (checked in, so consumers don't need to run it).
10. **Immediate-mode UI with persistent keyed state.** You rebuild the whole box tree
   every frame from plain code, but each box is found again by a 64-bit key hash, so
   hover/active/scroll/animation state survives. Layout runs once at `ui_end_build`;
   widgets read *last frame's* rect to decide interaction (so the UI settles in a few
   frames). Details: `references/ui-system.md`.
11. **Never block the UI thread.** Expensive results come from an asynchronous artifact
   cache keyed by bytes + a generation counter (`ac_artifact_from_key`); the UI asks with
   a zero deadline and renders stale/empty data until the async lanes finish. Access
   scopes keep artifacts alive while used. See `architecture-and-build.md`.
12. **One shader draws the whole UI.** Instanced quads with per-corner color, per-corner
   radius, border thickness and edge softness evaluated as a signed-distance field;
   text is the same quad batch sampling a glyph atlas. Thousands of boxes = a few draw
   calls. See `rendering-and-frame-loop.md`.
13. **Idle means asleep.** The frame loop blocks in `GetMessageW` unless
   `num_frames_requested > 0` (set to 4 on any input/animation). Animation state tells
   the loop when it may sleep again (`ui_animating_from_state`).
14. **Start fast by doing less before first paint.** Hidden window until frame 0 is
   rendered (`wm_window_first_paint`), fonts/atlases lazy, heavy loads async, no DLL
   dependencies beyond the OS, embedded assets via metagen `@embed_file`.

## Code style in one screen

```c
////////////////////////////////
//~ rjf: Section Banner (layer-level, appears in .h and .c alike)

//- rjf: sub-section (inside a function or a section)
internal UI_Signal                 // `internal` = static; return type on its own line
ui_button(String8 string)          // function name starts at column 0 -> `grep ^ui_button`
{
  UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText, string);
  UI_Signal sig = ui_signal_from_box(box);
  return sig;
}
```
- Allman braces, 2-space indent, `for EachIndex(i, n)`, `for EachNode(n, T, first)`.
- No `bool`/`const`-heavy APIs: `B32`, `U64`, `F32`, `S64`, `Vec2F32`, `Rng2F32`.
- `internal` / `global` / `local_persist` / `thread_static` / `read_only` replace `static`.
- Comments are sparse and explain *intent* ("rjf: no child has the active focus -> do
  navigation at this layer"), in `//- ` step markers, often one per block of a long function.
- Long functions are fine when they are linear and sectioned; no deep abstraction trees.
- Keep the copyright header (`// Copyright (c) Epic Games Tools` / MIT) on copied files.

## Quick start (verified on Windows x64, MSVC 2022, this machine)

1. Extract the kit: `powershell -File scripts/extract_kit.ps1 -Upstream <path-to>\raddebugger\src -Dest <proj>\kit`
   (copies the layers the UI stack needs, ~158 files / 2.2 MB incl. Linux backends; about 80 of them
   are actually compiled on Windows; see `references/extraction-guide.md`).
2. Copy `assets/minimal/src/main.c` and `build.bat` into your project; `set KIT=<proj>\kit`
   (default: `.\kit` next to the script, or `..\..\kit` inside the gui_fidget repo).
3. From a "x64 Native Tools" prompt (or `vcvarsall.bat x64`): `build.bat` → `build\gui_minimal.exe`
   (~300 KB). Run it: dark themed window with labels/buttons, tooltips, animated panel.
4. Replace `app_build_ui()` with your own UI. Everything else in `main.c` is boilerplate
   you keep: event conversion, theme, draw walk, frame pacing.

## Hard-won gotchas (each one cost a failed build or crash while validating this skill)

- `src/scratch/textperf.c` (the repo's "minimal graphical program") is **stale** and does
  not compile: it lacks `window_manager_inc.h`. Use `assets/minimal` instead.
- Include order: `base` → `window_manager` → `shell` → `font_provider` → `render` →
  `font_cache` → `draw` → `ui`. `render`/`draw` use `WM_Window`; render's error path
  calls `sh_message` (link error `sh_message` unresolved ⇒ forgot `shell`).
- `#define BUILD_TITLE "..."` and `#define OS_FEATURE_GRAPHICAL 1` **before** includes;
  define `internal B32 frame(void)` and `internal void entry_point(CmdLine *)` yourself.
  `main_thread_base_entry_point` auto-inits every layer present (`wm_init`, `fp_init`,
  `r_init`, `fnt_init`, …) unless you `#define X_INIT_MANUAL 1`.
- **Call `dr_begin_frame(icon_font)` before `ui_begin_build`.** The UI build calls
  `dr_fruns_from_fstrs`, which reads `dr_thread_ctx`; skipping it crashes with the
  "Fatal Exception" dialog (null deref), not a compile error.
- MSVC needs `/Zc:preprocessor` (variadic/designated-init macros), `/I<src>`; define
  `BUILD_DEBUG=0/1`. Linux uses clang/gcc with `-mcx16` (see `build.sh`).
- `UI_State` is `thread_static` — call `ui_select_state(state)` at the top of any frame
  that touches the UI (one state per window).
- The `ui_color_from_name` lookup returns **transparent black** when no theme pattern
  matches; if your widgets are invisible, your theme is missing `background`/`text`/`border`.
- Mouse position comes from `GetCursorPos` each frame, not from event payloads; test
  automation must move the real cursor (and will hijack it — warn the user first).
- **Overlay alphas must be tiny (0.01–0.02):** colors are linear and blended in linear space, so
  hover tint at 0.05–0.10 looks like a big grey wash.
- **`ui_em()` inside `UI_FontSize(...)` uses the scoped size**; capture `em` outside for sizes that
  shouldn't shrink with a smaller font.
- **`ui_line_edit` takes its size from the style stacks** (default 300×40 px): wrap it in
  `UI_PrefWidth(ui_pct(1,0)) UI_PrefHeight(ui_pct(1,0))`. It also consumes only `UI_Event`s — you must
  synthesize Navigate/Edit/Text/Accept/Cancel events yourself (recipe in the worked example).
- **Don't wrap the whole UI in `UI_Focus(UI_FocusKind_On)`**: set it (`ui_set_next_focus_hot/active`) only on the
  navigation-root box, or every box becomes keyboard-clickable and Enter clicks the first button.
- **Size the window after creating it**, from `wm_dpi_from_window` (CW_USEDEFAULT may open on a non-primary
  monitor with a different DPI than `wm_primary_monitor()`); scale font sizes by `dpi/96` per frame.
- Enter arrives as a `WM_EventKind_Text` with `'\n'`; filter it out before forwarding to the UI.
- **`frame()` is re-entered during live window resize → crash if you ignore it.** The Win32 window procedure
  calls `update()` on `WM_SIZE`/`WM_PAINT`, i.e. from *inside* `wm_get_events()` of an outer `frame()`
  (Windows runs a modal size loop). `wm_get_events` owns global event-list state, so a nested call that
  pumps events corrupts the outer one (symptom: "Fatal Exception" dialog when you enlarge the window).
  Keep a `frame_depth` counter; only `frame_depth == 1` calls `wm_get_events`; nested calls just
  build + draw + present (raddbg: `rd_state->frame_depth`). Both templates have this; any hand-rolled
  `frame()` must too. Test with a resize burst (`worked-example-todo.md`, "Testing recipe").

## Verification status (be honest with the user about this)

Verified here (raddebugger commit 8036f7be, MSVC 2022 Community, Windows x64, Release):
`scripts/extract_kit.ps1` into an empty dir (158 files, 2.2 MB) → template built against that copy
only → ~300 KB exe → process starts, window becomes visible ~0.4–0.6 s after launch (includes process
spawn + a polling script), renders the themed UI correctly (screenshot checked), stays responsive at
~56–62 MB working set, hover detection registers (`ui_hot_key` non-zero in the title hook).
Also verified with **real input** in `assets/todo` (Windows, 175 % monitor; user runs it at
150 %): typing + Enter, mouse clicks, double-click inline rename, right-click context menu, tooltips,
animated light/dark theme, live search filtering, hover animations, auto-save/load, DPI-based sizing.
`assets/fidget` also verified with real input (clicks, drags, wheel scroll on `ViewScrollY`, keyboard
navigation of a 20,000-row virtualized table, palette, menus, dropdowns, spinner flick), a resize burst, ~0.3 s to
visible window, 0 % idle CPU on pages without continuous animation. Test scripts: `scripts/windows-ui-testing/`.
**Not verified:** `ui_scroll_list`, file drag & drop, custom title bar
(`WM_WindowFlag_CustomBorder`), Linux (OpenGL/FreeType/X11) backends, `artifact_cache` in a
standalone app. For those, read the matching raddbg code (`raddbg_widgets.c`, `raddbg_views.c`) and test.

## Licensing

RAD Debugger is MIT licensed (Epic Games Tools). Keep the license text and file headers
when copying `src/` files; `third_party/*` carry their own licenses.
