////////////////////////////////
//~ gui_todo: a todo app built on the RAD Debugger UI stack
//
// Features: add / toggle / inline-edit / delete tasks, priorities (click the badge),
// filters + live search, sort-by-priority, progress bar, right-click context menu,
// tooltips, animated light/dark theme, keyboard shortcuts, auto-saved to disk.
//
// Shortcuts: Enter add/commit | Esc cancel | Ctrl+N new task | Ctrl+F search | Ctrl+T theme
//            double-click a task to rename | right-click for more | Tab/Enter keyboard nav

#define BUILD_TITLE "gui_todo"
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

////////////////////////////////
//~ Icons (codepoints of data/icons.ttf, from raddbg's rd_icon_kind_text_table)

#define ICON_TRASH   "3"
#define ICON_PENCIL  "P"
#define ICON_CHECK   "V"
#define ICON_ADD     "+"
#define ICON_PALETTE "E"
#define ICON_FIND    "N"
#define ICON_X       "x"

////////////////////////////////
//~ Types

typedef enum Prio { Prio_Low, Prio_Med, Prio_High, Prio_COUNT } Prio;
global char *prio_label[Prio_COUNT] = {"LOW", "MED", "HIGH"};
global char *prio_tag[Prio_COUNT]   = {"prio_low", "prio_med", "prio_high"};

typedef enum Filter { Filter_All, Filter_Active, Filter_Done, Filter_COUNT } Filter;
global char *filter_label[Filter_COUNT] = {"All", "Active", "Done"};

typedef struct Task Task;
struct Task
{
  Task *next;
  Task *prev;
  U64 id;
  U8 title[256];
  U64 title_size;
  B32 done;
  Prio prio;
  U32 created_unix;
  U32 done_unix;
};

typedef struct TextField TextField;
struct TextField
{
  U8 buf[256];
  U64 size;
  U64 cursor;
  U64 mark;
};

typedef struct App App;
struct App
{
  Arena *arena;
  WM_Window window;
  R_Handle r_window;
  UI_State *ui;
  FNT_Tag font;
  FNT_Tag icons;
  DR_Bucket *bucket;

  // frame pacing
  U64 frame_index;
  U64 frames_requested;
  U64 frame_depth;

  // themes
  UI_Theme themes[2];   // 0 = dark, 1 = light
  B32 light_theme;

  // tasks
  Task *first;
  Task *last;
  Task *free_task;
  U64 next_id;

  // view state
  Filter filter;
  B32 sort_by_prio;
  TextField add_field;
  TextField search_field;
  U64 edit_id;            // task being renamed (0 = none)
  TextField edit_field;
  U64 edit_frames;
  U64 ctx_task_id;
  U64 rename_request;
  TextField *focus_request;
  B32 want_quit;

  // per-frame bookkeeping
  UI_Key page_key;
  UI_Key nav_hot_key;
  char last_title[96];
  B32 dirty;
};
global App app = {0};

////////////////////////////////
//~ Theme (flat list of tags -> color; see references/ui-system.md)

typedef struct ThemeEntry ThemeEntry;
struct ThemeEntry { char *tags; U32 rgba; };

global ThemeEntry theme_dark[] =
{
  {"background",                  0x14161bff},
  {"text",                        0xe8eaedff},
  {"weak text",                   0x8b93a1ff},
  {"border",                      0x2a2f3aff},
  {"hover",                       0xffffffff},
  {"drop_shadow",                 0x00000090},
  {"selection",                   0x6c8cff55},
  {"cursor",                      0xe8eaedff},
  {"focus overlay",               0xffffff0c},
  {"focus border",                0x6c8cffff},
  {"accent",                      0x6c8cffff},
  {"track",                       0x252a34ff},
  {"row background",              0x1c1f26ff},
  {"row border",                  0x272b34ff},
  {"input background",            0x1c1f26ff},
  {"input border",                0x303643ff},
  {"accent_button background",    0x6c8cffff},
  {"accent_button text",          0x0b0d12ff},
  {"accent_button border",        0x6c8cffff},
  {"prio_low text",               0x6bbf8aff},
  {"prio_med text",               0xe0b45aff},
  {"prio_high text",              0xef6b73ff},
  {"floating background",         0x20242cff},
  {"floating text",               0xe8eaedff},
  {"floating border",             0x343a46ff},
};

global ThemeEntry theme_light[] =
{
  {"background",                  0xf3f4f7ff},
  {"text",                        0x1f2430ff},
  {"weak text",                   0x6b7380ff},
  {"border",                      0xd9dde4ff},
  {"hover",                       0x000000ff},
  {"drop_shadow",                 0x00000030},
  {"selection",                   0x3b5bdb44},
  {"cursor",                      0x1f2430ff},
  {"focus overlay",               0x0000000c},
  {"focus border",                0x3b5bdbff},
  {"accent",                      0x3b5bdbff},
  {"track",                       0xe0e4ebff},
  {"row background",              0xffffffff},
  {"row border",                  0xe3e6ebff},
  {"input background",            0xffffffff},
  {"input border",                0xcfd5dcff},
  {"accent_button background",    0x3b5bdbff},
  {"accent_button text",          0xffffffff},
  {"accent_button border",        0x3b5bdbff},
  {"prio_low text",               0x2f9e5aff},
  {"prio_med text",               0xc98a10ff},
  {"prio_high text",              0xd6333fff},
  {"floating background",         0xffffffff},
  {"floating text",               0x1f2430ff},
  {"floating border",             0xd0d5dcff},
};

internal void
app_build_theme(UI_Theme *theme, ThemeEntry *entries, U64 count)
{
  theme->patterns_count = count;
  theme->patterns = push_array(app.arena, UI_ThemePattern, count);
  for EachIndex(idx, count)
  {
    String8List tags = str8_split(app.arena, str8_cstring(entries[idx].tags), (U8 *)" ", 1, 0);
    theme->patterns[idx].tags   = str8_array_from_list(app.arena, &tags);
    theme->patterns[idx].linear = linear_from_srgba(rgba_from_u32(entries[idx].rgba));
  }
}

////////////////////////////////
//~ Tasks & persistence

internal Task *
task_from_id(U64 id)
{
  for(Task *t = app.first; t != 0; t = t->next) { if(t->id == id) { return t; } }
  return 0;
}

internal Task *
task_new(String8 title, B32 push_front)
{
  Task *t = app.free_task;
  if(t != 0) { SLLStackPop(app.free_task); }
  else       { t = push_array_no_zero(app.arena, Task, 1); }
  MemoryZeroStruct(t);
  t->id = ++app.next_id;
  t->title_size = Min(title.size, sizeof(t->title));
  MemoryCopy(t->title, title.str, t->title_size);
  t->created_unix = now_time_unix();
  t->prio = Prio_Med;
  if(push_front) { DLLPushFront(app.first, app.last, t); }
  else           { DLLPushBack(app.first, app.last, t); }
  return t;
}

internal void
task_release(Task *t)
{
  DLLRemove(app.first, app.last, t);
  SLLStackPush(app.free_task, t);
}

internal String8
app_data_dir(Arena *arena)
{
  return push_str8f(arena, "%S/gui_todo", get_process_info()->user_program_config_data_path);
}

internal void
app_save(void)
{
  Temp scratch = scratch_begin(0, 0);
  String8List lines = {0};
  str8_list_pushf(scratch.arena, &lines, "#gui_todo1\t%d\t%d\n", app.light_theme, app.sort_by_prio);
  // stored oldest-first so that loading with push-front restores the same order
  for(Task *t = app.last; t != 0; t = t->prev)
  {
    String8 title = push_str8_copy(scratch.arena, str8(t->title, t->title_size));
    for EachIndex(i, title.size) { if(title.str[i] == '\t' || title.str[i] == '\n' || title.str[i] == '\r') { title.str[i] = ' '; } }
    str8_list_pushf(scratch.arena, &lines, "%d\t%d\t%u\t%u\t%S\n", t->done, (int)t->prio, t->created_unix, t->done_unix, title);
  }
  String8 data = str8_list_join(scratch.arena, &lines, 0);
  String8 dir = app_data_dir(scratch.arena);
  make_directory(dir);
  write_data_to_file_path(push_str8f(scratch.arena, "%S/todos.tsv", dir), data);
  app.dirty = 0;
  scratch_end(scratch);
}

internal void
app_load(void)
{
  Temp scratch = scratch_begin(0, 0);
  String8 dir  = app_data_dir(scratch.arena);
  String8 data = data_from_file_path(scratch.arena, push_str8f(scratch.arena, "%S/todos.tsv", dir));
  String8List lines = str8_split(scratch.arena, data, (U8 *)"\n", 1, 0);
  for(String8Node *n = lines.first; n != 0; n = n->next)
  {
    String8 line = str8_skip_chop_whitespace(n->string);
    if(line.size == 0) { continue; }
    String8List f = str8_split(scratch.arena, line, (U8 *)"\t", 1, StringSplitFlag_KeepEmpties);
    if(line.str[0] == '#')
    {
      if(f.node_count >= 3)
      {
        app.light_theme  = (u64_from_str8(f.first->next->string, 10) != 0);
        app.sort_by_prio = (u64_from_str8(f.first->next->next->string, 10) != 0);
      }
      continue;
    }
    if(f.node_count < 5) { continue; }
    String8Node *c = f.first;
    B32 done = (u64_from_str8(c->string, 10) != 0); c = c->next;
    U64 prio = u64_from_str8(c->string, 10);        c = c->next;
    U32 created = (U32)u64_from_str8(c->string, 10); c = c->next;
    U32 done_at = (U32)u64_from_str8(c->string, 10); c = c->next;
    Task *t = task_new(c->string, 1);
    t->done = done;
    t->prio = (Prio)Min(prio, Prio_High);
    t->created_unix = created;
    t->done_unix = done_at;
  }
  scratch_end(scratch);
}

internal void
app_counts(U64 *total, U64 *done)
{
  *total = *done = 0;
  for(Task *t = app.first; t != 0; t = t->next) { *total += 1; *done += !!t->done; }
}

internal B32
task_is_visible(Task *t)
{
  if(app.filter == Filter_Active && t->done)  { return 0; }
  if(app.filter == Filter_Done   && !t->done) { return 0; }
  String8 q = str8(app.search_field.buf, app.search_field.size);
  if(q.size != 0 && str8_find_needle(str8(t->title, t->title_size), 0, q, StringMatchFlag_CaseInsensitive) >= t->title_size) { return 0; }
  return 1;
}

internal String8
age_string(Arena *arena, U32 unix_then)
{
  U32 now = now_time_unix();
  U64 d = (now > unix_then) ? (now - unix_then) : 0;
  if(d < 60)        { return s("now"); }
  if(d < 3600)      { return push_str8f(arena, "%I64um", d/60); }
  if(d < 86400)     { return push_str8f(arena, "%I64uh", d/3600); }
  return push_str8f(arena, "%I64ud", d/86400);
}

////////////////////////////////
//~ OS events -> UI events (incl. text-editing semantics)

internal void
app_push_ui_event(Arena *arena, UI_EventList *out, UI_Event *ev)
{
  ui_event_list_push(arena, out, ev);
}

internal void
app_ui_events_from_wm_events(Arena *arena, WM_EventList *wm_events, UI_EventList *out)
{
  for(WM_Event *e = wm_events->first; e != 0; e = e->next)
  {
    B32 ctrl  = !!(e->modifiers & WM_Modifier_Ctrl);
    B32 shift = !!(e->modifiers & WM_Modifier_Shift);
    B32 alt   = !!(e->modifiers & WM_Modifier_Alt);
    if(e->kind == WM_EventKind_WindowClose) { app.want_quit = 1; continue; }

    //- app-level shortcuts (consumed here, never forwarded)
    if(e->kind == WM_EventKind_Press && ctrl && !alt)
    {
      if(e->key == WM_Key_N) { app.focus_request = &app.add_field;    continue; }
      if(e->key == WM_Key_F) { app.focus_request = &app.search_field; continue; }
      if(e->key == WM_Key_T) { app.light_theme ^= 1; app.dirty = 1;   continue; }
    }

    //- raw passthrough (mouse, keys for default navigation, text, scroll, drops)
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
      app_push_ui_event(arena, out, &ev);
    }

    if(e->kind != WM_EventKind_Press) { continue; }

    //- semantic action slots
    if(e->key == WM_Key_Return) { UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Accept; app_push_ui_event(arena, out, &a); }
    if(e->key == WM_Key_Esc)    { UI_Event a = zero_struct; a.kind = UI_EventKind_Press; a.slot = UI_EventActionSlot_Cancel; app_push_ui_event(arena, out, &a); }

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
        app_push_ui_event(arena, out, &a);
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
    if(t.kind != UI_EventKind_Null) { t.timestamp_us = e->timestamp_us; app_push_ui_event(arena, out, &t); }
  }
}

////////////////////////////////
//~ Custom draws (data is built per frame in the UI build arena)

typedef struct CheckDraw CheckDraw;
struct CheckDraw { F32 t; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(app_check_draw)
{
  CheckDraw *d = (CheckDraw *)user_data;
  if(d->t < 0.01f) { return; }
  Rng2F32 r = box->rect;
  Vec2F32 dim = dim_2f32(r);
  F32 inset = (1.f - d->t) * dim.x * 0.5f;
  Vec4F32 c = d->accent; c.w *= Min(1.f, d->t*1.5f);
  R_Rect2DInst *inst = dr_rect(pad_2f32(r, -inset), c, box->corner_radii[Corner_00], 0, 1.f);
  (void)inst;
  F32 size = box->font_size * 0.95f * d->t;
  FNT_Metrics m = fnt_metrics_from_tag_size(app.icons, size);
  Vec2F32 td = fnt_dim_from_tag_size_string(app.icons, size, 0, 0, s(ICON_CHECK));
  Vec2F32 p = { floor_f32((r.x0 + r.x1)/2.f - td.x/2.f), floor_f32((r.y0 + r.y1)/2.f + m.ascent/2.f - m.descent/2.f) };
  dr_text(app.icons, size, 0, 0, FNT_RasterFlag_Smooth, p, v4f32(1, 1, 1, d->t), s(ICON_CHECK));
}

typedef struct BarDraw BarDraw;
struct BarDraw { F32 t; Vec4F32 track; Vec4F32 fill; };

internal UI_BOX_CUSTOM_DRAW(app_bar_draw)
{
  BarDraw *d = (BarDraw *)user_data;
  Rng2F32 r = box->rect;
  F32 radius = (r.y1 - r.y0) * 0.5f;
  dr_rect(r, d->track, radius, 0, 1.f);
  if(d->t > 0.002f)
  {
    Rng2F32 f = r; f.x1 = r.x0 + Max(radius*2.f, (r.x1 - r.x0) * d->t);
    dr_rect(f, d->fill, radius, 0, 1.f);
  }
}

typedef struct TabDraw TabDraw;
struct TabDraw { F32 t; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(app_tab_draw)
{
  TabDraw *d = (TabDraw *)user_data;
  if(d->t < 0.01f) { return; }
  Rng2F32 r = box->rect;
  F32 w = (r.x1 - r.x0) * d->t;
  F32 cx = (r.x0 + r.x1) / 2.f;
  dr_rect(r2f32p(cx - w/2.f, r.y1 - 3.f, cx + w/2.f, r.y1 - 1.f), d->accent, 1.f, 0, 1.f);
}

typedef struct RowDraw RowDraw;
struct RowDraw { F32 hover_t; Vec4F32 hover; };

internal UI_BOX_CUSTOM_DRAW(app_row_draw)
{
  RowDraw *d = (RowDraw *)user_data;
  if(d->hover_t < 0.01f) { return; }
  // NOTE: colors are linear and blending happens in linear space, so overlay alphas must be tiny
  // (raddbg uses 0.01..0.015); 0.05 already looks like a +40/255 brightening.
  Vec4F32 c = d->hover; c.w *= 0.011f * d->hover_t;
  R_Rect2DInst *inst = dr_rect(box->rect, c, 0, 0, 1.f);
  MemoryCopyArray(inst->corner_radii, box->corner_radii);
}

////////////////////////////////
//~ Small layout helpers

internal UI_Box *
app_row_begin(UI_Size w, UI_Size h)
{
  ui_set_next_pref_width(w);
  ui_set_next_pref_height(h);
  return ui_row_begin();
}

internal UI_Box *
app_col_begin(UI_Size w, UI_Size h)
{
  ui_set_next_pref_width(w);
  ui_set_next_pref_height(h);
  return ui_column_begin();
}

#define AppRow(w, h)    DeferLoop(app_row_begin((w), (h)), ui_row_end())
#define AppCol(w, h)    DeferLoop(app_col_begin((w), (h)), ui_column_end())
// a column that vertically centers whatever single child you build inside it
#define AppVCenter(w)   AppCol((w), ui_pct(1, 0)) UI_Padding(ui_pct(1, 0))

internal UI_Signal
app_icon_button(char *icon, F32 em_size, char *id_fmt, U64 id, F32 alpha)
{
  ui_set_next_transparency(1.f - alpha);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_em(em_size, 1.f));
  ui_set_next_font(app.icons);
  ui_set_next_font_size(ui_top_font_size() * 0.9f);
  ui_set_next_text_alignment(UI_TextAlign_Center);
  UI_Box *box = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DisableTextTrunc,
                                          "%s###%s%I64u", icon, id_fmt, id);
  return ui_signal_from_box(box);
}

////////////////////////////////
//~ Text fields

internal UI_Signal
app_text_field(TextField *f, String8 label_with_id, String8 id_only)
{
  UI_Key key = ui_key_from_string(ui_active_seed_key(), id_only);
  B32 was_active = ui_is_key_auto_focus_active(key);
  if(app.focus_request == f)
  {
    ui_set_auto_focus_active_key(key);
    f->cursor = f->size;
    f->mark = 0;
    app.focus_request = 0;
    app.frames_requested = 4;
  }
  // the box *containing* a line edit must carry the focus-active flag: its focus_active_t scales the cursor alpha
  ui_set_next_focus_active(UI_FocusKind_On);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_pct(1, 0));
  UI_Box *wrapper = ui_build_box_from_key(0, ui_key_zero());
  UI_Signal sig = {0};
  UI_Parent(wrapper)
  {
    if(f->size == 0 && !was_active) { ui_push_tag(s("weak")); }
    ui_push_tag(s("input"));
    UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))   // the line edit takes its size from the stacks
      sig = ui_line_edit(&f->cursor, &f->mark, f->buf, sizeof(f->buf), &f->size, str8(f->buf, f->size), label_with_id);
    ui_pop_tag();
    if(f->size == 0 && !was_active) { ui_pop_tag(); }
  }
  if(!was_active && ui_pressed(sig))
  {
    ui_set_auto_focus_active_key(key);
    f->cursor = f->size;
    f->mark = f->size;
  }
  return sig;
}

////////////////////////////////
//~ Task row

internal void
app_task_row(Task *t)
{
  Temp scratch = scratch_begin(0, 0);
  B32 editing = (app.edit_id == t->id);
  F32 em = ui_top_font_size();

  UI_Key row_key = ui_key_from_stringf(ui_active_seed_key(), "###task_%I64u", t->id);
  UI_Box *prev_row = ui_box_from_key(row_key);
  B32 over = contains_2f32(prev_row->rect, ui_mouse());
  F32 hover_t = ui_anim(ui_key_from_string(row_key, s("hover_t")), over ? 1.f : 0.f, .rate = 1 - pow_f32(2, -40.f * ui_dt()));
  F32 done_t  = ui_anim(ui_key_from_string(row_key, s("done_t")),  t->done ? 1.f : 0.f, .rate = 1 - pow_f32(2, -25.f * ui_dt()));

  ui_push_tag(s("row"));
  ui_set_next_child_layout_axis(Axis2_X);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_em(2.9f, 1.f));
  ui_set_next_corner_radius_00(em*0.45f); ui_set_next_corner_radius_01(em*0.45f);
  ui_set_next_corner_radius_10(em*0.45f); ui_set_next_corner_radius_11(em*0.45f);
  UI_Box *row = ui_build_box_from_stringf(UI_BoxFlag_MouseClickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, "###task_%I64u", t->id);
  RowDraw *rd = push_array(ui_build_arena(), RowDraw, 1);
  rd->hover_t = hover_t;
  rd->hover = ui_color_from_name(s("hover"));
  ui_box_equip_custom_draw(row, app_row_draw, rd);

  B32 title_double_clicked = 0;
  B32 any_right_click = 0;
  UI_Parent(row)
  {
    ui_spacer(ui_em(0.9f, 1.f));

    //- checkbox
    UI_CornerRadius(em*0.35f)
    AppVCenter(ui_em(1.5f, 1.f))
    {
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_em(1.5f, 1.f));
      Vec4F32 chk_border = ui_color_from_name(s("text")); chk_border.w = 0.45f;
      ui_set_next_border_color(chk_border);
      UI_Box *chk = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawHotEffects, s("###chk"));
      CheckDraw *cd = push_array(ui_build_arena(), CheckDraw, 1);
      cd->t = done_t;
      cd->accent = ui_color_from_name(s("accent"));
      ui_box_equip_custom_draw(chk, app_check_draw, cd);
      UI_Signal sig = ui_signal_from_box(chk);
      if(ui_clicked(sig))
      {
        t->done ^= 1;
        t->done_unix = t->done ? now_time_unix() : 0;
        app.dirty = 1;
      }
      any_right_click |= ui_right_clicked(sig);
    }
    ui_spacer(ui_em(0.7f, 1.f));

    //- priority badge (click to cycle)
    // NOTE: sizes use `em` captured *outside* the UI_FontSize scope; ui_em() inside it would shrink with the font
    UI_FontSize(em*0.78f) UI_CornerRadius(em*0.7f)
    AppVCenter(ui_px(em*3.6f, 1.f))
    {
      ui_push_tag(s(prio_tag[t->prio]));
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_px(em*1.4f, 1.f));
      ui_set_next_text_alignment(UI_TextAlign_Center);
      Vec4F32 pc = ui_color_from_name(s("text")); pc.w = 0.55f;
      ui_set_next_border_color(pc);
      UI_Box *b = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DisableTextTrunc, push_str8f(scratch.arena, "%s###prio", prio_label[t->prio]));
      UI_Signal sig = ui_signal_from_box(b);
      ui_pop_tag();
      if(ui_clicked(sig)) { t->prio = (Prio)((t->prio + 1) % Prio_COUNT); app.dirty = 1; }
      if(ui_hovering(sig)) UI_Tooltip { ui_label(s("Click to change priority")); }
      any_right_click |= ui_right_clicked(sig);
    }
    ui_spacer(ui_em(0.8f, 1.f));

    //- title (label, or inline editor)
    if(editing)
    {
      app.edit_frames += 1;
      UI_Key edit_key = ui_key_from_string(row->key, s("###title_edit"));
      B32 active = ui_is_key_auto_focus_active(edit_key);
      AppVCenter(ui_pct(1, 0))
      {
        ui_set_next_focus_active(UI_FocusKind_On);
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_em(2.1f, 1.f));
        UI_Box *wrapper = ui_build_box_from_key(0, ui_key_zero());
        UI_Parent(wrapper) UI_TagF("input")
        {
          UI_Signal sig = {0};
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))
            sig = ui_line_edit(&app.edit_field.cursor, &app.edit_field.mark, app.edit_field.buf, sizeof(app.edit_field.buf), &app.edit_field.size,
                               str8(t->title, t->title_size), s("###title_edit"));
          if(ui_committed(sig))
          {
            if(app.edit_field.size > 0)
            {
              t->title_size = Min(app.edit_field.size, sizeof(t->title));
              MemoryCopy(t->title, app.edit_field.buf, t->title_size);
              app.dirty = 1;
            }
            app.edit_id = 0;
          }
        }
      }
      if(app.edit_id == t->id && !active && app.edit_frames > 2) { app.edit_id = 0; }   // Esc / focus lost -> cancel
    }
    else
    {
      if(t->done) { ui_push_tag(s("weak")); }
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_pct(1, 0));
      UI_Box *tb = ui_build_box_from_string(UI_BoxFlag_DrawText|UI_BoxFlag_MouseClickable, s("###title"));
      DR_FStrList fstrs = {0};
      DR_FStrParams p = {tb->font, tb->text_raster_flags, tb->text_color, tb->font_size, 0, 0};
      dr_fstrs_push_new(ui_build_arena(), &fstrs, &p, str8(t->title, t->title_size), .strikethrough_thickness = (done_t > 0.5f) ? 1.5f : 0.f);
      ui_box_equip_display_fstrs(tb, &fstrs);
      UI_Signal sig = ui_signal_from_box(tb);
      if(t->done) { ui_pop_tag(); }
      title_double_clicked = ui_double_clicked(sig);
      any_right_click |= ui_right_clicked(sig);
      if(ui_hovering(sig) && !title_double_clicked) UI_Tooltip { ui_label(s("Double-click to rename")); }
    }

    //- age
    if(!editing)
    {
      UI_PrefWidth(ui_text_dim(8, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak")
        UI_FontSize(em*0.85f) UI_TextAlignment(UI_TextAlign_Right)
        ui_label(age_string(scratch.arena, t->done ? t->done_unix : t->created_unix));
    }

    //- row actions (fade in on hover)
    UI_CornerRadius(em*0.35f)
    {
      AppVCenter(ui_em(1.9f, 1.f))
      {
        UI_Signal sig = app_icon_button(ICON_PENCIL, 1.7f, "edit", t->id, hover_t);
        if(ui_clicked(sig)) { title_double_clicked = 1; }
        if(ui_hovering(sig)) UI_Tooltip { ui_label(s("Rename")); }
      }
      ui_spacer(ui_em(0.15f, 1.f));
      AppVCenter(ui_em(1.9f, 1.f))
      {
        UI_Signal sig = app_icon_button(ICON_TRASH, 1.7f, "del", t->id, hover_t);
        if(ui_clicked(sig))
        {
          if(app.edit_id == t->id) { app.edit_id = 0; }
          task_release(t);
          app.dirty = 1;
          scratch_end(scratch);
          ui_pop_tag();
          return;   // t is gone; the row box is recycled automatically (not touched next frame)
        }
        if(ui_hovering(sig)) UI_Tooltip { ui_label(s("Delete task")); }
      }
    }
    ui_spacer(ui_em(0.6f, 1.f));
  }

  UI_Signal row_sig = ui_signal_from_box(row);
  any_right_click |= ui_right_clicked(row_sig);
  title_double_clicked |= ui_double_clicked(row_sig);
  if(app.rename_request == t->id) { title_double_clicked = 1; app.rename_request = 0; }
  ui_pop_tag();

  if(any_right_click)
  {
    app.ctx_task_id = t->id;
    ui_ctx_menu_open(ui_key_from_string(ui_key_zero(), s("task_ctx")), row->key, sub_2f32(ui_mouse(), row->rect.p0));
  }
  if(title_double_clicked && !editing)
  {
    // start renaming: load the title into the edit buffer and focus the editor (key must match the one built above)
    app.edit_id = t->id;
    app.edit_frames = 0;
    app.edit_field.size = Min(t->title_size, sizeof(app.edit_field.buf));
    MemoryCopy(app.edit_field.buf, t->title, app.edit_field.size);
    app.edit_field.cursor = app.edit_field.size;
    app.edit_field.mark = 0;
    ui_set_auto_focus_active_key(ui_key_from_string(row->key, s("###title_edit")));
    ui_kill_action();
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ UI build

internal void
app_build_ui(void)
{
  Temp scratch = scratch_begin(0, 0);
  F32 em = ui_top_font_size();
  U64 total = 0, done = 0;
  app_counts(&total, &done);

  //- the page: focus root + default keyboard navigation (Tab / arrows / Enter), flagged only on this box
  ui_set_next_focus_hot(UI_FocusKind_On);
  ui_set_next_focus_active(UI_FocusKind_On);
  ui_set_next_child_layout_axis(Axis2_Y);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_pct(1, 0));
  UI_Box *page = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DefaultFocusNav, s("page"));
  app.page_key = page->key;
  app.nav_hot_key = page->default_nav_focus_hot_key;

  UI_Parent(page)
  {
    ui_spacer(ui_em(1.2f, 1.f));
    AppRow(ui_pct(1, 0), ui_pct(1, 0)) UI_Padding(ui_em(1.6f, 1.f))
    AppCol(ui_pct(1, 0), ui_pct(1, 0))
    {
      //- header: title, stats, theme toggle
      AppRow(ui_pct(1, 0), ui_em(3.2f, 1.f))
      {
        UI_FontSize(em*1.7f) UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) ui_label(s("Tasks"));
        ui_spacer(ui_em(0.8f, 1.f));
        UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak")
          ui_labelf("%I64u left  ·  %I64u done", total - done, done);
        ui_spacer(ui_pct(1, 0));
        UI_CornerRadius(em*0.45f) AppVCenter(ui_em(2.4f, 1.f))
        {
          UI_Signal sig = app_icon_button(ICON_PALETTE, 2.3f, "theme", 0, 1.f);
          if(ui_clicked(sig)) { app.light_theme ^= 1; app.dirty = 1; }
          if(ui_hovering(sig)) UI_Tooltip { ui_label(s("Toggle light / dark  (Ctrl+T)")); }
        }
      }

      //- progress bar
      {
        F32 target = total ? (F32)done / (F32)total : 0.f;
        F32 t = ui_anim(ui_key_from_string(page->key, s("progress")), target, .rate = 1 - pow_f32(2, -20.f * ui_dt()));
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_em(0.5f, 1.f));
        UI_Box *bar = ui_build_box_from_string(0, s("###progress"));
        BarDraw *bd = push_array(ui_build_arena(), BarDraw, 1);
        bd->t = t;
        bd->track = ui_color_from_name(s("track"));
        bd->fill = ui_color_from_name(s("accent"));
        ui_box_equip_custom_draw(bar, app_bar_draw, bd);
      }
      ui_spacer(ui_em(1.1f, 1.f));

      //- add row
      UI_CornerRadius(em*0.45f)
      AppRow(ui_pct(1, 0), ui_em(2.6f, 1.f))
      {
        UI_Signal sig = app_text_field(&app.add_field, s("What needs to be done?  (Ctrl+N)###add_input"), s("###add_input"));
        if(ui_committed(sig))
        {
          String8 title = str8_skip_chop_whitespace(str8(app.add_field.buf, app.add_field.size));
          if(title.size > 0)
          {
            task_new(title, 1);
            app.dirty = 1;
          }
          app.add_field.size = app.add_field.cursor = app.add_field.mark = 0;
          app.focus_request = &app.add_field;
        }
        ui_spacer(ui_em(0.6f, 1.f));
        ui_push_tag(s("accent_button"));
        ui_set_next_pref_width(ui_text_dim(18, 1));
        ui_set_next_pref_height(ui_pct(1, 0));
        ui_set_next_text_alignment(UI_TextAlign_Center);
        UI_Box *add = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects, s("Add###add_btn"));
        UI_Signal asig = ui_signal_from_box(add);
        ui_pop_tag();
        if(ui_clicked(asig))
        {
          String8 title = str8_skip_chop_whitespace(str8(app.add_field.buf, app.add_field.size));
          if(title.size > 0) { task_new(title, 1); app.dirty = 1; }
          app.add_field.size = app.add_field.cursor = app.add_field.mark = 0;
          app.focus_request = &app.add_field;
        }
      }
      ui_spacer(ui_em(0.9f, 1.f));

      //- filter tabs + search
      AppRow(ui_pct(1, 0), ui_em(2.4f, 1.f))
      {
        U64 counts[Filter_COUNT] = {total, total - done, done};
        for EachIndex(fi, Filter_COUNT)
        {
          B32 selected = (app.filter == (Filter)fi);
          F32 t = ui_anim(ui_key_from_stringf(page->key, "tab_%I64u", fi), selected ? 1.f : 0.f, .rate = 1 - pow_f32(2, -30.f * ui_dt()));
          if(!selected) { ui_push_tag(s("weak")); }
          ui_set_next_pref_width(ui_text_dim(14, 1));
          ui_set_next_pref_height(ui_pct(1, 0));
          ui_set_next_text_alignment(UI_TextAlign_Center);
          UI_Box *tab = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawBackground,
                                                 push_str8f(scratch.arena, "%s  %I64u###tab_%I64u", filter_label[fi], counts[fi], fi));
          TabDraw *td = push_array(ui_build_arena(), TabDraw, 1);
          td->t = t;
          td->accent = ui_color_from_name(s("accent"));
          ui_box_equip_custom_draw(tab, app_tab_draw, td);
          UI_Signal sig = ui_signal_from_box(tab);
          if(!selected) { ui_pop_tag(); }
          if(ui_clicked(sig)) { app.filter = (Filter)fi; }
        }
        ui_spacer(ui_pct(1, 0));
        UI_CornerRadius(em*0.45f) UI_PrefWidth(ui_em(13.f, 1.f))
          AppVCenter(ui_em(13.f, 1.f))
        {
          AppRow(ui_pct(1, 0), ui_em(2.1f, 1.f))
            app_text_field(&app.search_field, s("Search  (Ctrl+F)###search_input"), s("###search_input"));
        }
      }
      ui_spacer(ui_em(0.5f, 1.f));

      //- list
      ui_set_next_child_layout_axis(Axis2_Y);
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_pct(1, 0));
      UI_Box *list = ui_build_box_from_string(UI_BoxFlag_Clip|UI_BoxFlag_ViewScrollY|UI_BoxFlag_ViewClampY|UI_BoxFlag_AllowOverflowY, s("task_list"));
      U64 shown = 0;
      UI_Parent(list)
      {
        for(int pass = app.sort_by_prio ? Prio_High : 0; ; pass += (app.sort_by_prio ? -1 : 1))
        {
          for(Task *t = app.first, *next = 0; t != 0; t = next)
          {
            next = t->next;
            if(app.sort_by_prio && (int)t->prio != pass) { continue; }
            if(!task_is_visible(t)) { continue; }
            if(shown != 0) { ui_spacer(ui_em(0.35f, 1.f)); }
            shown += 1;
            app_task_row(t);
          }
          if(!app.sort_by_prio || pass == 0) { break; }
        }
        if(shown == 0)
        {
          ui_spacer(ui_em(3.f, 1.f));
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_em(2.f, 1.f)) UI_TextAlignment(UI_TextAlign_Center) UI_TagF("weak")
            ui_label(total == 0 ? s("Nothing to do. Type a task above and press Enter.") : s("No tasks match this view."));
        }
      }
      ui_signal_from_box(list);
      ui_spacer(ui_em(0.7f, 1.f));

      //- footer
      AppRow(ui_pct(1, 0), ui_em(2.4f, 1.f))
      {
        // fills whatever the buttons leave over (strictness 0 => truncates with "..." only when really needed)
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF("weak")
          ui_label(s("Enter add  ·  Esc cancel  ·  right-click"));
        UI_CornerRadius(em*0.45f) UI_PrefHeight(ui_pct(1, 0)) UI_PrefWidth(ui_text_dim(em*1.2f, 1)) UI_TextAlignment(UI_TextAlign_Center)
        {
          ui_push_tag(s(app.sort_by_prio ? "accent_button" : "weak"));
          ui_set_next_pref_height(ui_pct(1, 0));
          UI_Box *sb = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawHotEffects|(app.sort_by_prio ? UI_BoxFlag_DrawBackground : 0),
                                                s("Sort by priority###sort_btn"));
          UI_Signal ss = ui_signal_from_box(sb);
          ui_pop_tag();
          if(ui_clicked(ss)) { app.sort_by_prio ^= 1; app.dirty = 1; }
          ui_spacer(ui_em(0.5f, 1.f));
          ui_set_next_pref_height(ui_pct(1, 0));
          ui_set_next_flags(done == 0 ? UI_BoxFlag_Disabled : 0);
          UI_Signal cs = ui_buttonf("Clear done###clear_btn");
          if(ui_clicked(cs) && done > 0)
          {
            for(Task *t = app.first, *next = 0; t != 0; t = next) { next = t->next; if(t->done) { task_release(t); } }
            app.dirty = 1;
          }
        }
      }
      ui_spacer(ui_em(1.2f, 1.f));
    }
  }

  //- context menu (one shared menu; app.ctx_task_id says which task it targets)
  UI_Key ctx_key = ui_key_from_string(ui_key_zero(), s("task_ctx"));
  UI_CtxMenu(ctx_key)
  {
    Task *t = task_from_id(app.ctx_task_id);
    if(t == 0) { ui_ctx_menu_close(); }
    else
    {
      UI_PrefWidth(ui_em(13.f, 1.f)) UI_PrefHeight(ui_em(2.2f, 1.f)) UI_TextAlignment(UI_TextAlign_Left)
      {
        UI_Signal s_edit = ui_button(s("Rename###ctx_rename"));
        if(ui_clicked(s_edit))
        {
          app.rename_request = t->id;   // picked up by that task's row next frame (it knows its own focus key)
          ui_ctx_menu_close();
        }
        UI_Signal s_done = ui_button(t->done ? s("Mark as active###ctx_toggle") : s("Mark as done###ctx_toggle"));
        if(ui_clicked(s_done)) { t->done ^= 1; t->done_unix = t->done ? now_time_unix() : 0; app.dirty = 1; ui_ctx_menu_close(); }
        for(int p = Prio_High; p >= 0; p -= 1)
        {
          ui_push_tag(s(prio_tag[p]));
          UI_Signal sp = ui_buttonf("Priority: %s###ctx_prio_%d", prio_label[p], p);
          ui_pop_tag();
          if(ui_clicked(sp)) { t->prio = (Prio)p; app.dirty = 1; ui_ctx_menu_close(); }
        }
        UI_Signal s_del = ui_button(s("Delete###ctx_delete"));
        if(ui_clicked(s_del))
        {
          if(app.edit_id == t->id) { app.edit_id = 0; }
          task_release(t); app.ctx_task_id = 0; app.dirty = 1; ui_ctx_menu_close();
        }
      }
    }
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ UI draw walk (condensed from rd_window_frame in raddbg_core.c)

internal void
app_draw_ui(void)
{
  Vec4F32 shadow_color = ui_color_from_name(s("drop_shadow"));
  for(UI_Box *box = ui_root_from_state(app.ui); !ui_box_is_nil(box);)
  {
    UI_BoxRec rec = ui_box_rec_df_post(box, &ui_nil_box);
    if(box->transparency != 0) { dr_push_transparency(box->transparency); }

    if(box->flags & UI_BoxFlag_DrawDropShadow)
    {
      R_Rect2DInst *inst = dr_rect(shift_2f32(pad_2f32(box->rect, 8), v2f32(4, 4)), shadow_color, 0.8f, 0, 8.f);
      MemoryCopyArray(inst->corner_radii, box->corner_radii);
    }
    if(box->flags & UI_BoxFlag_DrawBackground)
    {
      F32 t = box->hot_t * (1 - ((box->flags & UI_BoxFlag_DrawActiveEffects) ? box->active_t : 0));
      if(box->flags & UI_BoxFlag_DrawHotEffects)
      {
        Vec4F32 sc = shadow_color; sc.w *= t * box->background_color.w * 0.6f;
        R_Rect2DInst *si = dr_rect(shift_2f32(pad_2f32(box->rect, 6), v2f32(2, 3)), sc, 0.8f, 0, 7.f);
        MemoryCopyArray(si->corner_radii, box->corner_radii);
      }
      R_Rect2DInst *inst = dr_rect(pad_2f32(box->rect, 1.f), box->background_color, 0, 0, 1.f);
      MemoryCopyArray(inst->corner_radii, box->corner_radii);
      if(box->flags & UI_BoxFlag_DrawHotEffects)
      {
        Vec4F32 h = ui_color_from_tags_key_name(box->tags_key, s("hover"));
        h.w *= 0.014f * box->hot_t + 0.014f * box->active_t;
        R_Rect2DInst *hi = dr_rect(pad_2f32(box->rect, 1.f), h, 0, 0, 1.f);
        MemoryCopyArray(hi->corner_radii, box->corner_radii);
      }
    }
    else if(box->flags & UI_BoxFlag_DrawHotEffects)
    {
      Vec4F32 h = ui_color_from_tags_key_name(box->tags_key, s("hover"));
      h.w *= 0.014f * box->hot_t + 0.014f * box->active_t;
      R_Rect2DInst *hi = dr_rect(pad_2f32(box->rect, 1.f), h, 0, 0, 1.f);
      MemoryCopyArray(hi->corner_radii, box->corner_radii);
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
      // focus ring: the keyboard-nav hot box, and an actively edited field
      B32 is_nav_hot = (!ui_key_match(b->key, ui_key_zero()) && ui_key_match(b->key, app.nav_hot_key));
      if(b->flags & UI_BoxFlag_Clickable && (is_nav_hot || b->focus_active_t > 0.01f))
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
      if(b->transparency != 0) { dr_pop_transparency(); }
    }
    box = rec.next;
  }
}

////////////////////////////////
//~ Initial window size
//
// CW_USEDEFAULT lets Windows pick the monitor, which may not be the primary one, so the DPI to
// scale by is only known *after* the window exists (wm_dpi_from_window). Size in logical pixels,
// scale by that window's DPI, and clamp to 92% of that monitor's work area.

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
  if(work.x > 0) { w = Min(w, work.x * 0.92f); }
  if(work.y > 0) { h = Min(h, work.y * 0.92f); }
  W32_WM_Window *w32 = w32_wm_window_from_handle(window);
  if(w32 != 0)
  {
    SetWindowPos(w32->hwnd, 0, 0, 0, (int)w, (int)h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
  }
#else
  (void)window; (void)logical_w; (void)logical_h;
#endif
}

////////////////////////////////
//~ Per-frame hook

internal B32
frame(void)
{
  Temp scratch = scratch_begin(0, 0);

  // RE-ENTRANCY: on WM_SIZE / WM_PAINT the window procedure calls update() -> frame() *from inside*
  // wm_get_events() of an outer frame() (live-resize keeps painting while Windows runs its modal size loop).
  // Only the outermost call may pump OS events (wm_get_events owns global event-list state); nested calls
  // just rebuild + redraw + present at the new size. raddbg does the same with rd_state->frame_depth.
  app.frame_depth += 1;

  //- block on the OS when idle (outermost call only)
  WM_EventList wm_events = {0};
  if(app.frame_depth == 1) { wm_events = wm_get_events(scratch.arena, app.frames_requested == 0); }
  UI_EventList ui_events = {0};
  app_ui_events_from_wm_events(scratch.arena, &wm_events, &ui_events);
  if(wm_events.count != 0) { app.frames_requested = 4; }

  //- DPI-aware sizes
  F32 dpi = wm_dpi_from_window(app.window);
  if(dpi < 48.f) { dpi = 96.f; }
  F32 scale = dpi / 96.f;

  dr_begin_frame(app.icons);   // before ui_begin_build (see skill gotchas)

  //- build
  F32 dt = 1.f / wm_get_system_info()->default_refresh_rate;
  ui_select_state(app.ui);
  UI_IconInfo icon_info = {app.icons};
  UI_AnimationInfo anim =
  {
    .hot_animation_rate     = 1 - pow_f32(2, -50.f * dt),
    .active_animation_rate  = 1 - pow_f32(2, -50.f * dt),
    .focus_animation_rate   = 1 - pow_f32(2, -50.f * dt),
    .tooltip_animation_rate = 1 - pow_f32(2, -40.f * dt),
    .menu_animation_rate    = 1 - pow_f32(2, -40.f * dt),
    .scroll_animation_rate  = 1 - pow_f32(2, -40.f * dt),
  };
  ui_begin_build(app.window, &ui_events, &icon_info, &app.themes[app.light_theme], &anim, dt, dt);
  ui_push_font(app.font);
  ui_push_font_size(floor_f32(15.f * scale));
  ui_push_text_padding(floor_f32(7.f * scale));
  ui_push_text_raster_flags(FNT_RasterFlag_Smooth|FNT_RasterFlag_Hinted);
  ui_push_blur_size(10.f);
  ui_push_pref_width(ui_px(300, 1));
  ui_push_pref_height(ui_px(40, 1));
  app_build_ui();
  ui_end_build();
  if(ui_animating_from_state(app.ui)) { app.frames_requested = 4; }
  if(app.edit_id != 0 || !ui_key_match(ui_active_key(UI_MouseButtonKind_Left), ui_key_zero())) { app.frames_requested = Max(app.frames_requested, 2); }

  //- persist
  if(app.dirty) { app_save(); }

  //- window title
  {
    U64 total = 0, done = 0;
    app_counts(&total, &done);
    String8 title = push_str8f(scratch.arena, "gui_todo — %I64u left of %I64u", total - done, total);
    U64 n = Min(title.size, sizeof(app.last_title) - 1);
    if(!(MemoryMatch(app.last_title, title.str, n) && app.last_title[n] == 0))
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
//~ Entry point

internal void
entry_point(CmdLine *cmdline)
{
  app.arena = arena_alloc();
  app.frames_requested = 4;
  app_build_theme(&app.themes[0], theme_dark,  ArrayCount(theme_dark));
  app_build_theme(&app.themes[1], theme_light, ArrayCount(theme_light));
  app_load();
  if(app.first == 0 && app.next_id == 0)
  {
    // first run: a few example tasks (oldest pushed last so they end up on top in order)
    task_new(s("Try the right-click menu on a task"), 1)->prio = Prio_Low;
    task_new(s("Double-click a task to rename it"), 1);
    task_new(s("Click the priority badge to cycle LOW / MED / HIGH"), 1);
    task_new(s("Press Ctrl+T to switch theme"), 1)->prio = Prio_High;
    app_save();
  }

  app.window = wm_window_open(r2f32p(0, 0, 640, 860), WM_WindowFlag_UseDefaultPosition, s("gui_todo"));
  app_fit_window_to_dpi(app.window, 640, 860);
  app.r_window = r_window_equip(app.window);
  app.font = fnt_tag_from_path(s("C:/Windows/Fonts/segoeui.ttf"));
  app.icons = fnt_tag_from_path(push_str8f(app.arena, "%S/icons.ttf", get_process_info()->binary_path));
  app.ui = ui_state_alloc();
  app.focus_request = &app.add_field;
  for(;!update(););
  if(app.dirty) { app_save(); }
}
