# Base layer patterns (`src/base/`)

Contents: keywords · arenas & scratch · per-frame arenas · strings · lists · nil sentinels ·
hash-table caches · scope macros · option structs · threads/stripes · logging/profiling/tests ·
context cracking

Everything here is plain C11 (MSVC `/Zc:preprocessor`, clang, gcc). No STL, no CRT allocation
in hot paths, no exceptions, no RTTI.

## Keywords and basic types (`base_core.h`)

```c
#define internal      static   // file-local function (unity build => everything is "file-local")
#define global        static   // file-local global
#define local_persist static   // function-local static
#define thread_static __declspec(thread) / __thread
#define read_only     // puts data in a read-only section (writes fault on MSVC/clang)
```
Types: `U8..U64 S8..S64 B8..B64 F32 F64`, `U128/U256/U512` unions, `Vec2F32/Vec3F32/Vec4F32`
(unions with `.x .y` and `.v[]`), `Rng1U64`, `Rng2F32` (`.x0 .y0 .x1 .y1` or `.p0 .p1`), `Mat3x3F32`.
Helpers: `Min/Max/Clamp(lo,x,hi)`, `KB/MB/GB`, `AlignPow2`, `ArrayCount`, `OffsetOf`,
`CastFromMember`, `MemoryCopy/Zero/Match(Struct|Array|Typed)`, `Assert` (debug only) vs
`AssertAlways`, `InvalidPath`, `NotImplemented`, `StaticAssert`.

Iteration macros hide the loop boilerplate:
```c
for EachIndex(idx, count)           // U64 idx in [0,count)
for EachElement(idx, array)         // over a C array
for EachEnumVal(UI_MouseButtonKind, k)   // requires Type_COUNT
for EachNode(n, UI_BoxNode, first)  // follow ->next
for EachInRange(i, rng)             // rng.min..rng.max
```

## Arenas (`base_arena.h/.c`)

A virtual-memory bump allocator: **reserve a big range (default 64 MB), commit in 64 KB
steps, push by bumping `pos`**. When a block is exhausted it chains a new block (unless
`ArenaFlag_NoChain`). Pop = move `pos` back; whole arena release = `release_memory`.

```c
Arena *arena = arena_alloc();                          // defaults: 64MB reserve / 64KB commit
Arena *big   = arena_alloc(.reserve_size = GB(64), .commit_size = MB(8));  // option macro
Foo *foos    = push_array(arena, Foo, 100);            // zeroed, 8-byte (or type) aligned
Foo *raw     = push_array_no_zero(arena, Foo, 100);    // skip memset when you fill it
U64 mark     = arena_pos(arena);   ...   arena_pop_to(arena, mark);
arena_clear(arena);                                    // = arena_pop_to(arena, 0)
arena_release(arena);
```
`Temp`/`temp_begin(arena)`/`temp_end(temp)` snapshot+restore `pos`:
```c
Temp temp = temp_begin(arena);
...push scratch data...
temp_end(temp);
```
Failure to commit = immediate abort with a message box; callers never check for NULL.
Debug builds poison unused arena memory under ASan (`AsanPoisonMemoryRegion`) so
use-after-pop is caught.

### Scratch arenas (`base_thread_context.h`)

Each thread owns 2 scratch arenas. Use them for any temporary allocation inside a function:
```c
Temp scratch = scratch_begin(0, 0);            // no conflicts
String8 s = push_str8f(scratch.arena, "%S/%S", dir, name);
...
scratch_end(scratch);
```
**If your function receives an arena that may itself be a scratch arena, pass it as a
conflict** so you don't get the same arena back and then pop away the caller's data:
```c
internal String8 foo(Arena *arena, ...)
{
  Temp scratch = scratch_begin(&arena, 1);     // guaranteed != arena
  ...
}
```
Returning data to a caller uses the *caller's* arena; temporaries use scratch. This single
rule replaces nearly all ownership questions.

### Frame-lifetime arenas

Anything rebuilt every frame lives in an arena that is cleared once per frame. Double-buffer
when the *previous* frame's data must stay readable during the next build:
```c
// ui_core.c
Arena *ui_build_arena(void){ return ui_state->build_arenas[ui_state->build_index % 2]; }
...
ui_state->build_index += 1;
arena_clear(ui_build_arena());   // end of ui_end_build: clears the arena that is now "next"
```
Per-frame arenas: UI boxes with no key (transient), event lists, display strings, draw
buckets (`dr_begin_frame` pops the draw arena back to its start pos), `rd_frame_arena()`.
Persistent things (cached boxes, theme cache nodes) come from a long-lived arena and are
recycled through free lists (`ui_state->first_free_box`).

## Strings (`base_strings.h`)

```c
String8 s = s("literal");                         // str8_lit
String8 t = str8_cstring(argv[1]);                // wraps, doesn't copy
String8 u = push_str8f(scratch.arena, "x=%d name=%S", 5, name);   // %S takes a String8
String8 a = str8_skip(s, 3);  String8 b = str8_prefix(s, 3);  String8 c = str8_substr(s, r1u64(2,5));
B32 same  = str8_match(a, b, StringMatchFlag_CaseInsensitive);
U64 pos   = str8_find_needle(s, 0, s("###"), 0);  // returns s.size if not found
String8List parts = str8_split(arena, s, (U8*)" ", 1, 0);
String8 joined    = str8_list_join(arena, &parts, &(StringJoin){.sep = s(" ")});
```
`str8_lit_comp("x")` is the initializer form for static tables. `str8_varg(s)` expands to
`(int)size, ptr` for `%.*s`. Conversions are named `to_from_from`: `str8_from_u64`,
`u64_from_str8`, `rgba_from_u32`, `linear_from_srgba`, `u64_hash_from_str8`.
Formatting is stb_sprintf with a `%S` extension (`raddbg_snprintf`).

## Intrusive lists (`base_core.h`)

Operate on any struct with `next`/`prev` members (names overridable with `_N`/`_NP`
suffix macros, and nil-aware with `_NPZ`):
```c
DLLPushBack(list->first, list->last, node);       DLLRemove(first, last, node);
SLLQueuePush(first, last, node);                  SLLQueuePop(first, last);
SLLStackPush(top, node);                          SLLStackPop(top);
// custom member names + nil sentinel:
DLLInsert_NPZ(&ui_nil_box, slot->first, slot->last, slot->last, box, hash_next, hash_prev);
SLLStackPush_N(ui_state->first_free_box, box, hash_next);
```
Typical node recycling: pop from a free-list stack else `push_array` from the owning arena;
on release `SLLStackPush` onto the free list. Memory never returns to the OS during a
session but stays bounded by peak use.

## Nil sentinels

```c
read_only global UI_Box ui_nil_box = { &ui_nil_box, &ui_nil_box, /* every link */ ... };
B32 ui_box_is_nil(UI_Box *b){ return b == 0 || b == &ui_nil_box; }
// freshly built box: box->first = box->last = box->next = box->prev = box->parent = &ui_nil_box;
```
Lookups return the nil sentinel instead of NULL (`ui_box_from_key` → `&ui_nil_box`), so
`box->flags`, `box->rect` etc. are always safe to read (all zero). The same idiom: `cfg_nil_node`,
`md_nil_node`, `rd_nil_window_state`. Declaring it `read_only` makes accidental writes fault
immediately.

## Hash-table caches with LRU-ish pruning

Pattern used by UI boxes, anim nodes, theme-color cache, font style cache, view states:
```c
U64 slot_idx = key.u64[0] % table_size;
for(UI_Box *b = table[slot_idx].hash_first; !ui_box_is_nil(b); b = b->hash_next)
  if(ui_key_match(b->key, key)) { result = b; break; }
```
Each node stores `last_touched_build_index`; at the end of the build, nodes not touched this
build are unlinked and pushed on the free list:
```c
if(box->last_touched_build_index < ui_state->build_index || key is zero) { unlink; free-list; }
```
So "garbage collection" is a side effect of the code that *uses* the data — you don't
write destroy paths. Animation nodes use `last_touched+2 < build_index` and an
intrusive LRU list so the prune loop can stop at the first fresh node.

## Scope macros (`DeferLoop`)

```c
#define DeferLoop(begin, end)        for(int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))
#define DeferLoopChecked(begin, end) // end only runs if begin returned true (e.g. UI_CtxMenu(key))
#define MutexScope(m)  DeferLoop(mutex_take(m), mutex_drop(m))
#define UI_Parent(b)   DeferLoop(ui_push_parent(b), ui_pop_parent())
#define DR_BucketScope(b) DeferLoop(dr_push_bucket(b), dr_pop_bucket())
```
They nest without braces: `UI_Row UI_PrefWidth(ui_pct(1,0)) UI_TagF("weak") ui_label(s("x"));`
Caveat: `break`/`return`/`goto` inside the body skips the `end` call — don't.

## Option structs through macros

```c
internal Arena *arena_alloc_(ArenaParams *params);
#define arena_alloc(...) arena_alloc_(&(ArenaParams){.reserve_size = arena_default_reserve_size, \
    .commit_size = arena_default_commit_size, .allocation_site_file = __FILE__, \
    .allocation_site_line = __LINE__, __VA_ARGS__})
```
Defaults first, caller overrides last (later designators win). Also captures `__FILE__/__LINE__`.
Same style for `ac_artifact_from_key`, `ui_anim`, `access_pt_is_expired`, `rd_cmd`, `dr_fstrs_push_new`.

## Threads, locks, stripes (`base_threads.h`)

`Mutex` (recursive), `RWMutex`, `CondVar`, `Semaphore`, `Barrier` are 8-byte handles.
Shared tables use **stripes**: an array of `{arena, rw_mutex, cv, free}` indexed by
`slot_idx` so contention is per-stripe and each stripe owns its allocation arena:
```c
Stripe *stripe = stripe_from_slot_idx(&cache->stripes, slot_idx);
RWMutexScope(stripe->rw_mutex, 0 /*read*/) { ...lookup... }
RWMutexScope(stripe->rw_mutex, 1 /*write*/){ ...insert... }
```
Atomics are macros: `ins_atomic_u64_inc_eval`, `ins_atomic_u32_eval_assign`,
`ins_atomic_u64_eval_cond_assign(ptr, new, expected)` (CAS).

**Lanes**: async worker threads are a *group* with `lane_idx()`, `lane_count()`,
`lane_sync()` (barrier), `lane_sync_u64(&v, src_lane)` (broadcast), `lane_range(count)`
(this lane's slice). A task can fan out across all workers without a job system:
```c
Rng1U64 range = lane_range(count);
for EachInRange(i, range) { ...process item i... }
lane_sync();
```

## Logging, profiling, tests

- `log_scope_begin()` / `log_info(s)` / `log_user_error(s)` / `log_scope_end(arena)`: frame-wide
  structured logging into per-thread arenas; `rd_frame` collects it into a file and a UI toast.
- `ProfBeginFunction() / ProfScope("name") { } / ProfEnd()` compile to nothing unless built with
  `telemetry` or `spall` (build.bat flags). Sprinkle them freely.
- Tests: `Test(name){ TestCheck(cond); }` self-register via static constructors when
  `BUILD_TESTS`; run with `run_tests.bat/.sh`; exemplars compared byte-for-byte from `data/`.

## Context cracking (`base_context_cracking.h`)

One header turns compiler predefines into `COMPILER_MSVC/CLANG/GCC`, `OS_WINDOWS/LINUX/MAC`,
`ARCH_X64/ARM64/...`, `LANG_C/CPP`, `BUILD_DEBUG`, `BUILD_TESTS`. All platform branches in the
rest of the code key off those (never raw `_WIN32`).

## OS abstraction convention

Functions tagged `@per_os_impl` in headers are implemented once per OS in `win32/...` and
`linux/...`, selected in `base_inc.c`:
```c
#if OS_WINDOWS
# include "win32/base/win32_base.c"
#elif OS_LINUX
# include "linux/base/linux_base.c"
#endif
```
The Win32 file also owns `WinMain`, which calls `main_thread_base_entry_point` — that is
why a program only defines `entry_point(CmdLine*)` (and `frame()` for graphical apps).
