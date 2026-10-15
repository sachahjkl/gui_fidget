# Architecture, build and async patterns

Contents: unity build & include discipline · layer anatomy · build script anatomy · metagen/mdesk ·
multi-target repo (several exes from one tree) · async artifact cache · access scopes · lanes ·
generation counters · IPC/wakeups · tests · how to apply this to a new project

## Unity build: one TU per executable

`src/raddbg/raddbg_main.c` is the whole debugger as one compilation unit:
```c
#define BUILD_TITLE "The RAD Debugger"
#define OS_FEATURE_GRAPHICAL 1
#define DMN_INIT_MANUAL 1          // layers whose init the program orders itself

//- [h] pass: every layer's header, in dependency order
#include "base/base_inc.h"
#include "window_manager/window_manager_inc.h"
...
#include "ui/ui_inc.h"
#include "raddbg/raddbg_inc.h"

//- [c] pass: the same list, .c files
#include "base/base_inc.c"
...
```
Rules that make this work:
- Headers contain only types, globals declarations and prototypes (all `internal`, i.e. `static`);
  `.c` files contain all definitions. Because there is one TU, `static` everywhere is fine and the
  compiler sees every call.
- Dependency order of the include list is the layer DAG. A layer's `.h` may use only types from
  layers above it in the list. Per-layer `*_inc.h/.c` bundles a layer + its selected backend.
- Layers announce presence with their header guard (`#if defined(UI_H)`), and
  `base_entry_point.c` uses that to auto-init / auto-tick: `#if defined(FONT_CACHE_H) && !defined(FNT_INIT_MANUAL) fnt_init();`
  Adding a layer to the include list is enough to get it initialized; no registry.
- Targets share layers freely: `radlink` (linker), `radbin`, `torture`, `metagen`, the debugger and test tools
  each have their own `*_main.c` listing only what they need (`metagen_main.c` = base + mdesk + metagen).
- Compile times stay low because there is little templated/header-heavy code and no STL. Release builds
  are noted as "significantly longer" than debug in the README; debug builds are the daily driver.

## Anatomy of a layer

```
ui/
  ui_core.h / ui_core.c           // engine
  ui_basic_widgets.h / .c         // optional widgets
  ui_inc.h / ui_inc.c             // includes the above (what mains include)
  ui.mdesk                        // metaprogram input (tables)
  generated/ui.meta.h / .c        // checked-in output of metagen
```
Backend-bearing layers add subfolders (`render/d3d11`, `render/opengl`, `render/stub`) and an `_inc` that picks
one; OS-specific layers live in `win32/<layer>/` and `linux/<layer>/` and implement `@per_os_impl` functions
declared in the portable header. `lib_*` layers (`lib_rdi`, `lib_rdi_make`, `lib_raddbg_markup`) include nothing
from the codebase so other projects can drop them in.

## Build script anatomy (`build.bat`, `build.sh`)

- **Arguments are bare words that become variables**: `for %%a in (%*) do set "%%~a=1"` → `build raddbg release clang asan`.
  Targets (`raddbg`, `radlink`, `radbin`, `torture`, …) and options (`release`, `clang`, `asan`, `telemetry`, `pgo`, `opengl`)
  are the same mechanism; unknown targets print a warning. `no_meta` skips metagen.
- Compile line pieces (MSVC): `/I..\src\ /I..\local\ /nologo /FC /Z7 /Zc:preprocessor`; debug `/Od /Ob1 /DBUILD_DEBUG=1`;
  release `/O2 /DBUILD_DEBUG=0`; link `/INCREMENTAL:NO /opt:ref /opt:icf /MANIFEST:EMBED /NATVIS:src\natvis\base.natvis`.
  Clang: `-mcx16 -g -O0|-O2 -Wall` with a long `-Wno-…` list, `-gcodeview` (or `-gdwarf`), `-fuse-ld=lld`.
- Git hash is baked in: `-DBUILD_GIT_HASH=\"%%i\"` from `git describe --always --dirty`.
- Pre-steps in order: make `build/` + `local/`, compile icon resource, **build and run `metagen`**, assemble
  BLAKE3 asm into a `.lib` once, then compile each requested target from `build/` (`..\src\x\x_main.c`).
- Debug-info niceties: `.natvis` visualizers for base types (`src/natvis/base.natvis`) so String8/arrays
  display well in other debuggers; `/pdbaltpath:%_PDB%`.
- PGO is scripted (`pgo` runs the target to produce a profile then rebuilds with `-fprofile-use`).
- `./run_tests.bat|sh` builds the `torture`/test mains and runs registered `Test(...)` functions.

Minimal script for your own project: see `assets/minimal/build.bat` (one `cl` line).

## metagen and `.mdesk`

`metagen_main.c` is itself a base-layer program. It locates `<dir of metagen.exe>/../src`, recursively
parses **every `*.mdesk`**, gathers `@table`s, and writes per-layer `generated/<layer>.meta.h` and `.meta.c`.
Place `metagen.exe` in `<project>/build/` and keep sources under `<project>/src/`. Generated files are
**committed**, so consumers of a layer (like this skill's kit) never need to run it unless they edit a `.mdesk`.

```
// 1. data table (columns named in @table)
@table(name, display_string, cfg_string)
WM_KeyTable:
{
  {Null   "Invalid Key" "null"}
  {Esc    "Escape"      "esc"}
}

// 2. generators
@enum WM_Key:                      // @enum(U32) NAME: makes `typedef U32 NAME; enum NAMEEnum{...}`
{
  @expand(WM_KeyTable a) `$(a.name)`,    // → WM_Key_Null, WM_Key_Esc, …  (prefix = enum name)
  COUNT,
}
@data(String8) wm_key_display_name_table:         // → read-only global array
{
  @expand(WM_KeyTable a) `str8_lit_comp("$(a.display_string)")`;
}
```
Other features seen in the repo:
- `@gen { ... }` raw text emission into the `.h` (or `@gen @c_file` into the `.c`): used to generate all UI stack
  node types, the `UI_DeclStacks` macro bodies, and `ui_push_*/pop_*/top_*/set_next_*` functions.
- `@struct Name:` emits a struct from rows; `@xlist` X-macro lists; `@expand_commands`, `@expand_if`,
  `@default(v)`, `@inherit(x)` are *app-level* tags consumed by raddbg's schema code, not by metagen.
- `@embed_file name: "../data/font.ttf"` → `read_only global String8 name` with the bytes baked in;
  `@embed_string name: """..."""` → multi-line string constant (used for HLSL/GLSL shaders).
- String expressions: `$(a.col)`, alignment `$(=>35)`, conditionals
  `$(a.x == "" -> "..." )`, `$(a.is_query -> 'X(' .. a.name_lower .. ')')`, concatenation `..`.
- Output header always starts with the MIT notice, `//- GENERATED CODE`, include guard `<LAYER>_META_H`.

Why it matters: one source row can drive enum value + display string + config key + icon + hotkey
table + generated function bodies, and adding a row can't desynchronize them.

## Async artifact cache (`artifact_cache/`, `AC_`)

Goal: the UI thread asks for "the thing computed from this key" every frame and **never waits**.
```c
typedef AC_Artifact AC_CreateFunctionType(String8 key, B32 *cancel_signal, AC_Status *status_out, U64 *gen_out);
typedef void        AC_DestroyFunctionType(AC_Artifact artifact);

// UI thread, every frame (endt_us = 0 => don't block at all)
Access *access = access_open();
B32 stale = 0;
AC_Artifact a = ac_artifact_from_key(access, key_bytes, my_create, my_destroy, 0,
                                     .gen = current_generation, .flags = AC_Flag_Wide,
                                     .evict_threshold_us = 2000000, .stale_out = &stale);
use(a);                       // zeros if never computed yet; old value if stale
access_close(access);         // releases eviction protection
```
Semantics:
- Caches are keyed by the **create function pointer**, then by key **bytes**; so any plain struct hashed via `str8_struct(&k)`
  is a key. The value is a 32-byte `AC_Artifact {U64 u64[4]}` (pointer + sizes + whatever).
- `gen` is the *validity generation*: if the stored `last_completed_gen != gen` the artifact is stale; the cache returns the
  stale value immediately (unless `AC_Flag_WaitForFresh`) and enqueues a recompute. Bump `gen` when the underlying data
  changes (file changed → `fs_change_gen()`, debug info loaded → `di_load_gen()`), never invalidate manually.
- `AC_Flag_HighPriority` goes to a separate request queue serviced first and cancels low-priority wide work in progress via
  `cancel_signal`; `AC_Flag_Wide` means the task runs on **all lanes** cooperatively (use `lane_range`/`lane_sync` inside `create`);
  thin tasks run on one lane.
- `AC_Status_NeedRetry` re-queues (e.g. dependency not ready); `Failed` caches the failure.
- Eviction runs inside the async tick: a node expires when untouched for `evict_threshold_us` (default 2 s) *and* not referenced by
  an open `Access`, not working, and (if other artifacts depend on it) a newer request generation exists.
- Concurrency: stripe locks (`RWMutex`) per slot range; reads take the shared lock; the fast path is lock-shared + atomic loads.
- When called from inside an artifact-create function (`ac_tctx != 0`) the node is flagged `other_nodes_depend_on_me` so
  dependency chains aren't evicted from under their dependents.

Callers seen in the repo: `fs_artifact_create` (file bytes by path+gen), `dasm_artifact_create` (disassembly),
`di_search_artifact_create` (fuzzy symbol search), `d_memory_artifact_create` (target memory pages), call stacks, geometry buffers,
MD5/SHA1 of blobs. All follow the same 5-line caller pattern above.

### Access scopes (`base_thread_context`)
`access_open()` creates a per-thread scope; `access_touch(access, &node->access_pt, cv)` increments a refcount on the node and
records last-touch time + update tick; `access_close` decrements. The eviction predicate is
`access_pt_is_expired(pt, .time = us, .update_idxs = n)`. Rule: **open an Access around any code that reads artifact memory,
close it before sleeping for vsync** (raddbg's comment in `rd_frame`), otherwise eviction threads wait on you.

### The async tick (`base_entry_point.c`)
At startup `N = logical cores − main threads` worker threads are launched (min 1) as *lanes* of one group; each loops:
lane 0 waits on a condvar (1 s timeout, signalled by new requests), then **all lanes** call each included layer's
`*_async_tick()` (`ac_async_tick`, `c_async_tick`, `fs_async_tick`, `di_async_tick`, …) between `lane_sync()` barriers.
`async_loop_again` / `async_loop_again_high_priority` are atomic flags that make the loop spin once more without sleeping.
Override with `--async_thread_count N`. Layers join the tick merely by being included.

## Other recurring patterns

- **Command/registers architecture (raddbg app level)**: everything user-visible is a *command* (`rd_cmd(Kind, .field = …)`), keys map
  to commands through a rebindable table, commands are queued and applied once per frame in one `switch` — that is why IPC
  (`raddbg --ipc "step_over"` over a TCP socket) and the UI share one code path. "Regs" (current window/panel/tab/thread…) are a
  stack pushed with `RD_RegsScope(...)` instead of globals.
- **Config as a tree**: user/project/command-line/transient config are one `CFG_Node` tree (`config/`), serialized as `.mdesk`;
  windows/panels/tabs are nodes in it, so layouts persist and the UI is a pure function of the tree.
- **Separate processes for risky work**: debug-info conversion runs in a child `raddbg --bin` process; IPC via TCP/shared memory.
- **Wakeups**: any thread → `wm_send_wakeup_event()` (posts a thread message) to rouse a blocked UI loop.
- **GUI + CLI in one name (Windows `.com` shim)**: `raddbg.exe` is a GUI-subsystem app; `com_shim_main.c` (a ~60-line
  base-only program built as `com_shim.exe`, copied to `raddbg.com`) is a console-subsystem launcher. `cmd.exe` prefers `.com`
  over `.exe`, so typing `raddbg` in a terminal runs the shim, which launches `raddbg.exe --cli …` (waiting + a Job Object
  that kills the child if the shim dies when `--cli`/`--bin`), while Explorer double-click runs the GUI exe directly.
- **Everything has a stub**: `R_BACKEND_STUB`, `WM_STUB`, `window_manager_stub.c`, `socket_stub`, `http_stub` → servers/CLI builds
  of the same code with no graphics.

## Applying it to a new project (recipe)

1. Skeleton: `proj/src/{base,…copied layers…,app/app_main.c}`, `proj/build.bat`, `proj/build/`.
2. `app_main.c`: build options defines → `[h]` includes → `[c]` includes → `frame()` + `entry_point()`.
3. Keep one `App` struct of persistent state (arena + data); pass nothing global except that struct and layer states.
4. Put per-frame temporaries in scratch or a frame arena; anything cached across frames uses a hash table + `last_touched` prune.
5. Anything that can take >1 ms and isn't needed *this* frame → artifact cache (or your own async job) with a generation.
6. When tables appear (commands, colors, icons, key names, settings schema) write an `.mdesk` + generators instead of parallel arrays.
7. Add `ProfScope`s early; build with `spall`/`telemetry` when needed.
8. Add layers by appending to both include lists; delete layers by deleting two lines.
