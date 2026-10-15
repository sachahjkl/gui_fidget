////////////////////////////////
//~ Commands, command palette, toasts, modal dialog
//
// One command table (CMD_TABLE in fw_app.h) drives: the enum, the palette entries, menu shortcuts, and
// the single dispatch switch below. Menus, shortcuts and the palette all end up in fw_run_cmd().

internal void fw_reset_values(void);   // defined with the pages

////////////////////////////////
//~ Toasts

internal void
fw_toast(ToastKind kind, char *fmt, ...)
{
  int slot = -1;
  U64 oldest = max_U64;
  int oldest_idx = 0;
  for EachIndex(i, TOAST_MAX)
  {
    if(!app.toasts[i].alive) { slot = (int)i; break; }
    if(app.toasts[i].born_us < oldest) { oldest = app.toasts[i].born_us; oldest_idx = (int)i; }
  }
  if(slot < 0) { slot = oldest_idx; }
  Toast *t = &app.toasts[slot];
  MemoryZeroStruct(t);
  va_list args;
  va_start(args, fmt);
  raddbg_vsnprintf(t->text, sizeof(t->text), fmt, args);
  va_end(args);
  t->alive = 1;
  t->kind = kind;
  t->born_us = now_time_us();
  t->life_us = 3800000;
  app.frames_requested = 4;
}

internal void
fw_toasts_build(void)
{
  F32 em = ui_top_font_size();
  U64 now = now_time_us();
  F32 w = Min(em*28.f, app.window_dim.x * 0.9f), h = em*3.1f, gap = em*0.55f, margin = em*1.1f;
  F32 y0 = em*2.6f + margin;
  int slot = 0;
  char *kind_icon[] = {ICON_INFO, ICON_CHECK, ICON_WARN, ICON_WARN_BIG};
  char *kind_stripe[] = {"stripe_info", "stripe_good", "stripe_warn", "stripe_bad"};
  char *kind_text[] = {"weak", "good", "warn", "bad"};
  UI_Box *root = ui_root_from_state(app.ui);
  for EachIndex(i, TOAST_MAX)
  {
    Toast *t = &app.toasts[i];
    if(!t->alive) { continue; }
    F32 age = (F32)(now - t->born_us) / 1000000.f;
    F32 life = (F32)t->life_us / 1000000.f;
    if(age > life) { t->alive = 0; continue; }
    F32 in_t = app.no_anim ? 1.f : fw_clamp01(age / 0.28f);
    F32 ease = 1.f - (1.f - in_t)*(1.f - in_t)*(1.f - in_t);
    F32 out_t = fw_clamp01((life - age) / 0.35f);
    F32 alpha = app.no_anim ? 1.f : Min(ease, out_t);
    F32 x = app.window_dim.x - margin - w + (1.f - ease)*em*6.f;
    F32 y = y0 + (F32)slot*(h + gap);
    slot += 1;
    app.frames_requested = 4;
    // NOTE: UI_FixedX/Y/Width/Height are *stacks*: they would apply to every child built inside the scope too.
    // ui_set_next_* applies to the next box only, which is what a floating container needs.
    UI_Parent(root) UI_TagF("floating") UI_CornerRadius(em*0.6f)
    {
      ui_set_next_fixed_x(x);
      ui_set_next_fixed_y(y);
      ui_set_next_fixed_width(w);
      ui_set_next_fixed_height(h);
      ui_set_next_transparency(1.f - alpha);
      ui_set_next_child_layout_axis(Axis2_X);
      UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow|UI_BoxFlag_Clip, "###toast_%d", (int)i);
      UI_Parent(box)
      {
        UI_TagF("%s", kind_stripe[t->kind]) UI_PrefWidth(ui_px(em*0.4f, 1)) UI_PrefHeight(ui_pct(1, 0))
          ui_build_box_from_key(UI_BoxFlag_DrawBackground, ui_key_zero());
        ui_spacer(ui_px(em*0.7f, 1));
        UI_TagF("%s", kind_text[t->kind]) fw_icon_label(kind_icon[t->kind], em*1.8f);
        ui_spacer(ui_px(em*0.3f, 1));
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(t->text));
        ui_spacer(ui_px(em*0.6f, 1));
      }
      UI_Signal sig = ui_signal_from_box(box);
      if(ui_clicked(sig)) { t->alive = 0; }
    }
  }
}

////////////////////////////////
//~ Command dispatch

internal void fw_palette_toggle(void);
internal F32 fw_text_w(FNT_Tag font, F32 size, char *text);

internal void
fw_run_cmd(CmdKind kind)
{
  switch(kind)
  {
    default:{}break;
    case Cmd_GoControls:   app.page = Page_Controls;   break;
    case Cmd_GoInputs:     app.page = Page_Inputs;     break;
    case Cmd_GoData:       app.page = Page_Data;       break;
    case Cmd_GoPlayground: app.page = Page_Playground; break;
    case Cmd_ThemeMidnight: case Cmd_ThemeGraphite: case Cmd_ThemeDaylight:
    case Cmd_ThemeNord: case Cmd_ThemeDracula: case Cmd_ThemeAmber:
    {
      fw_set_palette((int)(kind - Cmd_ThemeMidnight));
      fw_toast(Toast_Info, "Theme: %s", palettes[app.palette_idx].name);
    }break;
    case Cmd_NextTheme:    fw_set_palette(app.palette_idx + 1); fw_toast(Toast_Info, "Theme: %s", palettes[app.palette_idx].name); break;
    case Cmd_ToggleStatus: app.show_status ^= 1; break;
    case Cmd_ToggleNoAnim: app.anim_mode = app.no_anim ? Anim_On : Anim_Off; fw_anim_resolve(); fw_toast(Toast_Info, app.no_anim ? "Animations off" : "Animations on"); break;
    case Cmd_AnimSystem:   app.anim_mode = Anim_System; app.os_anim_checked_us = 0; fw_anim_resolve(); fw_toast(Toast_Info, app.no_anim ? "Animations follow system (currently off)" : "Animations follow system (currently on)"); break;
    case Cmd_AnimOn:       app.anim_mode = Anim_On;  fw_anim_resolve(); fw_toast(Toast_Info, "Animations always on");  break;
    case Cmd_AnimOff:      app.anim_mode = Anim_Off; fw_anim_resolve(); fw_toast(Toast_Info, "Animations always off"); break;
    case Cmd_ToggleSlowmo: app.slowmo ^= 1; fw_toast(Toast_Info, app.slowmo ? "Slow motion: animations run at 1/5 speed" : "Slow motion off"); break;
    case Cmd_ToggleBoxes:  app.show_boxes ^= 1; break;
    case Cmd_ToastInfo:    fw_toast(Toast_Info, "Heads up: this is an info toast"); break;
    case Cmd_ToastGood:    fw_toast(Toast_Good, "Saved successfully"); break;
    case Cmd_ToastWarn:    fw_toast(Toast_Warn, "Careful: disk is 91%% full"); break;
    case Cmd_ToastBad:     fw_toast(Toast_Bad, "Something went wrong (just kidding)"); break;
    case Cmd_ClearToasts:  MemoryZeroArray(app.toasts); break;
    case Cmd_OpenModal:    app.modal_open = 1; break;
    case Cmd_SpawnBalls:   app.page = Page_Playground; fw_spawn_balls_random(25, dim_2f32(app.arena_rect)); break;
    case Cmd_Confetti:     app.page = Page_Playground; fw_spawn_confetti(v2f32(dim_2f32(app.arena_rect).x*0.5f, dim_2f32(app.arena_rect).y*0.7f), 120); break;
    case Cmd_ClearBalls:   MemoryZeroArray(app.balls); break;
    case Cmd_ResetValues:  fw_reset_values(); fw_toast(Toast_Good, "All widget values reset"); break;
    case Cmd_Quit:         app.want_quit = 1; break;
  }
  app.frames_requested = 4;
}

////////////////////////////////
//~ Command palette

typedef struct PalHit PalHit;
struct PalHit { int cmd; int score; FuzzyMatchRangeList ranges; };
#define PAL_MAX_ROWS 9

internal void
fw_palette_toggle(void)
{
  app.pal_open ^= 1;
  if(app.pal_open)
  {
    MemoryZeroStruct(&app.pal_field);
    app.pal_sel = 0;
    app.pal_scroll = 0;
    app.pal_open_frames = 0;
    app.focus_request = &app.pal_field;
  }
  app.frames_requested = 4;
}

internal void
fw_hline(void)
{
  Vec4F32 c = ui_color_from_name(s("border"));
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_px(1, 1));
  ui_set_next_background_color(c);
  ui_build_box_from_key(UI_BoxFlag_DrawBackground, ui_key_zero());
}

// separator with an explicit pixel width: inside menus (whose width is ChildrenSum) a pct(1,0) child would
// resolve against the nearest sized ancestor = the whole window and stretch the menu
internal void
fw_hline_px(F32 w)
{
  Vec4F32 c = ui_color_from_name(s("border"));
  ui_set_next_pref_width(ui_px(w, 1));
  ui_set_next_pref_height(ui_px(1, 1));
  ui_set_next_background_color(c);
  ui_build_box_from_key(UI_BoxFlag_DrawBackground, ui_key_zero());
}

internal void
fw_palette_build(void)
{
  if(!app.pal_open) { return; }
  Temp scratch = scratch_begin(0, 0);
  F32 em = ui_top_font_size();
  Vec2F32 wd = app.window_dim;
  UI_Box *root = ui_root_from_state(app.ui);
  app.pal_open_frames += 1;

  //- filter + rank commands (each space-separated needle part must match somewhere in the title)
  PalHit hits[Cmd_COUNT];
  int nhits = 0;
  String8 needle = str8(app.pal_field.buf, app.pal_field.size);
  for EachIndex(i, Cmd_COUNT)
  {
    FuzzyMatchRangeList r = fuzzy_match_find(scratch.arena, needle, str8_cstring(cmd_info[i].title));
    if(r.count == r.needle_part_count)
    {
      PalHit h = {(int)i, (int)i, r};
      if(r.first != 0) { h.score = (int)r.first->range.min * 100 + (int)i; }
      hits[nhits++] = h;
    }
  }
  for(int i = 1; i < nhits; i += 1)      // insertion sort by score
  {
    PalHit h = hits[i];
    int j = i - 1;
    for(; j >= 0 && hits[j].score > h.score; j -= 1) { hits[j+1] = hits[j]; }
    hits[j+1] = h;
  }

  //- navigation keys (consumed here, before any widget sees them)
  if(nhits > 0)
  {
    if(ui_key_press(0, WM_Key_Down))     { app.pal_sel = (app.pal_sel + 1) % nhits; }
    if(ui_key_press(0, WM_Key_Up))       { app.pal_sel = (app.pal_sel + nhits - 1) % nhits; }
    if(ui_key_press(0, WM_Key_PageDown)) { app.pal_sel = Min(app.pal_sel + PAL_MAX_ROWS, nhits - 1); }
    if(ui_key_press(0, WM_Key_PageUp))   { app.pal_sel = Max(app.pal_sel - PAL_MAX_ROWS, 0); }
    app.pal_sel = Clamp(0, app.pal_sel, nhits - 1);
    if(app.pal_sel < app.pal_scroll) { app.pal_scroll = app.pal_sel; }
    if(app.pal_sel >= app.pal_scroll + PAL_MAX_ROWS) { app.pal_scroll = app.pal_sel - PAL_MAX_ROWS + 1; }
  }
  B32 want_close = ui_key_press(0, WM_Key_Esc);
  int run_cmd = -1;
  Vec2F32 mouse = ui_mouse();
  B32 mouse_moved = (abs_f32(mouse.x - app.pal_last_mouse.x) + abs_f32(mouse.y - app.pal_last_mouse.y)) > 0.5f;
  app.pal_last_mouse = mouse;

  //- panel (built first => drawn on top of the scrim and gets clicks first)
  F32 pw = Min(em*46.f, wd.x * 0.92f);
  UI_Box *panel = 0;
  ui_set_next_focus_hot(UI_FocusKind_On);
  ui_set_next_focus_active(UI_FocusKind_On);
  ui_set_next_child_layout_axis(Axis2_Y);
  UI_Parent(root) UI_FixedX((wd.x - pw)/2.f) UI_FixedY(Max(em*3.f, wd.y*0.11f)) UI_FixedWidth(pw) UI_PrefHeight(ui_children_sum(1)) UI_TagF("floating") UI_CornerRadius(em*0.8f)
    panel = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow|UI_BoxFlag_DrawBackgroundBlur|UI_BoxFlag_Clip|UI_BoxFlag_DefaultFocusNav|UI_BoxFlag_DisableFocusBorder, s("###pal_panel"));
  UI_Parent(panel) UI_TagF("floating") UI_PrefWidth(ui_pct(1, 0))
  {
    //- input
    FwRow(ui_pct(1, 0), ui_px(em*3.4f, 1))
    {
      ui_spacer(ui_px(em*0.9f, 1));
      FwVCenter(ui_pct(1, 0))
      {
        UI_Key ik = fw_key("pal_input");
        B32 active = ui_is_key_auto_focus_active(ik);
        ui_set_next_pref_height(ui_px(em*2.4f, 1));
        UI_Signal isig = fw_input(&app.pal_field, "Type a command...", "pal_input", ICON_FIND, 0);
        if(app.pal_field.changed) { app.pal_sel = 0; app.pal_scroll = 0; }
        if(ui_committed(isig) && nhits > 0) { run_cmd = hits[Clamp(0, app.pal_sel, nhits - 1)].cmd; }
        if(app.pal_open_frames > 3 && !active && app.focus_request != &app.pal_field) { want_close = 1; }
      }
      ui_spacer(ui_px(em*0.9f, 1));
    }
    fw_hline();
    ui_spacer(ui_px(em*0.4f, 1));

    //- results
    int shown = Min(nhits, PAL_MAX_ROWS);
    for(int vi = 0; vi < shown; vi += 1)
    {
      int hi = app.pal_scroll + vi;
      PalHit *h = &hits[hi];
      CmdInfo *ci = &cmd_info[h->cmd];
      B32 selected = (hi == app.pal_sel);
      FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1))
      {
        ui_spacer(ui_px(em*0.6f, 1));
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_pct(1, 0));
        ui_set_next_child_layout_axis(Axis2_X);
        if(selected) { ui_push_tag(s("selected")); }
        UI_Box *row = 0;
        UI_CornerRadius(em*0.5f)
          row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects|(selected ? UI_BoxFlag_DrawBackground : 0), "###pal_row_%s", ci->title);
        UI_Parent(row)
        {
          ui_spacer(ui_px(em*0.5f, 1));
          UI_TagF("weak") fw_icon_label(ci->icon, em*1.9f);
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))
          {
            UI_Box *lb = ui_label(str8_cstring(ci->title)).box;
            if(h->ranges.count != 0) { ui_box_equip_fuzzy_match_ranges(lb, &h->ranges); }
          }
          UI_PrefWidth(ui_text_dim(em*0.5f, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF("weak") ui_label(str8_cstring(ci->category));
          if(ci->shortcut[0])
          {
            ui_spacer(ui_px(em*0.6f, 1));
            F32 kw = fw_text_w(app.mono, em*0.8f, ci->shortcut) + em*1.2f;
            FwVCenter(ui_px(kw, 1)) fw_kbd(ci->shortcut);
          }
          ui_spacer(ui_px(em*0.6f, 1));
        }
        UI_Signal rsig = ui_signal_from_box(row);
        if(selected) { ui_pop_tag(); }
        if(ui_clicked(rsig)) { run_cmd = h->cmd; }
        if(ui_hovering(rsig) && mouse_moved) { app.pal_sel = hi; }
        ui_spacer(ui_px(em*0.6f, 1));
      }
    }
    if(nhits == 0)
    {
      UI_PrefHeight(ui_px(em*4.f, 1)) UI_TextAlignment(UI_TextAlign_Center) UI_TagF("weak") ui_label(s("No matching commands"));
    }
    ui_spacer(ui_px(em*0.4f, 1));
    fw_hline();
    FwRow(ui_pct(1, 0), ui_px(em*2.3f, 1))
    {
      ui_spacer(ui_px(em*1.0f, 1));
      UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.82f) UI_TagF("weak")
        ui_labelf("%d of %d commands   |   Up/Down select   Enter run   Esc close", nhits, (int)Cmd_COUNT);
    }
  }
  ui_signal_from_box(panel);

  //- scrim (after the panel: clicks outside the panel land here)
  UI_Parent(root) UI_FixedX(0) UI_FixedY(0) UI_FixedWidth(wd.x) UI_FixedHeight(wd.y) UI_TagF("scrim")
  {
    UI_Box *scrim = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground, s("###pal_scrim"));
    UI_Signal ssig = ui_signal_from_box(scrim);
    if(ui_pressed(ssig)) { want_close = 1; }
  }

  if(want_close) { app.pal_open = 0; app.frames_requested = 4; }
  if(run_cmd >= 0) { app.pal_open = 0; fw_run_cmd((CmdKind)run_cmd); }
  scratch_end(scratch);
}

////////////////////////////////
//~ Modal dialog

internal void
fw_modal_build(void)
{
  if(!app.modal_open) { return; }
  F32 em = ui_top_font_size();
  Vec2F32 wd = app.window_dim;
  UI_Box *root = ui_root_from_state(app.ui);
  B32 close = ui_key_press(0, WM_Key_Esc);
  F32 pw = Min(em*30.f, wd.x*0.9f);

  UI_Box *panel = 0;
  ui_set_next_focus_hot(UI_FocusKind_On);
  ui_set_next_focus_active(UI_FocusKind_On);
  ui_set_next_child_layout_axis(Axis2_Y);
  UI_Parent(root) UI_FixedX((wd.x - pw)/2.f) UI_FixedY(wd.y*0.28f) UI_FixedWidth(pw) UI_PrefHeight(ui_children_sum(1)) UI_TagF("floating") UI_CornerRadius(em*0.8f)
    panel = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow|UI_BoxFlag_DrawBackgroundBlur|UI_BoxFlag_DefaultFocusNav|UI_BoxFlag_DisableFocusBorder, s("###modal_panel"));
  UI_Parent(panel) UI_TagF("floating") UI_PrefWidth(ui_pct(1, 0))
  {
    ui_spacer(ui_px(em*1.2f, 1));
    FwRow(ui_pct(1, 0), ui_px(em*2.4f, 1))
    {
      ui_spacer(ui_px(em*1.4f, 1));
      UI_TagF("bad") fw_icon_label(ICON_WARN_BIG, em*2.2f);
      UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*1.25f) ui_label(s("Delete everything?"));
    }
    FwRow(ui_pct(1, 0), ui_children_sum(1))
    {
      ui_spacer(ui_px(em*1.4f, 1));
      UI_PrefWidth(ui_px(pw - em*2.8f, 1)) UI_PrefHeight(ui_px(em*1.7f, 1)) UI_TagF("weak")
        ui_label_multiline(pw - em*2.8f - em, s("This is a demo dialog: a modal floating panel with its own focus root, a scrim that eats clicks, blurred background, and Esc to dismiss."));
    }
    ui_spacer(ui_px(em*0.8f, 1));
    FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1))
    {
      ui_spacer(ui_pct(1, 0));
      if(ui_clicked(fw_button(Btn_Secondary, 0, "Cancel", "modal_cancel", 0))) { close = 1; fw_toast(Toast_Info, "Cancelled"); }
      ui_spacer(ui_px(em*0.6f, 1));
      if(ui_clicked(fw_button(Btn_Danger, ICON_TRASH, "Delete", "modal_ok", 0))) { close = 1; fw_toast(Toast_Bad, "Deleted... nothing, it's a demo"); }
      ui_spacer(ui_px(em*1.4f, 1));
    }
    ui_spacer(ui_px(em*1.2f, 1));
  }
  ui_signal_from_box(panel);

  UI_Parent(root) UI_FixedX(0) UI_FixedY(0) UI_FixedWidth(wd.x) UI_FixedHeight(wd.y) UI_TagF("scrim")
  {
    UI_Box *scrim = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground, s("###modal_scrim"));
    UI_Signal ssig = ui_signal_from_box(scrim);
    if(ui_pressed(ssig)) { close = 1; }
  }
  if(close) { app.modal_open = 0; app.frames_requested = 4; }
}
