////////////////////////////////
//~ Widget library built on the UI core
//
// Conventions: every widget takes a stable `id` (hashed as "###id" under the nearest keyed ancestor) so
// its hover / press / animation state survives rebuilds; state lives in caller-owned variables; the root
// box takes its size from the style stacks (caller sets UI_PrefWidth/Height) unless noted.

////////////////////////////////
//~ Layout helpers

internal UI_Box *
fw_row_begin(UI_Size w, UI_Size h)
{
  ui_set_next_pref_width(w);
  ui_set_next_pref_height(h);
  return ui_row_begin();
}

internal UI_Box *
fw_col_begin(UI_Size w, UI_Size h)
{
  ui_set_next_pref_width(w);
  ui_set_next_pref_height(h);
  return ui_column_begin();
}

#define FwRow(w, h)    DeferLoop(fw_row_begin((w), (h)), ui_row_end())
#define FwCol(w, h)    DeferLoop(fw_col_begin((w), (h)), ui_column_end())
// vertically centers the single child built inside (only valid when the parent row has a known height)
#define FwVCenter(w)   FwCol((w), ui_pct(1, 0)) UI_Padding(ui_pct(1, 0))
#define FwTip(sig, str) if(ui_hovering(sig)) UI_Tooltip { ui_label(str8_cstring(str)); }

internal UI_Key
fw_key(char *id)
{
  return ui_key_from_stringf(ui_active_seed_key(), "###%s", id);
}

internal void
fw_icon_label(char *icon, F32 em_width)
{
  UI_PrefWidth(ui_px(em_width, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_Font(app.icons) UI_TextAlignment(UI_TextAlign_Center) UI_FlagsAdd(UI_BoxFlag_DisableTextTrunc)
    ui_label(str8_cstring(icon));
}

////////////////////////////////
//~ Cards & form rows

internal void
fw_card_begin(char *title, char *icon)
{
  F32 em = ui_top_font_size();
  ui_push_tag(s("card"));
  ui_set_next_child_layout_axis(Axis2_Y);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_children_sum(1));
  UI_Box *card = 0;
  UI_CornerRadius(em*0.6f)
    card = ui_build_box_from_stringf(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, "###card_%s", title);
  ui_push_parent(card);
  ui_spacer(ui_px(em*0.9f, 1));
  fw_row_begin(ui_pct(1, 0), ui_children_sum(1));
  ui_spacer(ui_px(em*1.1f, 1));
  fw_col_begin(ui_pct(1, 0), ui_children_sum(1));
  UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_px(em*2.2f, 1)) UI_Row
  {
    if(icon) { UI_TagF("weak") fw_icon_label(icon, em*1.8f); }
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*1.1f) ui_label(str8_cstring(title));
  }
}

internal void
fw_card_end(void)
{
  F32 em = ui_top_font_size();
  ui_column_end();
  ui_spacer(ui_px(em*1.1f, 1));
  ui_row_end();
  ui_spacer(ui_px(em*0.9f, 1));
  ui_pop_parent();
  ui_pop_tag();
  ui_spacer(ui_px(em*1.0f, 1));
}

#define FwCard(title, icon) DeferLoop(fw_card_begin((title), (icon)), fw_card_end())

internal void
fw_form_row_begin(char *label)
{
  F32 em = ui_top_font_size();
  fw_row_begin(ui_pct(1, 0), ui_px(em*2.5f, 1));
  UI_PrefWidth(ui_pct(0.34f, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak") ui_label(str8_cstring(label));
  ui_push_pref_width(ui_pct(1, 0));
  ui_push_pref_height(ui_pct(1, 0));
}

internal void
fw_form_row_end(void)
{
  ui_pop_pref_height();
  ui_pop_pref_width();
  ui_row_end();
}

#define FwFormRow(label) DeferLoop(fw_form_row_begin(label), fw_form_row_end())

////////////////////////////////
//~ Buttons

typedef enum BtnKind { Btn_Secondary, Btn_Primary, Btn_Danger, Btn_Ghost } BtnKind;
global char *btn_tag[] = {"secondary_button", "accent_button", "danger_button", "ghost_button"};

internal UI_Signal
fw_button(BtnKind kind, char *icon, char *label, char *id, B32 disabled)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  ui_push_tag(str8_cstring(btn_tag[kind]));
  ui_set_next_child_layout_axis(Axis2_X);
  ui_set_next_pref_width(ui_children_sum(1));
  ui_set_next_pref_height(ui_px(em*2.2f, 1));
  ui_set_next_squish(fw_squish(prev->active_t * 0.045f));
  UI_Box *b = 0;
  UI_CornerRadius(em*0.45f)
    b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawActiveEffects|(disabled ? UI_BoxFlag_Disabled : 0), "###%s", id);
  UI_Parent(b)
  {
    ui_spacer(ui_px(em*0.95f, 1));
    if(icon) { fw_icon_label(icon, em*1.5f); }
    UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(label));
    ui_spacer(ui_px(em*0.95f, 1));
  }
  UI_Signal sig = ui_signal_from_box(b);
  ui_pop_tag();
  if(disabled) { sig.f = 0; }
  return sig;
}

internal UI_Signal
fw_icon_button(char *icon, char *id, F32 em_size)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  ui_push_tag(s("ghost_button"));
  ui_set_next_pref_width(ui_px(em*em_size, 1));
  ui_set_next_pref_height(ui_px(em*em_size, 1));
  ui_set_next_font(app.icons);
  ui_set_next_font_size(em*0.9f);
  ui_set_next_text_alignment(UI_TextAlign_Center);
  ui_set_next_squish(fw_squish(prev->active_t * 0.06f));
  UI_Box *b = 0;
  UI_CornerRadius(em*0.4f)
    b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawActiveEffects|UI_BoxFlag_DisableTextTrunc, "%s###%s", icon, id);
  UI_Signal sig = ui_signal_from_box(b);
  ui_pop_tag();
  return sig;
}

internal void
fw_kbd(char *text)
{
  F32 em = ui_top_font_size();
  ui_push_tag(s("kbd"));
  UI_Font(app.mono) UI_FontSize(em*0.8f) UI_CornerRadius(em*0.3f)
  {
    ui_set_next_pref_width(ui_text_dim(em*0.5f, 1));
    ui_set_next_pref_height(ui_px(em*1.5f, 1));
    ui_set_next_text_alignment(UI_TextAlign_Center);
    ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawText|UI_BoxFlag_DisableTextTrunc, str8_cstring(text));
  }
  ui_pop_tag();
}

////////////////////////////////
//~ Switch / checkbox / radio

typedef struct FwSwitchDraw FwSwitchDraw;
struct FwSwitchDraw { F32 t; F32 hot; F32 active; Vec4F32 track; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(fw_switch_draw)
{
  FwSwitchDraw *d = (FwSwitchDraw *)user_data;
  Rng2F32 r = box->rect;
  F32 h = r.y1 - r.y0;
  Vec4F32 col = mix_4f32(d->track, d->accent, d->t);
  dr_rect(r, col, h/2.f, 0, 1.f);
  F32 pad = h*0.14f;
  F32 kd = h - 2.f*pad;
  F32 kw = kd * (1.f + 0.28f*d->active + 0.06f*d->hot);
  F32 x = mix_1f32(r.x0 + pad, r.x1 - pad - kw, d->t);
  dr_rect(r2f32p(x - 1, r.y0 + pad + 1, x + kw + 1, r.y1 - pad + 2), v4f32(0, 0, 0, 0.30f), kd/2.f, 0, 3.f);
  dr_rect(r2f32p(x, r.y0 + pad, x + kw, r.y1 - pad), v4f32(1, 1, 1, 1), kd/2.f, 0, 1.f);
}

internal UI_Signal
fw_switch(B32 *v, char *label, char *id)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  F32 t = ui_anim(ui_key_from_string(key, s("t")), *v ? 1.f : 0.f, .rate = fw_rate(34));
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *row = 0;
  UI_CornerRadius(em*0.4f) row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, "###%s", id);
  UI_Parent(row)
  {
    ui_spacer(ui_px(em*0.4f, 1));
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(label));
    FwVCenter(ui_px(em*3.0f, 1))
    {
      ui_set_next_pref_width(ui_px(em*2.6f, 1));
      ui_set_next_pref_height(ui_px(em*1.4f, 1));
      UI_Box *sw = ui_build_box_from_key(0, ui_key_zero());
      FwSwitchDraw *d = push_array(ui_build_arena(), FwSwitchDraw, 1);
      d->t = t; d->hot = prev->hot_t; d->active = prev->active_t;
      d->track = ui_color_from_name(s("text")); d->track.w = 0.28f;      // off-state track readable on dark and light
      d->accent = ui_color_from_name(s("accent"));
      ui_box_equip_custom_draw(sw, fw_switch_draw, d);
    }
  }
  UI_Signal sig = ui_signal_from_box(row);
  if(ui_clicked(sig)) { *v ^= 1; }
  return sig;
}

typedef struct FwMarkDraw FwMarkDraw;
struct FwMarkDraw { F32 t; F32 hot; Vec4F32 accent; Vec4F32 ring; B32 radio; };

internal UI_BOX_CUSTOM_DRAW(fw_mark_draw)
{
  FwMarkDraw *d = (FwMarkDraw *)user_data;
  Rng2F32 r = box->rect;
  F32 w = r.x1 - r.x0;
  Vec4F32 ring = mix_4f32(d->ring, d->accent, d->t);
  if(d->radio)
  {
    dr_rect(r, ring, w/2.f, 1.5f, 1.f);
    F32 dot = w * 0.5f * d->t;
    Vec2F32 c = center_2f32(r);
    Vec4F32 a = d->accent; a.w *= d->t;
    dr_rect(r2f32p(c.x - dot/2, c.y - dot/2, c.x + dot/2, c.y + dot/2), a, dot/2, 0, 1.f);
  }
  else
  {
    F32 inset = (1.f - d->t) * w * 0.5f;
    Vec4F32 a = d->accent; a.w *= Min(1.f, d->t*1.6f);
    dr_rect(pad_2f32(r, -inset), a, w*0.28f, 0, 1.f);
    dr_rect(r, ring, w*0.28f, 1.5f, 1.f);
    if(d->t > 0.05f)
    {
      F32 size = box->font_size * 0.95f * d->t;
      FNT_Metrics m = fnt_metrics_from_tag_size(app.icons, size);
      Vec2F32 td = fnt_dim_from_tag_size_string(app.icons, size, 0, 0, s(ICON_CHECK));
      Vec2F32 p = {floor_f32((r.x0 + r.x1)/2.f - td.x/2.f), floor_f32((r.y0 + r.y1)/2.f + m.ascent/2.f - m.descent/2.f)};
      dr_text(app.icons, size, 0, 0, FNT_RasterFlag_Smooth, p, v4f32(1, 1, 1, d->t), s(ICON_CHECK));
    }
  }
}

internal UI_Signal
fw_mark_row(B32 on, B32 radio, char *label, char *id, B32 *out_clicked)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  F32 t = ui_anim(ui_key_from_string(key, s("t")), on ? 1.f : 0.f, .rate = fw_rate(30));
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *row = 0;
  UI_CornerRadius(em*0.4f) row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, "###%s", id);
  UI_Parent(row)
  {
    ui_spacer(ui_px(em*0.4f, 1));
    FwVCenter(ui_px(em*1.5f, 1))
    {
      ui_set_next_pref_width(ui_px(em*1.4f, 1));
      ui_set_next_pref_height(ui_px(em*1.4f, 1));
      UI_Box *m = ui_build_box_from_key(0, ui_key_zero());
      FwMarkDraw *d = push_array(ui_build_arena(), FwMarkDraw, 1);
      d->t = t; d->hot = prev->hot_t; d->radio = radio;
      d->accent = ui_color_from_name(s("accent"));
      Vec4F32 ring = ui_color_from_name(s("text")); ring.w = 0.45f + 0.25f*prev->hot_t;
      d->ring = ring;
      ui_box_equip_custom_draw(m, fw_mark_draw, d);
    }
    ui_spacer(ui_px(em*0.6f, 1));
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(label));
  }
  UI_Signal sig = ui_signal_from_box(row);
  *out_clicked = ui_clicked(sig);
  return sig;
}

internal UI_Signal
fw_checkbox(B32 *v, char *label, char *id)
{
  B32 clicked = 0;
  UI_Signal sig = fw_mark_row(*v, 0, label, id, &clicked);
  if(clicked) { *v ^= 1; }
  return sig;
}

internal UI_Signal
fw_radio(int *sel, int idx, char *label, char *id)
{
  B32 clicked = 0;
  UI_Signal sig = fw_mark_row(*sel == idx, 1, label, id, &clicked);
  if(clicked) { *sel = idx; }
  return sig;
}

////////////////////////////////
//~ Sliders

typedef struct FwSliderDraw FwSliderDraw;
struct FwSliderDraw
{
  F32 t0, t1;               // t0 only used by range sliders
  B32 range;
  F32 hot, active;
  Vec4F32 track, accent, text;
  String8 value_text;
};

internal void
fw_slider_geom(Rng2F32 r, F32 em, F32 *x0, F32 *x1)
{
  F32 pad = em*0.75f;
  F32 text_w = em*4.6f;
  *x0 = r.x0 + pad;
  *x1 = r.x1 - text_w - pad;
  if(*x1 < *x0 + 10.f) { *x1 = *x0 + 10.f; }
}

internal UI_BOX_CUSTOM_DRAW(fw_slider_draw)
{
  FwSliderDraw *d = (FwSliderDraw *)user_data;
  F32 em = box->font_size;
  Rng2F32 r = box->rect;
  F32 x0, x1;
  fw_slider_geom(r, em, &x0, &x1);
  F32 cy = (r.y0 + r.y1) / 2.f;
  F32 th = em*0.34f;
  dr_rect(r2f32p(x0, cy - th/2, x1, cy + th/2), d->track, th/2, 0, 1.f);
  F32 fx0 = d->range ? mix_1f32(x0, x1, d->t0) : x0;
  F32 fx1 = mix_1f32(x0, x1, d->t1);
  dr_rect(r2f32p(fx0, cy - th/2, fx1, cy + th/2), d->accent, th/2, 0, 1.f);
  F32 kr = em * (0.52f + 0.12f*d->hot + 0.10f*d->active);
  F32 knob_x[2] = {mix_1f32(x0, x1, d->t0), fx1};
  for(int i = d->range ? 0 : 1; i < 2; i += 1)
  {
    dr_rect(r2f32p(knob_x[i] - kr - 1, cy - kr + 1, knob_x[i] + kr + 1, cy + kr + 3), v4f32(0, 0, 0, 0.35f), kr, 0, 4.f);
    dr_rect(r2f32p(knob_x[i] - kr, cy - kr, knob_x[i] + kr, cy + kr), v4f32(1, 1, 1, 1), kr, 0, 1.f);
    dr_rect(r2f32p(knob_x[i] - kr, cy - kr, knob_x[i] + kr, cy + kr), d->accent, kr, 2.f, 1.f);    // accent ring: visible on light themes too
    dr_rect(r2f32p(knob_x[i] - kr*0.38f, cy - kr*0.38f, knob_x[i] + kr*0.38f, cy + kr*0.38f), d->accent, kr, 0, 1.f);
  }
  F32 size = em * 0.88f;
  FNT_Metrics m = fnt_metrics_from_tag_size(app.mono, size);
  Vec2F32 td = fnt_dim_from_tag_size_string(app.mono, size, 0, 0, d->value_text);
  Vec2F32 p = {floor_f32(r.x1 - td.x - 2.f), floor_f32(cy + m.ascent/2.f - m.descent/2.f)};
  dr_text(app.mono, size, 0, 0, FNT_RasterFlag_Smooth, p, d->text, d->value_text);
}

internal F32
fw_slider_pct_from_mouse(UI_Box *box)
{
  F32 x0, x1;
  fw_slider_geom(box->rect, ui_top_font_size(), &x0, &x1);
  return fw_clamp01((ui_mouse().x - x0) / (x1 - x0));
}

internal UI_Signal
fw_slider(F32 *v, F32 lo, F32 hi, F32 step, char *id, char *fmt)
{
  Temp scratch = scratch_begin(0, 0);
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_Clickable, "###%s", id);
  UI_Signal sig = ui_signal_from_box(box);
  if(ui_dragging(sig) || ui_pressed(sig))
  {
    F32 p = fw_slider_pct_from_mouse(box);
    F32 nv = lo + p * (hi - lo);
    if(step > 0) { nv = round_f32(nv / step) * step; }
    *v = Clamp(lo, nv, hi);
  }
  FwSliderDraw *d = push_array(ui_build_arena(), FwSliderDraw, 1);
  d->t1 = fw_clamp01((*v - lo) / (hi - lo));
  d->t1 = ui_anim(ui_key_from_string(key, s("t")), d->t1, .rate = fw_rate(60), .initial = d->t1);
  d->hot = prev->hot_t; d->active = prev->active_t;
  d->track = ui_color_from_name(s("track"));
  d->accent = ui_color_from_name(s("accent"));
  d->text = ui_color_from_name(s("text"));
  d->value_text = push_str8f(ui_build_arena(), fmt, *v);
  ui_box_equip_custom_draw(box, fw_slider_draw, d);
  scratch_end(scratch);
  return sig;
}

internal UI_Signal
fw_range_slider(F32 *lo_v, F32 *hi_v, F32 lo, F32 hi, char *id)
{
  UI_Key key = fw_key(id);
  UI_Box *prev = ui_box_from_key(key);
  UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_Clickable, "###%s", id);
  UI_Signal sig = ui_signal_from_box(box);
  if(ui_pressed(sig))
  {
    F32 p = fw_slider_pct_from_mouse(box);
    F32 pl = (*lo_v - lo) / (hi - lo), ph = (*hi_v - lo) / (hi - lo);
    U32 which = (abs_f32(p - pl) < abs_f32(p - ph)) ? 0 : 1;
    if(abs_f32(pl - ph) < 0.001f) { which = (p < pl) ? 0 : 1; }
    ui_store_drag_struct(&which);
  }
  if(ui_dragging(sig))
  {
    U32 which = *ui_get_drag_struct(U32);
    F32 nv = lo + fw_slider_pct_from_mouse(box) * (hi - lo);
    if(which == 0) { *lo_v = Clamp(lo, nv, *hi_v); }
    else           { *hi_v = Clamp(*lo_v, nv, hi); }
  }
  FwSliderDraw *d = push_array(ui_build_arena(), FwSliderDraw, 1);
  d->range = 1;
  d->t0 = ui_anim(ui_key_from_string(key, s("t0")), fw_clamp01((*lo_v - lo) / (hi - lo)), .rate = fw_rate(60), .initial = fw_clamp01((*lo_v - lo) / (hi - lo)));
  d->t1 = ui_anim(ui_key_from_string(key, s("t1")), fw_clamp01((*hi_v - lo) / (hi - lo)), .rate = fw_rate(60), .initial = fw_clamp01((*hi_v - lo) / (hi - lo)));
  d->hot = prev->hot_t; d->active = prev->active_t;
  d->track = ui_color_from_name(s("track"));
  d->accent = ui_color_from_name(s("accent"));
  d->text = ui_color_from_name(s("text"));
  d->value_text = push_str8f(ui_build_arena(), "%.0f-%.0f", *lo_v, *hi_v);
  ui_box_equip_custom_draw(box, fw_slider_draw, d);
  return sig;
}

////////////////////////////////
//~ Stepper (hold to repeat, shift = x10)

internal UI_Signal
fw_stepper(S64 *v, S64 lo, S64 hi, char *id)
{
  F32 em = ui_top_font_size();
  Temp scratch = scratch_begin(0, 0);
  UI_Signal result = {0};
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *row = ui_build_box_from_stringf(0, "###%s", id);
  UI_Parent(row)
  {
    for(int dir = -1; dir <= 1; dir += 2)
    {
      if(dir == 1)
      {
        UI_PrefWidth(ui_px(em*4.2f, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_Font(app.mono) UI_TextAlignment(UI_TextAlign_Center)
          ui_labelf("%I64d", *v);
      }
      FwVCenter(ui_px(em*2.2f, 1))
      {
        UI_Signal sig = fw_icon_button(dir < 0 ? ICON_MINUS : ICON_ADD, dir < 0 ? "step_dec" : "step_inc", 1.9f);
        S64 amt = (sig.event_flags & WM_Modifier_Shift) ? 10 : 1;
        U64 now = now_time_us();
        if(ui_pressed(sig)) { *v += dir*amt; app.stepper_repeat_us = now + 420000; }
        else if(ui_dragging(sig) && now >= app.stepper_repeat_us) { *v += dir*amt; app.stepper_repeat_us = now + 55000; app.frames_requested = 4; }
        *v = Clamp(lo, *v, hi);
        if(ui_dragging(sig)) { app.frames_requested = 4; }
        result = sig;
      }
    }
  }
  ui_signal_from_box(row);
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ Segmented control (sliding pill), chips, rating

typedef struct FwSegDraw FwSegDraw;
struct FwSegDraw { F32 t; int n; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(fw_seg_draw)
{
  FwSegDraw *d = (FwSegDraw *)user_data;
  F32 em = box->font_size;
  Rng2F32 r = box->rect;
  F32 pad = em*0.25f;
  F32 iw = ((r.x1 - r.x0) - 2*pad) / (F32)d->n;
  F32 x = r.x0 + pad + d->t * iw;
  dr_rect(r2f32p(x, r.y0 + pad, x + iw, r.y1 - pad), d->accent, box->corner_radii[Corner_00] - pad*0.5f, 0, 1.f);
}

internal UI_Signal
fw_segmented(int *sel, char **labels, int n, char *id)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  F32 t = ui_anim(ui_key_from_string(key, s("t")), (F32)*sel, .rate = fw_rate(40), .initial = (F32)*sel);
  ui_push_tag(s("secondary_button"));
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *box = 0;
  UI_CornerRadius(em*0.55f)
    box = ui_build_box_from_stringf(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, "###%s", id);
  FwSegDraw *d = push_array(ui_build_arena(), FwSegDraw, 1);
  d->t = t; d->n = n;
  d->accent = ui_color_from_tags_key_name(ui_top_tags_key(), s("accent"));
  ui_box_equip_custom_draw(box, fw_seg_draw, d);
  UI_Signal result = {0};
  UI_Parent(box)
  {
    ui_spacer(ui_px(em*0.25f, 1));
    for(int i = 0; i < n; i += 1)
    {
      B32 selected = (i == *sel);
      ui_push_tag(selected ? s("accent_button") : s("weak"));
      ui_set_next_pref_width(ui_pct(1.f/(F32)n, 0));
      ui_set_next_pref_height(ui_pct(1, 0));
      ui_set_next_text_alignment(UI_TextAlign_Center);
      UI_Box *item = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText, "%s###%s_%d", labels[i], id, i);
      UI_Signal sig = ui_signal_from_box(item);
      ui_pop_tag();
      if(ui_clicked(sig)) { *sel = i; result = sig; }
    }
    ui_spacer(ui_px(em*0.25f, 1));
  }
  ui_signal_from_box(box);
  ui_pop_tag();
  return result;
}

internal void
fw_chips(U32 *mask, char **labels, int n, char *id)
{
  F32 em = ui_top_font_size();
  UI_Row
  {
    for(int i = 0; i < n; i += 1)
    {
      B32 on = !!(*mask & (1u << i));
      ui_push_tag(on ? s("accent_button") : s("secondary_button"));
      ui_set_next_pref_width(ui_text_dim(em*0.9f, 0.6f));
      ui_set_next_pref_height(ui_px(em*1.9f, 1));
      ui_set_next_text_alignment(UI_TextAlign_Center);
      UI_Box *b = 0;
      UI_CornerRadius(em*0.95f)
        b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawActiveEffects, "%s###%s_%d", labels[i], id, i);
      UI_Signal sig = ui_signal_from_box(b);
      ui_pop_tag();
      if(ui_clicked(sig)) { *mask ^= (1u << i); }
      ui_spacer(ui_px(em*0.45f, 1));
    }
  }
}

internal void
fw_rating(int *rating, char *id)
{
  F32 em = ui_top_font_size();
  int hover = -1;
  UI_Box *row = 0;
  ui_set_next_child_layout_axis(Axis2_X);
  row = ui_build_box_from_stringf(0, "###%s", id);
  UI_Parent(row)
  {
    for(int i = 0; i < 5; i += 1)
    {
      UI_Key k = ui_key_from_stringf(ui_active_seed_key(), "###%s_%d", id, i);
      UI_Box *pb = ui_box_from_key(k);
      B32 lit = (i < *rating);
      F32 grow = ui_anim(ui_key_from_string(k, s("g")), pb->hot_t, .rate = fw_rate(40));
      if(!lit) { ui_push_tag(s("weak")); }
      ui_set_next_pref_width(ui_px(em*2.0f, 1));
      ui_set_next_pref_height(ui_pct(1, 0));
      ui_set_next_font(app.icons);
      ui_set_next_font_size(em*(1.0f + 0.35f*grow));
      ui_set_next_text_alignment(UI_TextAlign_Center);
      UI_Box *b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DisableTextTrunc, "%s###%s_%d", lit ? ICON_RADIO_ON : ICON_RADIO, id, i);
      UI_Signal sig = ui_signal_from_box(b);
      if(!lit) { ui_pop_tag(); }
      if(ui_clicked(sig)) { *rating = (*rating == i + 1) ? 0 : i + 1; }
      if(ui_hovering(sig)) { hover = i; }
    }
  }
  ui_signal_from_box(row);
  (void)hover;
}

////////////////////////////////
//~ Progress

typedef struct FwBarDraw FwBarDraw;
struct FwBarDraw { F32 t; B32 indeterminate; F32 phase; Vec4F32 track; Vec4F32 fill; };

internal UI_BOX_CUSTOM_DRAW(fw_bar_draw)
{
  FwBarDraw *d = (FwBarDraw *)user_data;
  Rng2F32 r = box->rect;
  F32 w = r.x1 - r.x0;
  F32 radius = (r.y1 - r.y0) * 0.5f;
  dr_rect(r, d->track, radius, 0, 1.f);
  if(d->indeterminate)
  {
    F32 seg = w * 0.32f;
    F32 e = d->phase * d->phase * (3.f - 2.f*d->phase);
    F32 x = mix_1f32(-seg, w, e);
    Rng2F32 f = r2f32p(r.x0 + Max(0.f, x), r.y0, r.x0 + Min(w, x + seg), r.y1);
    if(f.x1 > f.x0) { dr_rect(f, d->fill, radius, 0, 1.f); }
  }
  else if(d->t > 0.002f)
  {
    Rng2F32 f = r; f.x1 = r.x0 + Max(radius*2.f, w * d->t);
    dr_rect(f, d->fill, radius, 0, 1.f);
  }
}

internal void
fw_progress(F32 t, B32 indeterminate, char *id)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  F32 at = ui_anim(ui_key_from_string(key, s("t")), t, .rate = fw_rate(24), .initial = t);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_px(em*0.55f, 1));
  UI_Box *box = ui_build_box_from_stringf(0, "###%s", id);
  FwBarDraw *d = push_array(ui_build_arena(), FwBarDraw, 1);
  d->t = at; d->indeterminate = indeterminate;
  d->phase = app.no_anim ? 0.5f : (F32)(now_time_us() % 1600000) / 1600000.f;
  d->track = ui_color_from_name(s("track"));
  d->fill = ui_color_from_name(s("accent"));
  ui_box_equip_custom_draw(box, fw_bar_draw, d);
  if(indeterminate && !app.no_anim) { app.frames_requested = 4; }
}

////////////////////////////////
//~ Menus (dropdown, menu items) -- all built on the single shared context-menu root

internal B32
fw_menu_item(char *icon, char *label, char *shortcut, B32 checked, F32 width_px)
{
  F32 em = ui_top_font_size();
  ui_set_next_pref_width(ui_px(width_px, 1));
  ui_set_next_pref_height(ui_px(em*2.2f, 1));
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, "###mi_%s", label);
  UI_Parent(b)
  {
    ui_spacer(ui_px(em*0.5f, 1));
    if(checked)    { UI_TagF("weak") fw_icon_label(ICON_CHECK, em*1.6f); }
    else if(icon)  { UI_TagF("weak") fw_icon_label(icon, em*1.6f); }
    else           { ui_spacer(ui_px(em*1.6f, 1)); }
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(label));
    if(shortcut && shortcut[0]) { UI_Font(app.mono) UI_FontSize(em*0.82f) UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak") ui_label(str8_cstring(shortcut)); }
    ui_spacer(ui_px(em*0.7f, 1));
  }
  UI_Signal sig = ui_signal_from_box(b);
  B32 clicked = ui_clicked(sig);
  if(clicked) { ui_ctx_menu_close(); }
  return clicked;
}

internal UI_Signal
fw_dropdown(int *sel, char **items, int n, char *id)
{
  F32 em = ui_top_font_size();
  UI_Key key = fw_key(id);
  UI_Key menu_key = ui_key_from_stringf(ui_key_zero(), "dd_%s", id);
  UI_Box *prev = ui_box_from_key(key);
  ui_push_tag(s("secondary_button"));
  ui_set_next_child_layout_axis(Axis2_X);
  ui_set_next_squish(fw_squish(prev->active_t * 0.02f));
  UI_Box *btn = 0;
  UI_CornerRadius(em*0.45f)
    btn = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects, "###%s", id);
  UI_Parent(btn)
  {
    ui_spacer(ui_px(em*0.8f, 1));
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(items[*sel]));
    UI_TagF("weak") fw_icon_label(ui_ctx_menu_is_open(menu_key) ? ICON_UP : ICON_DOWN, em*1.8f);
  }
  UI_Signal sig = ui_signal_from_box(btn);
  ui_pop_tag();
  if(ui_clicked(sig)) { ui_ctx_menu_open(menu_key, key, v2f32(0, dim_2f32(btn->rect).y + 4.f)); }
  F32 w = Max(dim_2f32(prev->rect).x, em*10.f);
  UI_CtxMenu(menu_key)
  {
    for(int i = 0; i < n; i += 1)
    {
      if(fw_menu_item(0, items[i], 0, i == *sel, w)) { *sel = i; }
    }
  }
  return sig;
}

////////////////////////////////
//~ Text input (ui_line_edit + the event/focus plumbing it needs)

internal void
fw_field_set(TextField *f, char *text)
{
  U64 n = Min(cstring8_length((U8 *)text), sizeof(f->buf));
  MemoryCopy(f->buf, text, n);
  f->size = n; f->cursor = n; f->mark = n;
}

internal UI_Signal
fw_input(TextField *f, char *placeholder, char *id, char *icon, B32 invalid)
{
  F32 em = ui_top_font_size();
  Temp scratch = scratch_begin(0, 0);
  UI_Key key = fw_key(id);
  B32 was_active = ui_is_key_auto_focus_active(key);
  if(app.focus_request == f)
  {
    ui_set_auto_focus_active_key(key);
    f->cursor = f->size; f->mark = 0;
    app.focus_request = 0;
    app.frames_requested = 4;
  }
  U64 h0 = u64_hash_from_str8(str8(f->buf, f->size));
  ui_set_next_focus_active(UI_FocusKind_On);     // the container of a line edit must carry the focus flag (cursor alpha)
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *wrapper = ui_build_box_from_key(0, ui_key_zero());
  UI_Signal sig = {0};
  UI_Parent(wrapper)
  {
    if(icon) { UI_TagF("weak") fw_icon_label(icon, em*2.0f); }
    B32 weak = (f->size == 0 && !was_active);
    if(weak)    { ui_push_tag(s("weak")); }
    if(invalid) { ui_push_tag(s("bad")); }
    ui_push_tag(s("input"));
    UI_CornerRadius(em*0.45f) UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))
      sig = ui_line_edit(&f->cursor, &f->mark, f->buf, sizeof(f->buf), &f->size, str8(f->buf, f->size), push_str8f(scratch.arena, "%s###%s", placeholder, id));
    ui_pop_tag();
    if(invalid) { ui_pop_tag(); }
    if(weak)    { ui_pop_tag(); }
    if(f->size > 0)
    {
      ui_spacer(ui_px(em*0.3f, 1));
      FwVCenter(ui_px(em*2.0f, 1))
      {
        char clear_id[64];
        raddbg_snprintf(clear_id, sizeof(clear_id), "%s_clear", id);
        UI_Signal c = fw_icon_button(ICON_X, clear_id, 1.7f);
        if(ui_clicked(c)) { f->size = f->cursor = f->mark = 0; }
      }
    }
  }
  if(!was_active && ui_pressed(sig)) { ui_set_auto_focus_active_key(key); f->cursor = f->size; f->mark = f->size; }
  U64 h1 = u64_hash_from_str8(str8(f->buf, f->size));
  f->changed = (h0 != h1);
  f->last_hash = h1;
  scratch_end(scratch);
  return sig;
}

////////////////////////////////
//~ Tabs with sliding underline

typedef struct FwTabDraw FwTabDraw;
struct FwTabDraw { F32 t; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(fw_tab_draw)
{
  FwTabDraw *d = (FwTabDraw *)user_data;
  if(d->t < 0.01f) { return; }
  Rng2F32 r = box->rect;
  F32 w = (r.x1 - r.x0) * d->t;
  F32 cx = (r.x0 + r.x1) / 2.f;
  dr_rect(r2f32p(cx - w/2.f, r.y1 - 3.f, cx + w/2.f, r.y1), d->accent, 1.5f, 0, 1.f);
}

internal void
fw_tabs(int *sel, char **labels, char **icons, int n, char *id)
{
  F32 em = ui_top_font_size();
  for(int i = 0; i < n; i += 1)
  {
    B32 selected = (*sel == i);
    UI_Key k = ui_key_from_stringf(ui_active_seed_key(), "###%s_%d", id, i);
    F32 t = ui_anim(ui_key_from_string(k, s("t")), selected ? 1.f : 0.f, .rate = fw_rate(34));
    if(!selected) { ui_push_tag(s("weak")); }
    ui_set_next_child_layout_axis(Axis2_X);
    ui_set_next_pref_width(ui_children_sum(1));
    ui_set_next_pref_height(ui_pct(1, 0));
    UI_Box *tab = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, "###%s_%d", id, i);
    FwTabDraw *d = push_array(ui_build_arena(), FwTabDraw, 1);
    d->t = t; d->accent = ui_color_from_tags_key_name(ui_top_tags_key(), s("accent"));
    ui_box_equip_custom_draw(tab, fw_tab_draw, d);
    UI_Parent(tab)
    {
      ui_spacer(ui_px(em*1.0f, 1));
      fw_icon_label(icons[i], em*1.6f);
      UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(labels[i]));
      ui_spacer(ui_px(em*1.0f, 1));
    }
    UI_Signal sig = ui_signal_from_box(tab);
    if(!selected) { ui_pop_tag(); }
    if(ui_clicked(sig)) { *sel = i; }
  }
}
