////////////////////////////////
//~ gui_fidget: a widget playground on the RAD Debugger UI stack
//
// Pages: Controls (buttons, switches, sliders, dropdowns, menus, tooltips, progress, toasts),
// Inputs (validation, fuzzy search, tag input, form), Data (20,000-row virtualized sortable table,
// file tree), Playground (physics balls, fidget spinner, color picker, draggable notes).
// Overlays: command palette (Ctrl+K), modal dialog, toast stack, menu bar. 6 live-switchable themes.
//
// Test flags: --page=N --theme=N --palette --palette_query=txt --modal --menu=N(1..4) --toast

#define BUILD_TITLE "gui_fidget"
#define OS_FEATURE_GRAPHICAL 1

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

//- app layers (same include discipline: a file may only use what is above it)
#include "fw_app.h"
#include "fw_theme.c"
#include "fw_widgets.c"
#include "fw_physics.c"
#include "fw_commands.c"
#include "fw_pages.c"

////////////////////////////////
//~ OS events -> UI events (+ global shortcuts + text-editing semantics)

internal void
app_ui_events_from_wm_events(Arena *arena, WM_EventList *wm_events, UI_EventList *out)
{
  for(WM_Event *e = wm_events->first; e != 0; e = e->next)
  {
    B32 ctrl  = !!(e->modifiers & WM_Modifier_Ctrl);
    B32 shift = !!(e->modifiers & WM_Modifier_Shift);
    B32 alt   = !!(e->modifiers & WM_Modifier_Alt);
    if(e->kind == WM_EventKind_WindowClose) { app.want_quit = 1; continue; }

    //- global shortcuts
    if(e->kind == WM_EventKind_Press && ctrl && !alt)
    {
      B32 handled = 1;
      if(e->key == WM_Key_K)                    { fw_palette_toggle(); }
      else if(e->key == WM_Key_P && shift)      { fw_palette_toggle(); }
      else if(e->key == WM_Key_1)               { fw_run_cmd(Cmd_GoControls); }
      else if(e->key == WM_Key_2)               { fw_run_cmd(Cmd_GoInputs); }
      else if(e->key == WM_Key_3)               { fw_run_cmd(Cmd_GoData); }
      else if(e->key == WM_Key_4)               { fw_run_cmd(Cmd_GoPlayground); }
      else if(e->key == WM_Key_T)               { fw_run_cmd(Cmd_NextTheme); }
      else if(e->key == WM_Key_B)               { fw_run_cmd(Cmd_ToggleStatus); }
      else if(e->key == WM_Key_J)               { fw_run_cmd(Cmd_ToggleNoAnim); }
      else { handled = 0; }
      if(handled) { continue; }
    }

    if(e->kind == WM_EventKind_Scroll) { app.dbg_wheel_events += 1; }

    //- raw passthrough
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
    if(ev.kind == UI_EventKind_Text && (e->character == '\n' || ctrl || alt)) { ev.kind = UI_EventKind_Null; }
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
    if(e->kind != WM_EventKind_Press) { continue; }

    //- semantic action slots
    if(e->key == WM_Key_Return) { UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Accept; ui_event_list_push(arena, out, &a); }
    if(e->key == WM_Key_Esc)    { UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Cancel; ui_event_list_push(arena, out, &a); }

    //- text-editing semantics (consumed by ui_line_edit when a field is active)
    UI_Event t = zero_struct;
    UI_EventDeltaUnit unit = ctrl ? UI_EventDeltaUnit_Word : UI_EventDeltaUnit_Char;
    switch(e->key)
    {
      default:{}break;
      case WM_Key_Left: case WM_Key_Right:
      {
        S32 dir = (e->key == WM_Key_Left) ? -1 : +1;
        t.kind = UI_EventKind_Navigate; t.delta_unit = unit; t.delta_2s32 = v2s32(dir, 0);
        t.flags = shift ? (UI_EventFlag_KeepMark|UI_EventFlag_ExplicitDirectional)
                        : (UI_EventFlag_PickSelectSide|UI_EventFlag_ZeroDeltaOnSelect|UI_EventFlag_ExplicitDirectional);
      }break;
      case WM_Key_Home: case WM_Key_End:
      {
        S32 dir = (e->key == WM_Key_Home) ? -1 : +1;
        t.kind = UI_EventKind_Navigate; t.delta_unit = UI_EventDeltaUnit_Line; t.delta_2s32 = v2s32(dir, 0);
        t.flags = shift ? UI_EventFlag_KeepMark : 0;
      }break;
      case WM_Key_Backspace: case WM_Key_Delete:
      {
        S32 dir = (e->key == WM_Key_Backspace) ? -1 : +1;
        t.kind = UI_EventKind_Edit; t.delta_unit = unit; t.delta_2s32 = v2s32(dir, 0);
        t.flags = UI_EventFlag_Delete|UI_EventFlag_ZeroDeltaOnSelect;
      }break;
      case WM_Key_A:
      if(ctrl)
      {
        UI_Event a = zero_struct; a.kind = UI_EventKind_Navigate; a.delta_unit = UI_EventDeltaUnit_Whole; a.delta_2s32 = v2s32(-1, 0);
        ui_event_list_push(arena, out, &a);
        t.kind = UI_EventKind_Navigate; t.delta_unit = UI_EventDeltaUnit_Whole; t.delta_2s32 = v2s32(+1, 0); t.flags = UI_EventFlag_KeepMark;
      }break;
      case WM_Key_C: if(ctrl) { t.kind = UI_EventKind_Edit; t.flags = UI_EventFlag_Copy|UI_EventFlag_KeepMark; } break;
      case WM_Key_X: if(ctrl) { t.kind = UI_EventKind_Edit; t.flags = UI_EventFlag_Copy|UI_EventFlag_Delete; } break;
      case WM_Key_V:
      if(ctrl)
      {
        String8 clip = push_str8_copy(arena, wm_get_clipboard_text(arena));
        for EachIndex(i, clip.size) { if(clip.str[i] < 32) { clip.str[i] = ' '; } }
        t.kind = UI_EventKind_Text; t.flags = UI_EventFlag_Paste; t.string = clip;
      }break;
    }
    if(t.kind != UI_EventKind_Null) { t.timestamp_us = e->timestamp_us; ui_event_list_push(arena, out, &t); }
  }
}

////////////////////////////////
//~ UI draw walk (condensed from rd_window_frame in raddbg_core.c)
//
// Order: parents first, children after (siblings visited in reverse so earlier siblings draw on top);
// borders / focus rings / debug outlines are drawn when a subtree finishes, so they land above children.

internal void
app_draw_ui(void)
{
  Vec4F32 shadow_color = ui_color_from_name(s("drop_shadow"));
  F32 squish_eps = 0.001f;
  for(UI_Box *box = ui_root_from_state(app.ui); !ui_box_is_nil(box);)
  {
    UI_BoxRec rec = ui_box_rec_df_post(box, &ui_nil_box);
    F32 radii[Corner_COUNT] = {box->corner_radii[Corner_00], box->corner_radii[Corner_01], box->corner_radii[Corner_10], box->corner_radii[Corner_11]};

    if(box->transparency != 0) { dr_push_transparency(box->transparency); }
    if(box->squish > squish_eps)
    {
      Vec2F32 dim = dim_2f32(box->rect);
      Vec2F32 c = center_2f32(box->rect);
      Vec2F32 anchor = (box->flags & UI_BoxFlag_SquishAnchored) ? v2f32(c.x, box->rect.y0 - dim.y/8.f) : c;
      Mat3x3F32 to_origin = make_translate_3x3f32(v2f32(-anchor.x, -anchor.y));
      Mat3x3F32 scale = make_scale_3x3f32(v2f32(1 - box->squish, 1 - box->squish));
      Mat3x3F32 back = make_translate_3x3f32(anchor);
      dr_push_xform2d(mul_3x3f32(back, mul_3x3f32(scale, to_origin)));
      dr_push_tex2d_sample_kind(R_Tex2DSampleKind_Linear);
    }
    if(box->flags & UI_BoxFlag_DrawDropShadow)
    {
      R_Rect2DInst *inst = dr_rect(shift_2f32(pad_2f32(box->rect, 8), v2f32(4, 4)), shadow_color, 0.8f, 0, 8.f);
      MemoryCopyArray(inst->corner_radii, radii);
    }
    if(box->flags & UI_BoxFlag_DrawBackgroundBlur)
    {
      R_PassParams_Blur *blur = dr_blur(pad_2f32(box->rect, 1.f), box->blur_size*(1 - box->transparency), 0);
      MemoryCopyArray(blur->corner_radii, radii);
    }
    if(box->flags & UI_BoxFlag_DrawBackground)
    {
      F32 t = box->hot_t * (1 - ((box->flags & UI_BoxFlag_DrawActiveEffects) ? box->active_t : 0));
      if(box->flags & UI_BoxFlag_DrawHotEffects)
      {
        Vec4F32 sc = shadow_color; sc.w *= t * box->background_color.w * 0.5f;
        R_Rect2DInst *si = dr_rect(shift_2f32(pad_2f32(box->rect, 6), v2f32(2, 3)), sc, 0.8f, 0, 7.f);
        MemoryCopyArray(si->corner_radii, radii);
      }
      R_Rect2DInst *inst = dr_rect(pad_2f32(box->rect, 1.f), box->background_color, 0, 0, 1.f);
      MemoryCopyArray(inst->corner_radii, radii);
    }
    if(box->flags & UI_BoxFlag_DrawHotEffects)
    {
      // NOTE: colors blend in linear space, so these overlay alphas must be tiny
      Vec4F32 h = ui_color_from_tags_key_name(box->tags_key, s("hover"));
      h.w *= 0.014f * box->hot_t + 0.014f * box->active_t;
      R_Rect2DInst *hi = dr_rect(pad_2f32(box->rect, 1.f), h, 0, 0, 1.f);
      MemoryCopyArray(hi->corner_radii, radii);
    }
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
      if(box->flags & UI_BoxFlag_HasFuzzyMatchRanges)
      {
        Vec4F32 mc = ui_color_from_tags_key_name(box->tags_key, s("accent"));
        mc.w *= 0.30f;
        dr_truncated_fancy_run_fuzzy_matches(text_pos, &box->display_fruns, max_x, &box->fuzzy_match_ranges, mc);
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

    S32 pop_idx = 0;
    for(UI_Box *b = box; !ui_box_is_nil(b) && pop_idx <= rec.pop_count; b = b->parent)
    {
      pop_idx += 1;
      if(b == box && rec.push_count != 0) { continue; }
      if(b->flags & UI_BoxFlag_Clip) { dr_pop_clip(); }
      if(b->flags & UI_BoxFlag_DrawBorder)
      {
        R_Rect2DInst *inst = dr_rect(pad_2f32(b->rect, 1.f), b->border_color, 0, 1.f, 1.f);
        MemoryCopyArray(inst->corner_radii, b->corner_radii);
      }
      if(b->flags & (UI_BoxFlag_DrawSideTop|UI_BoxFlag_DrawSideBottom|UI_BoxFlag_DrawSideLeft|UI_BoxFlag_DrawSideRight))
      {
        Rng2F32 r = b->rect;
        if(b->flags & UI_BoxFlag_DrawSideTop)    { dr_rect(r2f32p(r.x0, r.y0, r.x1, r.y0 + 1.f), b->border_color, 0, 0, 0); }
        if(b->flags & UI_BoxFlag_DrawSideBottom) { dr_rect(r2f32p(r.x0, r.y1 - 1.f, r.x1, r.y1), b->border_color, 0, 0, 0); }
        if(b->flags & UI_BoxFlag_DrawSideLeft)   { dr_rect(r2f32p(r.x0, r.y0, r.x0 + 1.f, r.y1), b->border_color, 0, 0, 0); }
        if(b->flags & UI_BoxFlag_DrawSideRight)  { dr_rect(r2f32p(r.x1 - 1.f, r.y0, r.x1, r.y1), b->border_color, 0, 0, 0); }
      }
      B32 is_nav_hot = (!ui_key_match(b->key, ui_key_zero()) && ui_key_match(b->key, app.nav_hot_key));
      if(b->flags & UI_BoxFlag_Clickable && !(b->flags & UI_BoxFlag_DisableFocusBorder) && (is_nav_hot || b->focus_active_t > 0.01f))
      {
        String8 extras[] = {s("focus"), s("border")};
        Vec4F32 c = ui_color_from_tags_key_extras(b->tags_key, (String8Array){extras, ArrayCount(extras)});
        c.w *= is_nav_hot ? 1.f : b->focus_active_t;
        R_Rect2DInst *inst = dr_rect(pad_2f32(b->rect, 1.f), c, 0, 1.5f, 1.f);
        MemoryCopyArray(inst->corner_radii, b->corner_radii);
      }
      if(b->disabled_t >= 0.005f)
      {
        Vec4F32 bg = ui_color_from_name(s("background")); bg.w = b->disabled_t * 0.5f;
        R_Rect2DInst *inst = dr_rect(b->rect, bg, 0, 0, 1.f);
        MemoryCopyArray(inst->corner_radii, b->corner_radii);
      }
      if(app.show_boxes)
      {
        Vec4F32 oc = fw_hsv_color((F32)((((U64)b) >> 5) & 255) / 255.f, 0.8f, 1.f, 0.35f);
        dr_rect(b->rect, oc, 0, 1.f, 0.f);
      }
      if(b->squish > squish_eps) { dr_pop_xform2d(); dr_pop_tex2d_sample_kind(); }
      if(b->transparency != 0)   { dr_pop_transparency(); }
    }
    box = rec.next;
  }
}

////////////////////////////////
//~ Per-frame hook

internal B32
frame(void)
{
  Temp scratch = scratch_begin(0, 0);

  // RE-ENTRANCY: WM_SIZE / WM_PAINT call update() -> frame() from inside wm_get_events() during live resize.
  // Only the outermost call pumps OS events; nested calls just rebuild, redraw and present.
  app.frame_depth += 1;
  WM_EventList wm_events = {0};
  if(app.frame_depth == 1) { wm_events = wm_get_events(scratch.arena, app.frames_requested == 0); }
  UI_EventList ui_events = {0};
  app_ui_events_from_wm_events(scratch.arena, &wm_events, &ui_events);
  if(wm_events.count != 0) { app.frames_requested = 4; }

  U64 t_begin = now_time_us();
  F32 dt_real = (app.last_frame_us == 0) ? (1.f/60.f) : (F32)(t_begin - app.last_frame_us) / 1000000.f;
  app.last_frame_us = t_begin;
  app.real_dt = Clamp(0.0005f, dt_real, 0.1f) * (app.slowmo ? 0.2f : 1.f);

  F32 dpi = wm_dpi_from_window(app.window);
  if(dpi < 48.f) { dpi = 96.f; }
  app.scale = dpi / 96.f;
  app.window_dim = dim_2f32(wm_client_rect_from_window(app.window));

  dr_begin_frame(app.icons);                       // must precede ui_begin_build
  fw_theme_update();
#if OS_WINDOWS
  {
    // match the OS title bar to the palette (immersive dark mode attribute = 20)
    local_persist int last_dark = -1;
    int dark = palettes[app.palette_idx].dark;
    W32_WM_Window *w32 = w32_wm_window_from_handle(app.window);
    if(w32 != 0 && dark != last_dark) { BOOL v = dark; DwmSetWindowAttribute(w32->hwnd, 20, &v, sizeof(v)); last_dark = dark; }
  }
#endif

  F32 dt = 1.f / wm_get_system_info()->default_refresh_rate;
  F32 adt = dt * (app.slowmo ? 0.2f : 1.f);
  ui_select_state(app.ui);
  UI_IconInfo icon_info = {app.icons};
  // rate 1 = jump straight to the target value, i.e. no animation (hover, press, focus, menus, tooltips, scrolling)
  F32 r50 = app.no_anim ? 1.f : 1 - pow_f32(2, -50.f * adt);
  F32 r40 = app.no_anim ? 1.f : 1 - pow_f32(2, -40.f * adt);
  UI_AnimationInfo anim =
  {
    .hot_animation_rate     = r50,
    .active_animation_rate  = r50,
    .focus_animation_rate   = r50,
    .tooltip_animation_rate = r40,
    .menu_animation_rate    = r40,
    .scroll_animation_rate  = r40,
  };
  ui_begin_build(app.window, &ui_events, &icon_info, &app.theme, &anim, dt, adt);
  ui_push_font(app.font);
  ui_push_font_size(floor_f32(15.f * app.scale));
  ui_push_text_padding(floor_f32(7.f * app.scale));
  ui_push_text_raster_flags(FNT_RasterFlag_Smooth|FNT_RasterFlag_Hinted);
  ui_push_blur_size(10.f);
  ui_push_pref_width(ui_px(300, 1));
  ui_push_pref_height(ui_px(40, 1));

  //- overlays first: they must be built before the page so they draw on top and get clicks first
  fw_toasts_build();
  fw_palette_build();
  fw_modal_build();
  fw_shell_build();

  ui_end_build();
  app.ui_boxes_total = ui_state->last_build_box_count;
  if(ui_animating_from_state(app.ui)) { app.frames_requested = 4; }
  if(!ui_key_match(ui_active_key(UI_MouseButtonKind_Left), ui_key_zero())) { app.frames_requested = Max(app.frames_requested, 2); }
  if(app.pal_open || app.modal_open) { app.frames_requested = Max(app.frames_requested, 2); }

  //- window title (--title_state: expose internal state for scripted tests)
  {
    String8 title = push_str8f(scratch.arena, "gui_fidget  |  %s  |  %s", page_names[app.page], palettes[app.palette_idx].name);
    if(app.debug_title)
    {
      U64 balls = 0, toasts = 0;
      for EachIndex(i, BALL_MAX) { balls += app.balls[i].alive; }
      for EachIndex(i, TOAST_MAX) { toasts += app.toasts[i].alive; }
      title = push_str8f(scratch.arena, "S pg=%d pal=%d vol=%.0f sw1=%d seg=%d dd0=%d rate=%d step=%I64d chk1=%d palopen=%d modal=%d menu=%d balls=%I64u spin=%.2f scy=%.0f/%.0f bnd=%.0f vh=%.0f wheel_evts=%I64u sort=%d/%d sel=%I64d toasts=%I64u",
                         (int)app.page, app.palette_idx, app.slider[0], app.sw[1], app.seg, app.dd[0], app.rating, app.stepper, app.chk[1],
                         app.pal_open, app.modal_open, app.menu_open, balls, app.spin_omega,
                         fw_scroll_box ? fw_scroll_box->view_off.y : 0.f, fw_scroll_box ? fw_scroll_box->view_off_target.y : 0.f,
                         fw_scroll_box ? fw_scroll_box->view_bounds.y : 0.f, fw_scroll_box ? fw_scroll_box->fixed_size.y : 0.f, app.dbg_wheel_events,
                         app.sort_col, app.sort_asc, app.selected_row, toasts);
    }
    U64 n = Min(title.size, sizeof(app.last_title) - 1);
    B32 same = (MemoryMatch(app.last_title, title.str, n) && app.last_title[n] == 0);
    if(!same)   // only touch the OS title when it changed
    {
      MemoryCopy(app.last_title, title.str, n);
      app.last_title[n] = 0;
      wm_window_set_title(app.window, title);
    }
  }

  //- draw
  app.bucket = dr_bucket_make();
  DR_BucketScope(app.bucket)
  {
    dr_rect(wm_client_rect_from_window(app.window), ui_color_from_name(s("background")), 0, 0, 0);
    app_draw_ui();
  }
  U64 t_end = now_time_us();
  app.ui_ms = mix_1f32(app.ui_ms, (F32)(t_end - t_begin) / 1000.f, 0.25f);
  app.ms_hist[app.ms_hist_idx % 64] = (F32)(t_end - t_begin) / 1000.f;
  app.ms_hist_idx += 1;

  //- present
  r_begin_frame();
  r_window_begin_frame(app.window, app.r_window);
  dr_submit_bucket(app.window, app.r_window, app.bucket);
  r_window_end_frame(app.window, app.r_window);
  r_end_frame();

  if(app.frame_index == 0) { wm_window_first_paint(app.window); }
  app.frame_index += 1;
  if(app.frames_requested > 0) { app.frames_requested -= 1; }

  app.frame_depth -= 1;
  scratch_end(scratch);
  return app.want_quit;
}

////////////////////////////////
//~ DPI-aware initial window size (see worked-example notes: size AFTER creation, from the window's DPI)

internal void
app_fit_window_to_dpi(WM_Window window, F32 logical_w, F32 logical_h)
{
#if OS_WINDOWS
  F32 dpi = wm_dpi_from_window(window);
  if(dpi < 48.f) { dpi = 96.f; }
  F32 sc = dpi / 96.f;
  Vec2F32 work = wm_dim_from_monitor(wm_monitor_from_window(window));
  F32 w = logical_w * sc;
  F32 h = logical_h * sc;
  if(work.x > 0) { w = Min(w, work.x * 0.94f); }
  if(work.y > 0) { h = Min(h, work.y * 0.92f); }
  W32_WM_Window *w32 = w32_wm_window_from_handle(window);
  if(w32 != 0) { SetWindowPos(w32->hwnd, 0, 0, 0, (int)w, (int)h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE); }
#else
  (void)window; (void)logical_w; (void)logical_h;
#endif
}

////////////////////////////////
//~ Entry point

internal void
entry_point(CmdLine *cmdline)
{
  app.arena = arena_alloc();
  app.frames_requested = 4;
  app.menu_open = -1;
  app.show_status = 1;
  fw_rng_state ^= now_time_us() * 0x9e3779b97f4a7c15ull;
  fw_theme_init();
  fw_set_palette(0);
  fw_reset_values();
  fw_data_init();
  app.tree_open[0] = 1; app.tree_open[1] = 1; app.tree_open[10] = 1; app.tree_open[6] = 1;
  app.tree_sel = 2;
  for EachIndex(i, 64) { app.ms_hist[i] = 0; }

  //- test flags
  {
    U64 v = 0;
    if(try_u64_from_str8_c_rules(cmd_line_string(cmdline, s("page")), &v))  { app.page = (PageKind)Min(v, Page_COUNT - 1); }
    if(try_u64_from_str8_c_rules(cmd_line_string(cmdline, s("theme")), &v)) { fw_set_palette((int)v); }
    if(try_u64_from_str8_c_rules(cmd_line_string(cmdline, s("menu")), &v))  { app.debug_open_menu = (int)v; }
    if(cmd_line_has_flag(cmdline, s("palette")))
    {
      fw_palette_toggle();
      String8 q = cmd_line_string(cmdline, s("palette_query"));
      if(q.size > 0) { U64 n = Min(q.size, sizeof(app.pal_field.buf)); MemoryCopy(app.pal_field.buf, q.str, n); app.pal_field.size = app.pal_field.cursor = app.pal_field.mark = n; }
    }
    if(cmd_line_has_flag(cmdline, s("title_state"))) { app.debug_title = 1; }
    if(cmd_line_has_flag(cmdline, s("modal"))) { app.modal_open = 1; }
    if(cmd_line_has_flag(cmdline, s("toast"))) { fw_toast(Toast_Good, "Welcome to gui_fidget"); fw_toast(Toast_Warn, "Press Ctrl+K for the command palette"); }
    if(cmd_line_has_flag(cmdline, s("balls")))  { app.page = Page_Playground; }
  }

  app.window = wm_window_open(r2f32p(0, 0, 1180, 780), WM_WindowFlag_UseDefaultPosition, s("gui_fidget"));
  app_fit_window_to_dpi(app.window, 1180, 780);
  app.r_window = r_window_equip(app.window);
  app.font  = fnt_tag_from_path(s("C:/Windows/Fonts/segoeui.ttf"));
  app.mono  = fnt_tag_from_path(s("C:/Windows/Fonts/consola.ttf"));
  app.icons = fnt_tag_from_path(push_str8f(app.arena, "%S/icons.ttf", get_process_info()->binary_path));
  app.ui = ui_state_alloc();
  for(;!update(););
}
