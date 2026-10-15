# Window → frame loop → draw → GPU

Contents: layer stack · window manager API · the frame loop (idle, pacing, first paint) ·
OS events → UI events · draw layer (buckets, stacks, fancy strings) · render layer (passes,
batches, backends) · the SDF rect shader · fonts · custom title bar · DPI · startup/idle checklist

## Layer stack (bottom → top)

```
base ─ window_manager (WM_) ─ shell (SH_ dialogs) ─ font_provider (FP_) ─ render (R_)
     ─ font_cache (FNT_) ─ draw (DR_) ─ ui (UI_) ─ your app
```
Backends: `win32/window_manager`, `win32/shell`, `font_provider/dwrite` (Windows) or `freetype`
(Linux); `render/d3d11` (Windows), `render/opengl` (Linux: GLX/EGL), `render/stub` (headless).
Selected by `R_BACKEND`, `FP_BACKEND`, `WM_STUB` macros or `OS_*`.

## Window manager API (`window_manager.h`)

Handles are `U64` structs. Core calls:
```c
WM_Window w = wm_window_open(r2f32p(0,0,900,600), WM_WindowFlag_UseDefaultPosition, s("title"));
wm_window_first_paint(w);                    // actually SW_SHOW; call after the first frame rendered
WM_EventList evs = wm_get_events(arena, B32 wait);   // wait=1 blocks in GetMessageW
Rng2F32 client = wm_client_rect_from_window(w);       F32 dpi = wm_dpi_from_window(w);
Vec2F32 mouse = wm_mouse_from_window(w);              WM_Modifiers m = wm_get_modifiers();
wm_set_cursor(WM_Cursor_IBar);   wm_set_clipboard_text(str);   wm_send_wakeup_event();
```
`wm_send_wakeup_event()` posts a thread message so *any* thread (async workers, a debugger control
thread, a socket) can wake the blocked UI thread. Events: `Press/Release/MouseMove/Text/Scroll/
WindowLoseFocus/WindowClose/FileDrop/Wakeup`, with `key` (generated `WM_Key_*` table from
`window_manager.mdesk`), `modifiers`, `character`, `pos`, `delta`, `timestamp_us`. Helpers
`wm_key_press(&events, window, mods, key)` consume an event for hotkeys.

Windows implementation notes (`win32_window_manager.c`): per-monitor-v2 DPI awareness is set
dynamically via `GetProcAddress` (works on old Windows too); runs a gfx thread id for wakeups;
custom borders are done with `WM_NCCALCSIZE` (shrink the non-client area to the resize frame) and
`WM_NCHITTEST` (map regions to `HTCAPTION`/`HTTOP`/…) plus `DwmExtendFrameIntoClientArea`.
`wm_window_push_custom_title_bar(w, thickness)` / `…_client_area(w, rect)` tell the hit-test which
rects are draggable caption vs interactive UI — you draw your own menu bar/buttons in that strip
(`WM_WindowFlag_CustomBorder`). Fullscreen toggles the style bits; maximize compensates frame padding.

## The frame loop

`main_thread_base_entry_point` → inits layers → spawns async lanes → calls your `entry_point`, which
loops `for(;!update(););`. `update()` = `fnt_frame()` + your `frame()`. In raddbg, `rd_frame()` is
the one big per-frame function; the skeleton to copy:

```c
// 1. get events — block when nothing asked for a frame
WM_EventList events = wm_get_events(scratch.arena, num_frames_requested == 0 && !DEV_always_refresh);
// 2. decide target Hz from recent frame times (snap to 60/75/120/144/165/240)
// 3. consume events → commands / UI events        (any event => rd_request_frame())
// 4. build UI per window; if ui_animating_from_state(ui) => rd_request_frame()
// 5. draw into DR buckets, then r_window_submit + present (vsync Present(1,0))
// 6. num_frames_requested -= 1   (rd_request_frame sets it to 4)
// 7. frame 0: wm_window_first_paint(window)       (window stays hidden until it has content)
// 8. frame 15: SetProcessWorkingSetSize(self, -1, -1)  (trim startup pages on Windows)
```
**Re-entrancy:** during a live resize/move Windows runs a modal loop and the window procedure calls `update()` on
`WM_SIZE`/`WM_PAINT` — from inside `wm_get_events()`. So `frame()` must tolerate nesting: count `frame_depth`, pump OS
events only at depth 1 (raddbg: `if(rd_state->frame_depth == 1) events = wm_get_events(...)`, plus guards on every
once-per-frame step), and let nested calls only rebuild/draw/present. This also gives smooth live-resize rendering.

Why 4 frames: interaction uses last frame's layout and animations need to see the *new* state a
couple of times; requesting a few frames after any input makes everything settle without a
"dirty" system. Zero input + zero animation = process sleeps in `GetMessageW` (0% CPU).
Async completions call `wm_send_wakeup_event` (raddbg: `d_set_wakeup_hook`) so results appear
immediately instead of at the next input.

Target-Hz picking: it starts at the monitor refresh rate and falls to the highest standard rate
whose frame budget the last ~N frames actually met, so a slow frame doesn't spiral; `frame_dt = 1/target_hz`
feeds animation rates, and `frame_eval_memread_endt_us = now + (budget - avg_frame_time) + 4ms` gives
data readers a *time budget* instead of letting them block the frame.

## OS events → UI events

See `assets/minimal/src/main.c: app_ui_events_from_wm_events`. Minimum viable mapping:
`WM_EventKind_{Press,Release,MouseMove,Text,Scroll,FileDrop}` → same-named `UI_EventKind_*` (copy `key`,
`modifiers`, `pos`, `delta`, `timestamp_us`, `character → string`, `strings → paths`), plus synthetic
`UI_EventActionSlot_Accept` on Return and `Cancel` on Esc. For text editing also synthesize
`UI_EventKind_Navigate/Edit` events with `delta_unit`/`delta_2s32` and flags (see raddbg's
`RD_CmdKind_MoveLeft/…/Delete/Copy/Paste` cases around `raddbg_core.c:17646+`): arrows →
`Navigate` (`UI_EventDeltaUnit_Char/Word/Line/Page/Whole`), Backspace/Delete → `Edit` with
`UI_EventFlag_Delete`, Copy → `Edit` with `UI_EventFlag_Copy|KeepMark`, Cut → `Edit` with
`Copy|Delete`, Paste → `Text` event with `UI_EventFlag_Paste` and `string = wm_get_clipboard_text(arena)`
(`raddbg_core.c` ~18040–18061).
`ui_single_line_txt_op_from_event` turns them into replace-range operations.

## Draw layer (`draw.h`)

A **bucket** = ordered list of render passes built on the CPU. You create one per window per frame
and select it with a scope:
```c
dr_begin_frame(icon_font);                       // resets the per-thread draw arena
DR_Bucket *b = dr_bucket_make();
DR_BucketScope(b)
{
  dr_rect(rect, color, corner_radius, border_thickness, edge_softness);   // returns R_Rect2DInst* to tweak
  dr_text(font, size, 0, tab_size, raster_flags, pos, color, string);
  DR_ClipScope(rect) DR_TransparencyScope(0.5f) DR_XForm2DScope(mat) { ... }
  dr_blur(rect, blur_size, corner_radius);       // backdrop blur pass (frosted tooltips/menus)
}
r_window_submit(os_window, r_window, &b->passes);   // or dr_submit_bucket(...)
```
Draw state (`tex2d_sample_kind`, `xform2d`, `clip`, `transparency`) is itself a generated stack (see
`draw.mdesk`); a change in the stack starts a new batch group so state is captured per group, not per rect.
`dr_rect` returns the instance so you can set per-corner colors (gradients) and per-corner radii:
```c
R_Rect2DInst *r = dr_rect(rect, v4f32(0,0,0,0), 0, 0, 1.f);
r->colors[Corner_00] = r->colors[Corner_10] = shadow;           // top edge opaque
r->colors[Corner_01] = r->colors[Corner_11] = v4f32(0,0,0,0);   // fades to transparent at bottom
MemoryCopyArray(r->corner_radii, box->corner_radii);
```
Fancy strings: `DR_FStrList` = list of `{String8, {font, raster_flags, color, size, underline, strike}}`;
`dr_fruns_from_fstrs` shapes them into `DR_FRun`s (cached on the box), `dr_truncated_fancy_run_list`
draws them clipped at `max_x` with a trailing `...` run, `dr_truncated_fancy_run_fuzzy_matches`
highlights fuzzy-search matches.

## Render layer (`render_core.h`)

Backend API is a handful of `r_hook` functions: `r_init`, `r_window_equip/unequip`,
`r_tex2d_alloc/release/fill_tex2d_region`, `r_buffer_alloc/release`, `r_begin_frame/r_end_frame`,
`r_window_begin_frame/end_frame`, `r_window_submit(window, equip, &passes)`. Pass kinds: **UI** (rect
batches), **Blur**, **Geo3D** (meshes with view/projection — used for the 3D viewer), plus internal
composite/finalize. Instance data is packed raw bytes in `R_BatchList` chunks (256 instances per chunk).

D3D11 specifics: hardware device, falls back to WARP if creation fails; flip-discard swap chain;
`Present(1, 0)` (vsync); each batch is one `DrawInstanced(4, instance_count, 0, 0)` (a 4-vertex strip per
instance, `SV_VertexID` selects the corner); shaders are HLSL **embedded as strings in the `.mdesk`**
(`@embed_string r_d3d11_g_rect_shader_src: """ ... """`) and compiled with `D3DCompile` at startup.

## The SDF rect shader (why it looks so good)

One shader draws filled rects, rounded rects, borders, soft shadows, gradients, glyphs and images.
Per instance: `dst_rect, src_rect, color[4 corners], corner_radii[4], border_thickness, softness, omit_texture, shear`.
Vertex stage computes, per corner, color and radius plus an `sdf_sample_pos` (position relative to the
rect center in pixels). Pixel stage:
```hlsl
float rect_sdf(float2 p, float2 half_size, float r) { return length(max(abs(p) - half_size + r, 0.0)) - r; }
...
// corner/AA mask: 1 inside, smooth 0 across 2*softness
corner_sdf_t = 1 - smoothstep(0, 2*softness, rect_sdf(p, half_size - softness*2, radius));
// border: ring between rect and an inset rect
if(border_thickness > 0) border_t = smoothstep(0, 2*softness, rect_sdf(p, half_size - softness*2 - border, max(radius-border,0)));
color = texture_sample * tint(4-corner bilinear) ; color.a *= opacity * corner_sdf_t * border_t;
```
Tricks to reuse: *softness* > ~1 turns a rect into a blurred drop shadow (raddbg draws shadows as
`dr_rect(padded, shadow_color, 0.8f /*radius*/, 0, 8.f /*softness*/)`); `edge_softness = 1` on normal boxes gives
anti-aliased edges for free; borders are drawn *after children* by drawing at pop time; colors are converted to
linear before blending (`linear_from_srgba`), textures sampled through an sRGB→linear step. Text uses the same
instance with `src_rect` pointing into the glyph atlas and `omit_texture = 0`; plain rects set it to 1.

## Fonts (`font_provider`, `font_cache`)

- Provider (`FP_`): loads a font (path or in-memory data), reports metrics, rasterizes one glyph run to a bitmap
  (DirectWrite on Windows, FreeType on Linux). Flags: `FNT_RasterFlag_Smooth` (anti-alias) and `Hinted`.
- Cache (`FNT_`): `FNT_Tag` = 128-bit font identity (`fnt_tag_from_path`, `fnt_tag_from_static_data_string(&bytes)`
  for embedded fonts). Rasterized glyphs are packed into GPU atlas textures with a quad-tree region allocator
  (`fnt_atlas_region_alloc`); lookups go `(tag,size,flags) → style node → direct ASCII map | hash → raster info`;
  shaped strings are cached per frame in `run_slots` (`fnt_run_from_string` → `FNT_Run{pieces,dim,ascent,descent}`).
- Measure without drawing: `fnt_dim_from_tag_size_string`, `fnt_char_pos_from_tag_size_string_p` (hit-testing),
  `fnt_wrapped_string_lines_from_font_size_string_max` (word wrap), `fnt_metrics_from_tag_size`.
- raddbg embeds its fonts with metagen: `@embed_file rd_default_main_font_bytes: "../data/segoeui.ttf"` and loads
  them via `fnt_tag_from_static_data_string`. A separate **icon font** (`data/icons.ttf`) renders all icons as
  text glyphs (`UI_IconInfo.icon_font` + codepoint strings), so icons are recolorable, scalable, free.
- DPI change → `fnt_reset()` and scale the font size by `new_dpi/old_dpi` (see `rd_window_frame`).

## Startup & idle checklist (what raddbg does; copy it)

- [ ] Window created hidden; **first frame rendered, then `wm_window_first_paint`** (no white/black flash).
- [ ] Layer init is explicit and minimal; set `X_INIT_MANUAL` to order/skip inits (`raddbg_main.c` does manual
      init of dmn/d/wm/fp/r/fnt/rd so it controls order).
- [ ] No runtime asset reads for core UI: fonts/icons embedded (`fnt_tag_from_static_data_string`); theme presets
      are embedded text (`md_tree_from_string(rd_theme_preset_cfg_string_table[p])`) parsed once at init.
- [ ] Heavy work (symbol loading, file parsing, search) is async behind the artifact cache; UI never waits.
- [ ] Debug builds use `-O0`/`/Od`; still fast because the build is one TU and the UI core is data-oriented.
- [ ] `SetProcessWorkingSetSize` at frame ~15 on Windows to drop one-time init pages.
- [ ] No frames when idle: `wm_get_events(wait=1)`; wake with `wm_send_wakeup_event`.
- [ ] DPI: per-monitor aware v2; fonts re-rasterized on DPI change; rects floored to whole pixels.
- [ ] Small binaries: `raddbg.exe` is ~4.4 MB (whole debugger, fonts embedded, measured on the local
      `build/raddbg.exe`); a UI-only app built from this kit is ~300 KB (measured).
