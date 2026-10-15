# Extracting and repackaging the kit

Contents: what the UI kit consists of · extraction script · include order · what to add for each
extra capability · what to NOT copy · modifying generated code · Linux notes · upgrade strategy

## The UI kit = exactly these layers (measured with `cl /showIncludes` on the template)

| Layer (dir under `src/`) | Why |
|---|---|
| `base/` (+`third_party/stb`, `radsort`, `martins_hash`, `martins_memfun`, `xxHash`) | arenas, strings, threads, entry point, math, hashing |
| `win32/base`, `win32/shell`, `win32/window_manager` (or `linux/...`) | OS backends |
| `window_manager/` (+`generated/`) | windows, events, clipboard, cursors, DPI |
| `shell/` | modal message boxes (used by render/arena failure paths) |
| `font_provider/` (`dwrite/` on Windows; `freetype/` on Linux) | glyph rasterization |
| `render/` (`d3d11/`, `opengl/`, `stub/`, `generated/`) | GPU abstraction + backends |
| `font_cache/` | glyph atlas + shaped-run cache |
| `draw/` (+`generated/`) | draw buckets, fancy strings |
| `ui/` (+`generated/`) | the immediate-mode UI |
| `lib_raddbg_markup/raddbg_markup.h` | header-only; `base_markup` includes it (debugger annotations, harmless) |

Total: ~80 files compiled (template compiles to ~300 KB exe). `scripts/extract_kit.ps1` copies the whole
directories (so you also get the Linux/OpenGL/stub backends and generated code) — about 1–2 MB of source.

```powershell
powershell -File scripts/extract_kit.ps1 -Upstream <path-to>\raddebugger\src -Dest C:\proj\kit
# then in your build script:  /IC:\proj\kit   and  #include "base/base_inc.h"  …
```
Keep the *relative directory structure* — includes are written relative to `src/` (`"base/base_inc.h"`,
`"win32/base/win32_base.h"`, `"third_party/stb/stb_sprintf.h"`).

## Mandatory include order for a graphical app

```c
#define BUILD_TITLE "gui_minimal"
#define OS_FEATURE_GRAPHICAL 1
#include "base/base_inc.h"
#include "window_manager/window_manager_inc.h"
#include "shell/shell_inc.h"
#include "font_provider/font_provider_inc.h"
#include "render/render_inc.h"
#include "font_cache/font_cache.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"
/* then the same list with .c: base_inc.c, window_manager_inc.c, shell_inc.c, font_provider_inc.c,
   render_inc.c, font_cache.c, draw.c, ui_inc.c */
```
Headless tool (no graphics): just `base/base_inc.h/.c` and omit `OS_FEATURE_GRAPHICAL`.

## Add capabilities à la carte

| Capability | Add (after base unless noted) | Notes |
|---|---|---|
| Async cache + worker lanes | `artifact_cache/artifact_cache.h/.c` | header only needs base types; `ac_init` + `ac_async_tick` auto-wired by presence (not compile-tested in the kit) |
| Content-addressed blob store | `content/` | 128-bit hash keyed store used by file_stream/text |
| Async file streaming + hot reload | `content/`, `file_stream/` (needs `artifact_cache`) | `fs_*` APIs |
| Text lexing/layout for code views | `text/`, `mutable_text/` | depends on content + artifact_cache |
| Config tree / Metadesk files | `mdesk/`, `config/` | `.mdesk` also drives metagen |
| TCP/UDP + HTTP | `socket/`, `http/` (+ `win32/socket`, `win32/http`) | `*_stub` variants exist |
| Command-line parsing | already in base (`CmdLine`, `cmd_line_has_flag`, `cmd_line_string`) | |
| Tests | `base_test.h` (`BUILD_TESTS=1`) | see `src/*/tests/` |
| Custom title bar | window flags `WM_WindowFlag_CustomBorder` + `wm_window_push_custom_title_bar*` | study `raddbg_core.c` menu bar section |
| 3D viewport | `R_PassParams_Geo3D` via `dr_geo3d_begin` + `dr_mesh` | see `raddbg_views.c` geo3d view |

Do **not** copy: `dbg_engine`, `demon`, `ctrl`, `eval*`, `rdi*`, `linker/`, `pdb/coff/pe/elf/dwarf/codeview` unless you want
those features; they are the debugger, not the UI foundation. `raddbg/` (app layer) is reference material:
its widgets (`raddbg_widgets.c`), views (`raddbg_views.c`) and `rd_window_frame` (full UI draw walk) are worth reading and
adapting but are coupled to its command/cfg systems.

## Editing generated code

`ui/generated/ui.meta.*`, `draw/generated/draw.meta.*`, `window_manager/generated/window_manager.meta.*`,
`render/generated/*` are checked-in outputs. To change e.g. a style stack: edit `ui/ui.mdesk`, then run metagen:
1. Copy `src/metagen/`, `src/mdesk/` (and `base/`) into your tree.
2. `cl … src\metagen\metagen_main.c /link /out:build\metagen.exe` (same flags as the app).
3. Run `build\metagen.exe` — it scans `<exe dir>\..\src` for `*.mdesk` and rewrites every `generated/` folder.
Make this a step in your `build.bat` exactly as raddbg does (`if not exist metagen.exe` build it; always run it).

## Adapting the template

`assets/minimal/src/main.c` marks what is boilerplate and what is yours:
- keep: `app_build_theme`, `app_ui_events_from_wm_events`, `app_draw_ui`, `frame()` skeleton, `entry_point`.
- replace: `app_build_ui()`, `App` fields, font path (use `@embed_file` + `fnt_tag_from_static_data_string` for shipping).
- to add text input: synthesize `UI_EventKind_Navigate/Edit/Text` (see rendering doc) and use `ui_line_edit`.
- to add a custom title bar: open the window with `WM_WindowFlag_CustomBorder`, then each frame call
  `wm_window_clear_custom_border_data`, `wm_window_push_custom_title_bar(w, thickness)` and
  `wm_window_push_custom_title_bar_client_area(w, rect)` for each interactive control rect.

## Linux

The same layers have `linux/` backends (X11 window manager, GLX/EGL OpenGL render, FreeType fonts) and `build.sh`
(`clang`/`gcc`, needs `libfreetype libx11 libxext libxfixes libxrandr libgl libegl`). I did not build or run these;
treat them as less battle-tested than the Windows path (the README says Windows x64 is the primary target and Linux
debugging is in progress).

## Upgrading when upstream changes

Pin the commit you copied (`git rev-parse HEAD` in the raddebugger checkout; store it in `kit/KIT_COMMIT.txt`).
Re-run `extract_kit.ps1` into a scratch dir and diff against your copy: the layers are loosely coupled, so most
upstream changes are local. Recent upstream renames to watch: `demon_core.*` → `demon.*`, BLAKE3 files moved up one
directory (irrelevant to the UI kit), `ui` stack tables gain rows over time (regenerated files must be taken together
with `ui_core.c`).
