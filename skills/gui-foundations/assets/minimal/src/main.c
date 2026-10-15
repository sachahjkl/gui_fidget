////////////////////////////////
//~ Build Options
//
// Must be defined BEFORE including any layer.

#define BUILD_TITLE "gui_minimal"
#define OS_FEATURE_GRAPHICAL 1

////////////////////////////////
//~ Includes (unity build: [h] pass, then [c] pass, same order)
//
// Order matters: a layer may only depend on layers above it.
// window_manager must come before render/draw/ui (they use WM_Window).

//- [h]
#include "base/base_inc.h"
#include "window_manager/window_manager_inc.h"
#include "shell/shell_inc.h"
#include "font_provider/font_provider_inc.h"
#include "render/render_inc.h"
#include "font_cache/font_cache.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"

//- [c]
#include "base/base_inc.c"
#include "window_manager/window_manager_inc.c"
#include "shell/shell_inc.c"
#include "font_provider/font_provider_inc.c"
#include "render/render_inc.c"
#include "font_cache/font_cache.c"
#include "draw/draw.c"
#include "ui/ui_inc.c"

////////////////////////////////
//~ App State

typedef struct App App;
struct App
{
  Arena *arena;
  WM_Window window;
  R_Handle r_window;
  UI_State *ui;
  FNT_Tag font;
  FNT_Tag icon_font;
  DR_Bucket *bucket;

  // frame pacing
  U64 frame_index;
  U64 frames_requested; // >0 means "keep producing frames"; 0 means block on OS events
  U64 frame_depth;      // frame() is re-entered by WM_SIZE/WM_PAINT during live resize

  // theme
  UI_Theme theme;

  // app data
  U64 click_count;
  B32 show_panel;
};
global App app = {0};

////////////////////////////////
//~ Theme
//
// A theme is a flat list of (tags -> color). Lookups pick the pattern with the most
// matching tags where the LAST tag is the queried name. So {"weak","text"} is what
// `UI_TagF("weak") ui_color_from_name("text")` resolves to, falling back to {"text"}.

typedef struct ThemeEntry ThemeEntry;
struct ThemeEntry { char *tags; U32 rgba; };

internal void
app_build_theme(Arena *arena)
{
  ThemeEntry entries[] =
  {
    {"background",               0x1e2127ff},
    {"text",                     0xe6e6e6ff},
    {"border",                   0x3b4048ff},
    {"hover",                    0xffffffff},
    {"drop_shadow",              0x00000090},
    {"selection",                0x4a90e255},
    {"cursor",                   0xffffffff},
    {"focus overlay",            0xffffff10},
    {"focus border",             0x4a90e2ff},
    {"weak text",                0x9aa0a6ff},
    {"floating background",      0x2a2e36ff},
    {"floating text",            0xe6e6e6ff},
    {"floating border",          0x4b5260ff},
    {"good_pop background",      0x2f7d4fff},
    {"good_pop text",            0xffffffff},
  };
  app.theme.patterns_count = ArrayCount(entries);
  app.theme.patterns = push_array(arena, UI_ThemePattern, ArrayCount(entries));
  for EachIndex(idx, ArrayCount(entries))
  {
    String8List tags = str8_split(arena, str8_cstring(entries[idx].tags), (U8 *)" ", 1, 0);
    app.theme.patterns[idx].tags   = str8_array_from_list(arena, &tags);
    app.theme.patterns[idx].linear = linear_from_srgba(rgba_from_u32(entries[idx].rgba));
  }
}

////////////////////////////////
//~ OS events -> UI events

internal void
app_ui_events_from_wm_events(Arena *arena, WM_EventList *wm_events, UI_EventList *out, B32 *quit)
{
  for(WM_Event *e = wm_events->first; e != 0; e = e->next)
  {
    if(e->kind == WM_EventKind_WindowClose) { *quit = 1; }

    UI_Event ev = zero_struct;
    switch(e->kind)
    {
      default:{}break;
      case WM_EventKind_Press:     ev.kind = UI_EventKind_Press;     break;
      case WM_EventKind_Release:   ev.kind = UI_EventKind_Release;   break;
      case WM_EventKind_MouseMove: ev.kind = UI_EventKind_MouseMove; break;
      case WM_EventKind_Text:      ev.kind = UI_EventKind_Text;      break;
      case WM_EventKind_Scroll:    ev.kind = UI_EventKind_Scroll;    break;
      case WM_EventKind_FileDrop:  ev.kind = UI_EventKind_FileDrop;  break;
    }
    if(ev.kind != UI_EventKind_Null)
    {
      ev.key          = e->key;
      ev.modifiers    = e->modifiers;
      ev.string       = e->character ? str8_from_32(arena, str32(&e->character, 1)) : str8_zero();
      ev.paths        = e->strings;
      ev.pos          = e->pos;
      ev.delta_2f32   = e->delta;
      ev.timestamp_us = e->timestamp_us;
      ui_event_list_push(arena, out, &ev);
    }

    // semantic "action slots": the UI layer reacts to Accept/Cancel, not raw keys
    if(e->kind == WM_EventKind_Press && e->key == WM_Key_Return)
    {
      UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Accept;
      ui_event_list_push(arena, out, &a);
    }
    if(e->kind == WM_EventKind_Press && e->key == WM_Key_Esc)
    {
      UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Cancel;
      ui_event_list_push(arena, out, &a);
    }
  }
}

////////////////////////////////
//~ UI draw pass (walk the built box tree, emit rects/text)
//
// Condensed from rd_window_frame() in raddbg_core.c ("draw UI"). The full version adds
// blur, squish transforms, scroll fades, focus overlays, etc. Post-order border drawing
// makes borders land on top of children.

internal void
app_draw_ui(void)
{
  F32 rounded = 1.f;
  Vec4F32 shadow_color = ui_color_from_name(s("drop_shadow"));
  for(UI_Box *box = ui_root_from_state(app.ui); !ui_box_is_nil(box);)
  {
    UI_BoxRec rec = ui_box_rec_df_post(box, &ui_nil_box);
    F32 radii[Corner_COUNT] =
    {
      box->corner_radii[Corner_00]*rounded, box->corner_radii[Corner_01]*rounded,
      box->corner_radii[Corner_10]*rounded, box->corner_radii[Corner_11]*rounded,
    };
    if(box->transparency != 0) { dr_push_transparency(box->transparency); }

    // background (+ animated hover shadow)
    if(box->flags & UI_BoxFlag_DrawBackground)
    {
      F32 t = box->hot_t * (1 - ((box->flags & UI_BoxFlag_DrawActiveEffects) ? box->active_t : 0));
      if(box->flags & UI_BoxFlag_DrawHotEffects)
      {
        Vec4F32 c = shadow_color; c.w *= t * box->background_color.w;
        dr_rect(shift_2f32(pad_2f32(box->rect, 8), v2f32(4, 4)), c, 0.8f, 0, 8.f);
      }
      R_Rect2DInst *inst = dr_rect(pad_2f32(box->rect, 1.f), box->background_color, 0, 0, 1.f);
      MemoryCopyArray(inst->corner_radii, radii);
      if(box->flags & UI_BoxFlag_DrawHotEffects)
      {
        Vec4F32 h = ui_color_from_tags_key_name(box->tags_key, s("hover"));
        h.w *= 0.014f * box->hot_t;   // alphas are blended in LINEAR space: keep overlays tiny (0.01-0.02)
        R_Rect2DInst *hi = dr_rect(pad_2f32(box->rect, 1.f), h, 0, 0, 1.f);
        MemoryCopyArray(hi->corner_radii, radii);
      }
    }

    // text (truncated with "..." at the box's right edge)
    if(box->flags & UI_BoxFlag_DrawText)
    {
      Vec2F32 text_pos = ui_box_text_position(box);
      F32 max_x = 100000.f;
      FNT_Run ellipses = {0};
      if(!(box->flags & UI_BoxFlag_DisableTextTrunc))
      {
        max_x = box->rect.x1 - text_pos.x;
        ellipses = fnt_run_from_string(box->font, box->font_size, 0, box->tab_size, box->text_raster_flags, s("..."));
      }
      dr_truncated_fancy_run_list(text_pos, &box->display_fruns, max_x, ellipses);
    }

    if(box->flags & UI_BoxFlag_Clip)
    {
      Rng2F32 clip = pad_2f32(box->rect, -1);
      Rng2F32 top = dr_top_clip();
      if(top.x1 != 0 || top.y1 != 0) { clip = intersect_2f32(clip, top); }
      dr_push_clip(clip);
    }
    if(box->custom_draw) { box->custom_draw(box, box->custom_draw_user_data); }

    // pop: finish boxes whose subtrees ended here (borders drawn after children)
    S32 pop_idx = 0;
    for(UI_Box *b = box; !ui_box_is_nil(b) && pop_idx <= rec.pop_count; b = b->parent)
    {
      pop_idx += 1;
      if(b == box && rec.push_count != 0) { continue; }
      if(b->flags & UI_BoxFlag_Clip) { dr_pop_clip(); }
      F32 br[Corner_COUNT] =
      {
        b->corner_radii[Corner_00]*rounded, b->corner_radii[Corner_01]*rounded,
        b->corner_radii[Corner_10]*rounded, b->corner_radii[Corner_11]*rounded,
      };
      if(b->flags & UI_BoxFlag_DrawBorder)
      {
        Rng2F32 r = pad_2f32(b->rect, 1.f);
        R_Rect2DInst *inst = dr_rect(r, b->border_color, 0, 1.f, 1.f);
        MemoryCopyArray(inst->corner_radii, br);
      }
      if(b->flags & UI_BoxFlag_Clickable && b->focus_active_t > 0.01f)
      {
        String8 extras[] = {s("focus"), s("border")};
        Vec4F32 c = ui_color_from_tags_key_extras(b->tags_key, (String8Array){extras, ArrayCount(extras)});
        c.w *= b->focus_active_t;
        R_Rect2DInst *inst = dr_rect(b->rect, c, 0, 1.f, 1.f);
        MemoryCopyArray(inst->corner_radii, br);
      }
      if(b->transparency != 0) { dr_pop_transparency(); }
    }
    box = rec.next;
  }
}

////////////////////////////////
//~ UI build (this is the part you write for your own app)

internal void
app_build_ui(void)
{
  // a column that fills the window
  ui_set_next_child_layout_axis(Axis2_Y);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_pct(1, 0));
  UI_Box *page = ui_build_box_from_string(UI_BoxFlag_DrawBackground, s("page"));
  UI_Parent(page) UI_PrefHeight(ui_em(2.5f, 1.f))
  {
    ui_spacer(ui_em(1.f, 1.f));
    UI_Row UI_Padding(ui_em(1.f, 1.f)) UI_PrefWidth(ui_text_dim(10, 1)) UI_CornerRadius(4)
    {
      ui_label(s("Hello, fast immediate-mode UI"));
      ui_spacer(ui_pct(1, 0));
      UI_TagF("weak") ui_labelf("%I64u clicks", app.click_count);
    }
    ui_spacer(ui_em(0.5f, 1.f));
    UI_Row UI_Padding(ui_em(1.f, 1.f)) UI_PrefWidth(ui_text_dim(14, 1)) UI_CornerRadius(4)
    {
      // key = hash of the string; "###" splits label from identity
      if(ui_clicked(ui_button(s("Click me###click_btn"))))
      {
        app.click_count += 1;
      }
      ui_spacer(ui_em(0.5f, 1.f));
      UI_Signal sig = ui_button(s("Toggle panel###toggle_btn"));
      if(ui_clicked(sig)) { app.show_panel ^= 1; }
      if(ui_hovering(sig)) UI_Tooltip
      {
        ui_label(s("Tooltips are just more boxes under a floating root"));
      }
    }
    if(app.show_panel)
    {
      ui_spacer(ui_em(1.f, 1.f));
      UI_Row UI_Padding(ui_em(1.f, 1.f))
      {
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_em(6.f, 1.f));
        ui_set_next_child_layout_axis(Axis2_Y);
        UI_Box *panel = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Clip, s("panel"));
        UI_Parent(panel) UI_PrefWidth(ui_pct(1, 0)) UI_TextAlignment(UI_TextAlign_Center)
        {
          ui_label(s("Animated: boxes persist across frames by key"));
        }
      }
    }
  }
}

////////////////////////////////
//~ Per-frame hook (called by update() in the base entry point)

internal B32
frame(void)
{
  Temp scratch = scratch_begin(0, 0);
  B32 quit = 0;

  // RE-ENTRANCY: during a live resize the window procedure calls update() -> frame() from INSIDE wm_get_events()
  // of an outer frame(). Only the outermost call may pump OS events; nested calls rebuild/redraw/present only.
  app.frame_depth += 1;

  //- block on the OS when idle; poll when we asked for frames
  WM_EventList wm_events = {0};
  if(app.frame_depth == 1) { wm_events = wm_get_events(scratch.arena, app.frames_requested == 0); }
  UI_EventList ui_events = {0};
  app_ui_events_from_wm_events(scratch.arena, &wm_events, &ui_events, &quit);
  if(wm_events.count != 0) { app.frames_requested = 4; }

  //- begin draw frame FIRST: the UI build calls dr_fruns_from_fstrs(), which reads
  // dr_thread_ctx (allocated + reset by dr_begin_frame). Skipping this = null deref.
  dr_begin_frame(app.icon_font);

  //- build
  F32 dt = 1.f / wm_get_system_info()->default_refresh_rate;
  ui_select_state(app.ui);
  UI_IconInfo icon_info = {app.icon_font};
  UI_AnimationInfo anim =
  {
    .hot_animation_rate     = 1 - pow_f32(2, -50.f * dt),
    .active_animation_rate  = 1 - pow_f32(2, -50.f * dt),
    .focus_animation_rate   = 1.f,
    .tooltip_animation_rate = 1 - pow_f32(2, -40.f * dt),
    .menu_animation_rate    = 1 - pow_f32(2, -40.f * dt),
    .scroll_animation_rate  = 1 - pow_f32(2, -60.f * dt),
  };
  ui_begin_build(app.window, &ui_events, &icon_info, &app.theme, &anim, dt, dt);
  ui_push_font(app.font);
  ui_push_font_size(16.f);
  ui_push_text_padding(5.f);
  ui_push_text_raster_flags(FNT_RasterFlag_Smooth|FNT_RasterFlag_Hinted);
  ui_push_pref_width(ui_px(300, 1));
  ui_push_pref_height(ui_px(40, 1));
  app_build_ui();
  ui_end_build();
  if(ui_animating_from_state(app.ui)) { app.frames_requested = 4; }

  //- draw
  app.bucket = dr_bucket_make();
  DR_BucketScope(app.bucket)
  {
    dr_rect(wm_client_rect_from_window(app.window), ui_color_from_name(s("background")), 0, 0, 0);
    app_draw_ui();
  }

  //- (test hook) mirror state into the window title so external tools can read it
  {
    String8 title = push_str8f(scratch.arena, "gui_minimal clicks=%I64u panel=%d hot=%d", app.click_count, app.show_panel, !ui_key_match(ui_hot_key(), ui_key_zero()));
    wm_window_set_title(app.window, title);
  }

  //- present
  r_begin_frame();
  r_window_begin_frame(app.window, app.r_window);
  dr_submit_bucket(app.window, app.r_window, app.bucket);
  r_window_end_frame(app.window, app.r_window);
  r_end_frame();

  //- show the window only after the first frame has been rendered (no white flash)
  if(app.frame_index == 0) { wm_window_first_paint(app.window); }
  app.frame_index += 1;
  if(app.frames_requested > 0) { app.frames_requested -= 1; }

  app.frame_depth -= 1;
  scratch_end(scratch);
  return quit;
}

////////////////////////////////
//~ Entry point (called after all layers auto-init)

internal void
entry_point(CmdLine *cmdline)
{
  app.arena = arena_alloc();
  app.frames_requested = 4;
  app.window = wm_window_open(r2f32p(0, 0, 900, 600), WM_WindowFlag_UseDefaultPosition, s("gui_minimal"));
  app.r_window = r_window_equip(app.window);
  app.font = fnt_tag_from_path(s("C:/Windows/Fonts/segoeui.ttf"));
  app.icon_font = app.font;
  app.ui = ui_state_alloc();
  app_build_theme(app.arena);
  for(;!update(););
}

