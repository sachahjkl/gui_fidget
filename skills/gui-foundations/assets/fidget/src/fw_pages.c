////////////////////////////////
//~ Shell (menu bar, tabs, status bar) and the four demo pages

////////////////////////////////
//~ Small helpers

internal F32
fw_text_w(FNT_Tag font, F32 size, char *text)
{
  return fnt_dim_from_tag_size_string(font, size, 0, 0, str8_cstring(text)).x;
}

global UI_Box *fw_scroll_box = 0;

internal UI_BOX_CUSTOM_DRAW(fw_scrollbar_draw)
{
  F32 size = box->fixed_size.y;
  F32 total = box->view_bounds.y;
  if(total <= size + 1.f) { return; }
  F32 em = box->font_size;
  F32 th = Max(em*2.f, size*size/total);
  F32 maxoff = total - size;
  F32 ty = box->rect.y0 + fw_clamp01(box->view_off.y / maxoff) * (size - th);
  Vec4F32 c = ui_color_from_tags_key_name(box->tags_key, s("text"));
  c.w = 0.08f + 0.10f*box->hot_t;
  dr_rect(r2f32p(box->rect.x1 - em*0.55f, ty, box->rect.x1 - em*0.15f, ty + th), c, em*0.2f, 0, 1.f);
}

internal void
fw_scroll_page_begin(char *id)
{
  F32 em = ui_top_font_size();
  ui_set_next_child_layout_axis(Axis2_Y);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_pct(1, 0));
  UI_Box *sc = ui_build_box_from_stringf(UI_BoxFlag_Clip|UI_BoxFlag_ViewScrollY|UI_BoxFlag_ViewClampY|UI_BoxFlag_AllowOverflowY, "###%s", id);
  ui_box_equip_custom_draw(sc, fw_scrollbar_draw, 0);
  ui_push_parent(sc);
  fw_scroll_box = sc;
  ui_spacer(ui_px(em*1.0f, 1));
  fw_row_begin(ui_pct(1, 0), ui_children_sum(1));
  ui_spacer(ui_px(em*1.3f, 1));
  fw_col_begin(ui_pct(1, 0), ui_children_sum(1));
}

internal void
fw_scroll_page_end(void)
{
  F32 em = ui_top_font_size();
  ui_column_end();
  ui_spacer(ui_px(em*1.3f, 1));
  ui_row_end();
  ui_spacer(ui_px(em*0.5f, 1));
  UI_Box *sc = ui_pop_parent();
  ui_signal_from_box(sc);   // after the children: consumes wheel events inside the box and clamps view_off_target
}

////////////////////////////////
//~ Default values

internal void
fw_reset_values(void)
{
  app.sw[0] = 1; app.sw[1] = 0; app.sw[2] = 1;
  app.chk[0] = 1; app.chk[1] = 0; app.chk[2] = 0;
  app.radio = 0;
  app.slider[0] = 64.f; app.slider[1] = 0.75f; app.slider[2] = 21.5f;
  app.range_lo = 20.f; app.range_hi = 70.f;
  app.stepper = 5;
  app.seg = 1;
  app.chips = 0x5;
  app.dd[0] = 0; app.dd[1] = 1;
  app.rating = 3;
  app.progress = 0.35f; app.progress_run = 0;
  fw_field_set(&app.name, "Ada Lovelace");
  fw_field_set(&app.email, "ada@example.com");
  fw_field_set(&app.number, "42");
  fw_field_set(&app.search, "");
  fw_field_set(&app.tag_input, "");
  fw_field_set(&app.bio_title, "");
  fw_field_set(&app.filter, "");
  char *init_tags[] = {"ui", "c", "fast"};
  app.tag_count = 3;
  for EachIndex(i, 3) { MemoryZeroArray(app.tags[i]); MemoryCopy(app.tags[i], init_tags[i], cstring8_length((U8 *)init_tags[i])); }
  app.agree = 0; app.form_sent = 0;
  app.gravity = 1.f; app.bounce = 0.72f; app.repel = 1; app.phys_run = 1;
  app.sort_col = 0; app.sort_asc = 1; app.selected_row = -1;
  app.notes[0] = (Note){{20, 18},  {210, 118}, "Notes", 0};
  app.notes[1] = (Note){{150, 96}, {200, 112}, "Counter", 1};
  app.notes[2] = (Note){{40, 170}, {220, 118}, "Mix", 2};
  for EachIndex(i, NOTE_COUNT) { app.note_order[i] = (int)i; }
}

////////////////////////////////
//~ Data: 20,000 generated rows, sort + filter into an index array, virtualized table

global char *fw_first[] = {"Ada","Alan","Grace","Linus","Margaret","Dennis","Ken","Barbara","Edsger","Donald","Radia","Tim","Katherine","Claude","Hedy","Niklaus","Frances","Bjarne","Guido","Yukihiro"};
global char *fw_last[]  = {"Lovelace","Turing","Hopper","Torvalds","Hamilton","Ritchie","Thompson","Liskov","Dijkstra","Knuth","Perlman","Lee","Johnson","Shannon","Lamarr","Wirth","Allen","Stroustrup","Rossum","Matsumoto"};
global char *fw_dept[]  = {"Engineering","Design","Sales","Support","Legal"};
global char *fw_status[] = {"Active","Away","Offline"};
global char *fw_col_names[DATA_COLS] = {"ID", "Name", "Department", "Score", "Status"};

internal void
fw_data_init(void)
{
  app.rows = push_array(app.arena, DataRow, DATA_ROW_COUNT);
  app.order = push_array(app.arena, U32, DATA_ROW_COUNT);
  for EachIndex(i, DATA_ROW_COUNT)
  {
    DataRow *r = &app.rows[i];
    r->id = (U32)i + 1;
    char *f = fw_first[fw_rand_u64() % ArrayCount(fw_first)];
    char *l = fw_last[fw_rand_u64() % ArrayCount(fw_last)];
    raddbg_snprintf(r->name, sizeof(r->name), "%s %s", f, l);
    r->dept = (U8)(fw_rand_u64() % ArrayCount(fw_dept));
    r->status = (U8)(fw_rand_u64() % 3);
    r->score = (U16)(fw_rand_u64() % 1001);
    r->year = (U16)(2005 + fw_rand_u64() % 22);
  }
  F32 w[DATA_COLS] = {5.f, 13.f, 10.5f, 6.f, 8.f};     // in em units (DPI independent)
  for EachIndex(i, DATA_COLS) { app.col_w[i] = w[i]; }
  app.order_key = max_U64;
}

global int fw_cmp_col = 0;
global B32 fw_cmp_asc = 1;

internal int
fw_row_cmp(const void *a, const void *b)
{
  DataRow *ra = &app.rows[*(U32 *)a];
  DataRow *rb = &app.rows[*(U32 *)b];
  int c = 0;
  switch(fw_cmp_col)
  {
    case 0: c = (ra->id < rb->id) ? -1 : (ra->id > rb->id); break;
    case 1: c = MemoryCompare(ra->name, rb->name, sizeof(ra->name)); break;
    case 2: c = (int)ra->dept - (int)rb->dept; break;
    case 3: c = (int)ra->score - (int)rb->score; break;
    case 4: c = (int)ra->status - (int)rb->status; break;
  }
  if(c == 0) { c = (ra->id < rb->id) ? -1 : 1; }
  return fw_cmp_asc ? c : -c;
}

internal void
fw_data_rebuild_order(void)
{
  U64 key = u64_hash_from_str8(str8(app.filter.buf, app.filter.size)) ^ ((U64)app.sort_col * 0x9e3779b97f4a7c15ull) ^ (app.sort_asc ? 0x1234 : 0);
  if(key == app.order_key) { return; }
  app.order_key = key;
  String8 q = str8(app.filter.buf, app.filter.size);
  U64 n = 0;
  for EachIndex(i, DATA_ROW_COUNT)
  {
    DataRow *r = &app.rows[i];
    if(q.size == 0 ||
       str8_find_needle(str8_cstring(r->name), 0, q, StringMatchFlag_CaseInsensitive) < cstring8_length((U8 *)r->name) ||
       str8_find_needle(str8_cstring(fw_dept[r->dept]), 0, q, StringMatchFlag_CaseInsensitive) < cstring8_length((U8 *)fw_dept[r->dept]))
    {
      app.order[n++] = (U32)i;
    }
  }
  app.order_count = n;
  fw_cmp_col = app.sort_col;
  fw_cmp_asc = app.sort_asc;
  qsort(app.order, n, sizeof(U32), fw_row_cmp);
  app.selected_row = -1;
}

typedef struct FwResizeDraw FwResizeDraw;
struct FwResizeDraw { F32 hot; Vec4F32 line; Vec4F32 accent; };

internal UI_BOX_CUSTOM_DRAW(fw_resize_draw)
{
  FwResizeDraw *d = (FwResizeDraw *)user_data;
  F32 cx = (box->rect.x0 + box->rect.x1) / 2.f;
  Vec4F32 c = mix_4f32(d->line, d->accent, d->hot);
  F32 half = 0.5f + d->hot;
  dr_rect(r2f32p(cx - half, box->rect.y0 + 4.f, cx + half, box->rect.y1 - 4.f), c, 0, 0, 0.5f);
}

internal void
fw_page_data(void)
{
  F32 em = ui_top_font_size();
  Temp scratch = scratch_begin(0, 0);
  fw_data_rebuild_order();
  F32 row_h = em*2.1f;

  ui_spacer(ui_px(em*1.0f, 1));
  FwRow(ui_pct(1, 0), ui_pct(1, 0))
  {
    ui_spacer(ui_px(em*1.3f, 1));

    //- left: file tree
    FwCol(ui_px(em*17.f, 1), ui_pct(1, 0))
    {
      ui_push_tag(s("card"));
      ui_set_next_child_layout_axis(Axis2_Y);
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_pct(1, 0));
      UI_Box *card = 0;
      UI_CornerRadius(em*0.6f) card = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Clip, s("###tree_card"));
      UI_Parent(card) UI_PrefWidth(ui_pct(1, 0))
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1))
        {
          ui_spacer(ui_px(em*0.9f, 1));
          UI_TagF("weak") fw_icon_label(ICON_FOLDER_OPEN, em*1.8f);
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*1.05f) ui_label(s("Explorer"));
        }
        fw_hline();
        TreeNode nodes[] =
        {
          {"gui_fidget",-1,1}, {"src",0,1}, {"main.c",1,0}, {"fw_app.h",1,0}, {"fw_widgets.c",1,0}, {"fw_pages.c",1,0},
          {"kit",0,1}, {"base",6,1}, {"base_arena.c",7,0}, {"base_strings.c",7,0}, {"ui",6,1}, {"ui_core.c",10,0}, {"ui_core.h",10,0},
          {"draw",6,1}, {"draw.c",13,0}, {"data",0,1}, {"icons.ttf",15,0}, {"build.bat",0,0}, {"README.md",0,0},
        };
        int count = (int)ArrayCount(nodes);
        for(int i = 0; i < count; i += 1)
        {
          B32 visible = 1;
          int depth = 0;
          for(int p = nodes[i].parent; p >= 0; p = nodes[p].parent) { depth += 1; if(!app.tree_open[p]) { visible = 0; } }
          if(!visible) { continue; }
          B32 selected = (app.tree_sel == i);
          if(selected) { ui_push_tag(s("selected")); }
          ui_set_next_child_layout_axis(Axis2_X);
          ui_set_next_pref_width(ui_pct(1, 0));
          ui_set_next_pref_height(ui_px(em*2.0f, 1));
          UI_Box *row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects|(selected ? UI_BoxFlag_DrawBackground : 0), "###tree_%d", i);
          UI_Parent(row)
          {
            ui_spacer(ui_px(em*(0.5f + 1.1f*(F32)depth), 1));
            if(nodes[i].dir)
            {
              UI_PrefWidth(ui_px(em*1.6f, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak")
              {
                UI_Signal es = ui_expanderf(app.tree_open[i], "tree_exp_%d", i);
                if(ui_clicked(es)) { app.tree_open[i] ^= 1; }
              }
            }
            else { ui_spacer(ui_px(em*1.6f, 1)); }
            UI_TagF("weak") fw_icon_label(nodes[i].dir ? (app.tree_open[i] ? ICON_FOLDER_OPEN : ICON_FOLDER) : ICON_FILE, em*1.7f);
            UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(nodes[i].name));
          }
          UI_Signal rs = ui_signal_from_box(row);
          if(selected) { ui_pop_tag(); }
          if(ui_clicked(rs)) { app.tree_sel = i; }
          if(ui_double_clicked(rs) && nodes[i].dir) { app.tree_open[i] ^= 1; }
        }
      }
      ui_signal_from_box(card);
      ui_pop_tag();
    }
    ui_spacer(ui_px(em*1.0f, 1));

    //- right: toolbar + table
    FwCol(ui_pct(1, 0), ui_pct(1, 0))
    {
      FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
      {
        FwVCenter(ui_px(em*20.f, 1))
        {
          ui_set_next_pref_height(ui_px(em*2.3f, 1));
          UI_PrefWidth(ui_pct(1, 0)) fw_input(&app.filter, "Filter rows (name or department)", "data_filter", ICON_FIND, 0);
        }
        ui_spacer(ui_px(em*1.0f, 1));
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak")
          ui_labelf("%I64u of %d rows  |  %d UI boxes built for the visible rows", app.order_count, DATA_ROW_COUNT, (int)(app.boxes_built));
      }
      ui_spacer(ui_px(em*0.5f, 1));

      ui_push_tag(s("card"));
      ui_set_next_child_layout_axis(Axis2_Y);
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_pct(1, 0));
      UI_Box *tcard = 0;
      UI_CornerRadius(em*0.6f) tcard = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Clip, s("###table_card"));
      UI_Parent(tcard) UI_PrefWidth(ui_pct(1, 0))
      {
        //- header (sortable, resizable columns)
        ui_push_tag(s("bar"));
        ui_set_next_child_layout_axis(Axis2_X);
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*2.4f, 1));
        UI_Box *head = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawSideBottom, s("###table_head"));
        UI_Parent(head)
        {
          ui_spacer(ui_px(em*0.4f, 1));
          for EachIndex(c, DATA_COLS)
          {
            B32 sorting = (app.sort_col == (int)c);
            UI_PrefWidth(ui_px(em*app.col_w[c], 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak")
            {
              UI_Signal hs = ui_sort_headerf(sorting, app.sort_asc, "%s###hd_%d", fw_col_names[c], (int)c);
              if(ui_clicked(hs))
              {
                if(sorting) { app.sort_asc ^= 1; } else { app.sort_col = (int)c; app.sort_asc = 1; }
              }
            }
            ui_set_next_pref_width(ui_px(em*0.7f, 1));
            ui_set_next_pref_height(ui_pct(1, 0));
            ui_set_next_hover_cursor(WM_Cursor_LeftRight);
            UI_Box *rz = ui_build_box_from_stringf(UI_BoxFlag_Clickable, "###colsz_%d", (int)c);
            FwResizeDraw *rd = push_array(ui_build_arena(), FwResizeDraw, 1);
            rd->hot = Max(rz->hot_t, rz->active_t);
            rd->line = ui_color_from_name(s("border"));
            rd->accent = ui_color_from_name(s("accent"));
            ui_box_equip_custom_draw(rz, fw_resize_draw, rd);
            UI_Signal zs = ui_signal_from_box(rz);
            if(ui_pressed(zs)) { ui_store_drag_struct(&app.col_w[c]); }
            if(ui_dragging(zs)) { app.col_w[c] = Max(3.f, *ui_get_drag_struct(F32) + ui_drag_delta().x / em); }
          }
        }
        ui_signal_from_box(head);
        ui_pop_tag();

        //- virtualized rows: only rows intersecting the viewport become boxes
        UI_Key list_key = ui_key_from_string(ui_active_seed_key(), s("###table_rows"));
        UI_Box *prev = ui_box_from_key(list_key);
        F32 view_h = Max(dim_2f32(prev->rect).y, row_h*4.f);
        ui_set_next_child_layout_axis(Axis2_Y);
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_pct(1, 0));
        UI_Box *list = ui_build_box_from_string(UI_BoxFlag_Clip|UI_BoxFlag_ViewScrollY|UI_BoxFlag_ViewClampY|UI_BoxFlag_AllowOverflowY, s("###table_rows"));
        ui_box_equip_custom_draw(list, fw_scrollbar_draw, 0);

        //- keyboard navigation (only when the table was clicked and no text field is being edited)
        UI_Box *page = ui_box_from_key(app.page_key);
        B32 editing = !ui_key_match(page->default_nav_focus_active_key, ui_key_zero());
        if(app.table_focus && !editing && app.order_count > 0 && !app.pal_open && !app.modal_open)
        {
          S64 sel = app.selected_row;
          if(ui_key_press(0, WM_Key_Down))     { sel = (sel < 0) ? 0 : Min(sel + 1, (S64)app.order_count - 1); }
          if(ui_key_press(0, WM_Key_Up))       { sel = (sel < 0) ? 0 : Max(sel - 1, 0); }
          if(ui_key_press(0, WM_Key_PageDown)) { sel = Min(Max(sel, 0) + (S64)(view_h/row_h), (S64)app.order_count - 1); }
          if(ui_key_press(0, WM_Key_PageUp))   { sel = Max(Max(sel, 0) - (S64)(view_h/row_h), 0); }
          if(ui_key_press(0, WM_Key_Home))     { sel = 0; }
          if(ui_key_press(0, WM_Key_End))      { sel = (S64)app.order_count - 1; }
          if(sel != app.selected_row)
          {
            app.selected_row = sel;
            F32 y = (F32)sel * row_h;
            if(y < list->view_off_target.y) { list->view_off_target.y = y; }
            if(y + row_h > list->view_off_target.y + view_h) { list->view_off_target.y = y + row_h - view_h; }
          }
        }

        F32 off = Max(0.f, list->view_off_target.y < list->view_off.y ? list->view_off_target.y : list->view_off.y);
        S64 first = (S64)(off / row_h);
        S64 visible_n = (S64)(view_h / row_h) + 3;
        first = Clamp(0, first, Max(0, (S64)app.order_count - 1));
        S64 last = Min(first + visible_n, (S64)app.order_count);
        U64 boxes_before = ui_state->build_box_count;
        UI_Parent(list) UI_PrefWidth(ui_pct(1, 0))
        {
          ui_spacer(ui_px((F32)first * row_h, 1));
          for(S64 i = first; i < last; i += 1)
          {
            DataRow *r = &app.rows[app.order[i]];
            B32 selected = (i == app.selected_row);
            B32 zebra = (i & 1);
            ui_push_tag(selected ? s("selected") : (zebra ? s("zebra") : s("card")));
            ui_set_next_child_layout_axis(Axis2_X);
            ui_set_next_pref_width(ui_pct(1, 0));
            ui_set_next_pref_height(ui_px(row_h, 1));
            UI_Box *row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects|((selected || zebra) ? UI_BoxFlag_DrawBackground : 0), "###row_%u", r->id);
            UI_Parent(row)
            {
              ui_spacer(ui_px(em*0.4f, 1));
              UI_PrefHeight(ui_pct(1, 0))
              {
                UI_PrefWidth(ui_px(em*app.col_w[0] + em*0.7f, 1)) UI_Font(app.mono) UI_FontSize(em*0.9f) UI_TagF("weak") ui_labelf("%u", r->id);
                UI_PrefWidth(ui_px(em*app.col_w[1] + em*0.7f, 1)) ui_label(str8_cstring(r->name));
                UI_PrefWidth(ui_px(em*app.col_w[2] + em*0.7f, 1)) UI_TagF("weak") ui_label(str8_cstring(fw_dept[r->dept]));
                UI_PrefWidth(ui_px(em*app.col_w[3] + em*0.7f, 1)) UI_Font(app.mono) UI_FontSize(em*0.9f) ui_labelf("%u", (U32)r->score);
                UI_PrefWidth(ui_px(em*app.col_w[4] + em*0.7f, 1))
                {
                  ui_set_next_child_layout_axis(Axis2_X);
                  UI_Box *cell = ui_build_box_from_key(0, ui_key_zero());
                  UI_Parent(cell) UI_TagF("%s", r->status == 0 ? "good" : (r->status == 1 ? "warn" : "weak"))
                  {
                    fw_icon_label(ICON_DOT, em*1.2f);
                    UI_PrefWidth(ui_pct(1, 0)) ui_label(str8_cstring(fw_status[r->status]));
                  }
                }
              }
            }
            UI_Signal rs = ui_signal_from_box(row);
            ui_pop_tag();
            if(ui_pressed(rs)) { app.selected_row = i; app.table_focus = 1; }
          }
          ui_spacer(ui_px((F32)(app.order_count - (U64)last) * row_h, 1));
        }
        ui_signal_from_box(list);
        app.boxes_built = ui_state->build_box_count - boxes_before;
      }
      ui_signal_from_box(tcard);
      ui_pop_tag();
      ui_spacer(ui_px(em*0.4f, 1));
      FwRow(ui_pct(1, 0), ui_px(em*1.8f, 1))
      {
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.88f) UI_TagF("weak")
        {
          if(app.selected_row >= 0 && (U64)app.selected_row < app.order_count)
          {
            DataRow *r = &app.rows[app.order[app.selected_row]];
            ui_labelf("Selected #%u: %s, %s, score %u, joined %u   (Up/Down/PgUp/PgDn/Home/End to navigate)", r->id, r->name, fw_dept[r->dept], (U32)r->score, (U32)r->year);
          }
          else { ui_label(s("Click a row, then use the keyboard. Drag column borders to resize, click headers to sort.")); }
        }
      }
      ui_spacer(ui_px(em*0.8f, 1));
    }
    ui_spacer(ui_px(em*1.3f, 1));
  }
  scratch_end(scratch);
}

////////////////////////////////
//~ Controls page

global char *fw_seg_labels[] = {"Day", "Week", "Month"};
global char *fw_chip_labels[] = {"Alpha", "Beta", "Gamma", "Delta"};
global char *fw_langs[] = {"C", "C++", "Rust", "Zig", "Odin", "Go"};
global char *fw_sorts[] = {"Name", "Date modified", "Size", "Type"};

internal void
fw_page_controls(void)
{
  F32 em = ui_top_font_size();
  fw_scroll_page_begin("pg_controls");
  FwRow(ui_pct(1, 0), ui_children_sum(1))
  {
    FwCol(ui_pct(0.5f, 0), ui_children_sum(1))
    {
      FwCard("Buttons", ICON_PLAY)
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Primary, ICON_PLAY, "Primary", "b_primary", 0)))     { fw_toast(Toast_Good, "Primary clicked"); }
          ui_spacer(ui_px(em*0.5f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Secondary", "b_secondary", 0)))       { fw_toast(Toast_Info, "Secondary clicked"); }
          ui_spacer(ui_px(em*0.5f, 1));
          if(ui_clicked(fw_button(Btn_Danger, ICON_TRASH, "Danger", "b_danger", 0)))       { fw_toast(Toast_Bad, "Danger clicked"); }
          ui_spacer(ui_px(em*0.5f, 1));
          if(ui_clicked(fw_button(Btn_Ghost, 0, "Ghost", "b_ghost", 0)))                   { fw_toast(Toast_Info, "Ghost clicked"); }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          char *icons[] = {ICON_PLAY, ICON_PAUSE, ICON_STOP, ICON_REFRESH, ICON_SAVE, ICON_PIN, ICON_CLIPBOARD, ICON_GEAR};
          char *tips[]  = {"Play", "Pause", "Stop", "Refresh", "Save", "Pin", "Copy to clipboard", "Settings"};
          for EachIndex(i, ArrayCount(icons))
          {
            char id[24];
            raddbg_snprintf(id, sizeof(id), "ib_%d", (int)i);
            FwVCenter(ui_px(em*2.2f, 1))
            {
              UI_Signal sig = fw_icon_button(icons[i], id, 2.1f);
              FwTip(sig, tips[i]);
              if(ui_clicked(sig)) { fw_toast(Toast_Info, "%s", tips[i]); }
            }
            ui_spacer(ui_px(em*0.2f, 1));
          }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          fw_button(Btn_Secondary, 0, "Disabled", "b_disabled", 1);
          ui_spacer(ui_px(em*0.8f, 1));
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak") ui_label(s("Hover, press (they squish), and tab to focus."));
        }
      }
      FwCard("Selection", ICON_LIST)
      {
        FwFormRow("Range") { UI_CornerRadius(em*0.5f) fw_segmented(&app.seg, fw_seg_labels, 3, "seg1"); }
        FwFormRow("Tags")  { fw_chips(&app.chips, fw_chip_labels, 4, "chips1"); }
        FwFormRow("Language")  fw_dropdown(&app.dd[0], fw_langs, (int)ArrayCount(fw_langs), "dd_lang");
        FwFormRow("Sort by")   fw_dropdown(&app.dd[1], fw_sorts, (int)ArrayCount(fw_sorts), "dd_sort");
      }
      FwCard("Menus & tooltips", ICON_WINDOW)
      {
        UI_Key ctx_key = ui_key_from_string(ui_key_zero(), s("ctx_demo"));
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*5.5f, 1));
        ui_set_next_text_alignment(UI_TextAlign_Center);
        UI_Box *area = 0;
        ui_push_tag(s("input"));
        UI_CornerRadius(em*0.6f)
          area = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawText|UI_BoxFlag_DrawHotEffects, s("Right-click anywhere in this box###ctx_area"));
        ui_pop_tag();
        UI_Signal as = ui_signal_from_box(area);
        if(ui_right_clicked(as)) { ui_ctx_menu_open(ctx_key, area->key, sub_2f32(ui_mouse(), area->rect.p0)); }
        UI_CtxMenu(ctx_key)
        {
          F32 w = em*16.f;
          if(fw_menu_item(ICON_CLIPBOARD, "Copy", "Ctrl+C", 0, w))   { fw_toast(Toast_Info, "Copy"); }
          if(fw_menu_item(0, "Paste", "Ctrl+V", 0, w))               { fw_toast(Toast_Info, "Paste"); }
          if(fw_menu_item(0, "Duplicate", "Ctrl+D", 0, w))           { fw_toast(Toast_Info, "Duplicate"); }
          fw_hline_px(w);
          if(fw_menu_item(0, "Show grid", "", app.chk[2], w))        { app.chk[2] ^= 1; }
          fw_hline_px(w);
          if(fw_menu_item(ICON_TRASH, "Delete", "Del", 0, w))        { fw_toast(Toast_Bad, "Delete"); }
        }
        ui_spacer(ui_px(em*0.7f, 1));
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          UI_Signal ts = fw_button(Btn_Secondary, ICON_INFO, "Hover for a rich tooltip", "b_tip", 0);
          if(ui_hovering(ts)) UI_Tooltip
          {
            UI_PrefWidth(ui_text_dim(em*0.5f, 1)) UI_PrefHeight(ui_px(em*2.f, 1))
            {
              UI_FontSize(em*1.1f) ui_label(s("Rich tooltip"));
              UI_TagF("weak") ui_label(s("Tooltips are floating boxes: any widgets work in them."));
              UI_Row
              {
                UI_PrefWidth(ui_text_dim(0, 1)) ui_label(s("Open palette:"));
                ui_spacer(ui_px(em*0.5f, 1));
                FwVCenter(ui_px(em*1.7f, 1)) fw_kbd("Ctrl+K");
              }
            }
          }
        }
      }
    }
    ui_spacer(ui_px(em*1.0f, 1));
    FwCol(ui_pct(0.5f, 0), ui_children_sum(1))
    {
      FwCard("Toggles", ICON_TARGET)
      {
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_px(em*2.3f, 1))
        {
          fw_switch(&app.sw[0], "Wi-Fi", "sw0");
          fw_switch(&app.sw[1], "Bluetooth", "sw1");
          fw_switch(&app.sw[2], "Notifications", "sw2");
          fw_checkbox(&app.chk[0], "Remember me", "ck0");
          fw_checkbox(&app.chk[1], "Send anonymous usage data", "ck1");
          fw_radio(&app.radio, 0, "Small", "rd0");
          fw_radio(&app.radio, 1, "Medium", "rd1");
          fw_radio(&app.radio, 2, "Large", "rd2");
        }
      }
      FwCard("Sliders & steppers", ICON_GRID)
      {
        FwFormRow("Volume")      fw_slider(&app.slider[0], 0.f, 100.f, 1.f, "sl0", "%.0f");
        FwFormRow("Opacity")     fw_slider(&app.slider[1], 0.f, 1.f, 0.f, "sl1", "%.2f");
        FwFormRow("Temperature") fw_slider(&app.slider[2], -10.f, 40.f, 0.5f, "sl2", "%.1f C");
        FwFormRow("Range")       fw_range_slider(&app.range_lo, &app.range_hi, 0.f, 100.f, "rng1");
        FwFormRow("Stepper")     { FwVCenter(ui_pct(1, 0)) { ui_set_next_pref_height(ui_px(em*2.2f, 1)); fw_stepper(&app.stepper, 0, 99, "step1"); } }
        FwFormRow("Rating")      fw_rating(&app.rating, "rate1");
      }
      FwCard("Progress & feedback", ICON_INFO)
      {
        if(app.progress_run)
        {
          app.progress += app.real_dt * 0.12f;
          app.frames_requested = 4;
          if(app.progress >= 1.f) { app.progress = 1.f; app.progress_run = 0; fw_toast(Toast_Good, "Download complete"); }
        }
        FwFormRow("Download") { FwVCenter(ui_pct(1, 0)) fw_progress(app.progress, 0, "pr_det"); }
        FwFormRow("Loading")  { FwVCenter(ui_pct(1, 0)) fw_progress(0, 1, "pr_ind"); }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Primary, app.progress_run ? ICON_PAUSE : ICON_PLAY, app.progress_run ? "Pause" : "Start", "b_dl", 0)))
          {
            if(app.progress >= 1.f) { app.progress = 0.f; }
            app.progress_run ^= 1;
          }
          ui_spacer(ui_px(em*0.5f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, ICON_UNDO, "Reset", "b_dl_reset", 0))) { app.progress = 0.f; app.progress_run = 0; }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Info",    "t_info", 0))) { fw_run_cmd(Cmd_ToastInfo); }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Success", "t_good", 0))) { fw_run_cmd(Cmd_ToastGood); }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Warning", "t_warn", 0))) { fw_run_cmd(Cmd_ToastWarn); }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Error",   "t_bad", 0)))  { fw_run_cmd(Cmd_ToastBad); }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Ghost, ICON_QUESTION, "Open dialog", "b_modal", 0))) { app.modal_open = 1; }
        }
      }
    }
  }
  fw_scroll_page_end();
}

////////////////////////////////
//~ Inputs page

global char *fw_words[] =
{
  "arena allocator", "artifact cache", "box tree", "command palette", "context menu", "custom draw", "draw bucket",
  "font atlas", "frame loop", "hash table", "immediate mode", "keyboard focus", "layout pass", "linked list",
  "metaprogram", "nil sentinel", "render pass", "scratch arena", "signal flags", "string slice", "style stack",
  "theme tags", "tooltip root", "unity build", "virtual list", "window manager", "zero is initialization",
};

internal B32
fw_email_valid(TextField *f)
{
  String8 s_ = str8(f->buf, f->size);
  U64 at = str8_find_needle(s_, 0, s("@"), 0);
  if(at == 0 || at >= s_.size) { return 0; }
  U64 dot = str8_find_needle(s_, at, s("."), 0);
  return dot < s_.size && dot + 1 < s_.size;
}

internal B32
fw_number_valid(TextField *f)
{
  if(f->size == 0) { return 0; }
  for EachIndex(i, f->size) { if(!((f->buf[i] >= '0' && f->buf[i] <= '9') || (i == 0 && f->buf[i] == '-'))) { return 0; } }
  return 1;
}

internal void
fw_page_inputs(void)
{
  F32 em = ui_top_font_size();
  Temp scratch = scratch_begin(0, 0);
  fw_scroll_page_begin("pg_inputs");
  FwRow(ui_pct(1, 0), ui_children_sum(1))
  {
    FwCol(ui_pct(0.5f, 0), ui_children_sum(1))
    {
      FwCard("Text inputs", ICON_PENCIL)
      {
        B32 email_ok = fw_email_valid(&app.email);
        B32 num_ok = fw_number_valid(&app.number);
        FwFormRow("Name")  fw_input(&app.name, "Your name", "in_name", ICON_PERSON, 0);
        FwFormRow("Email") fw_input(&app.email, "you@example.com", "in_email", ICON_CLIPBOARD, app.email.size > 0 && !email_ok);
        FwRow(ui_pct(1, 0), ui_px(em*1.8f, 1))
        {
          ui_spacer(ui_pct(0.34f, 0));
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF(app.email.size == 0 ? "weak" : (email_ok ? "good" : "bad"))
            ui_label(app.email.size == 0 ? s("We never share it.") : (email_ok ? s("Looks good.") : s("Needs an @ and a domain.")));
        }
        FwFormRow("Number") fw_input(&app.number, "digits only", "in_num", ICON_GRID, app.number.size > 0 && !num_ok);
        FwRow(ui_pct(1, 0), ui_px(em*1.8f, 1))
        {
          ui_spacer(ui_pct(0.34f, 0));
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF(app.number.size == 0 ? "weak" : (num_ok ? "good" : "bad"))
            ui_label(app.number.size == 0 ? s("Try typing letters.") : (num_ok ? s("Valid integer.") : s("Not a number.")));
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.2f, 1))
        {
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF("weak")
            ui_label(s("Arrows, Home/End, Ctrl+arrows, Shift = select, Ctrl+A/C/X/V."));
        }
      }
      FwCard("Live fuzzy search", ICON_FIND)
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1)) { FwVCenter(ui_pct(1, 0)) { ui_set_next_pref_height(ui_px(em*2.3f, 1)); fw_input(&app.search, "Search concepts (try: ar al, \"tab\", box)", "in_search", ICON_FIND, 0); } }
        String8 needle = str8(app.search.buf, app.search.size);
        int matches = 0;
        for EachIndex(i, ArrayCount(fw_words))
        {
          FuzzyMatchRangeList r = fuzzy_match_find(scratch.arena, needle, str8_cstring(fw_words[i]));
          if(r.count != r.needle_part_count) { continue; }
          matches += 1;
          ui_set_next_pref_width(ui_pct(1, 0));
          ui_set_next_pref_height(ui_px(em*2.0f, 1));
          ui_set_next_child_layout_axis(Axis2_X);
          UI_Box *row = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawHotEffects, "###word_%d", (int)i);
          UI_Parent(row)
          {
            ui_spacer(ui_px(em*0.5f, 1));
            UI_TagF("weak") fw_icon_label(ICON_DOT, em*1.2f);
            UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0))
            {
              UI_Box *lb = ui_label(str8_cstring(fw_words[i])).box;
              if(r.count != 0) { ui_box_equip_fuzzy_match_ranges(lb, &r); }
            }
          }
          UI_Signal rs = ui_signal_from_box(row);
          if(ui_clicked(rs)) { fw_toast(Toast_Info, "Picked: %s", fw_words[i]); }
        }
        if(matches == 0) { UI_PrefHeight(ui_px(em*2.2f, 1)) UI_TagF("weak") ui_label(s("No matches.")); }
        UI_PrefHeight(ui_px(em*1.8f, 1)) UI_FontSize(em*0.85f) UI_TagF("weak") ui_labelf("%d of %d", matches, (int)ArrayCount(fw_words));
      }
    }
    ui_spacer(ui_px(em*1.0f, 1));
    FwCol(ui_pct(0.5f, 0), ui_children_sum(1))
    {
      FwCard("Tag input", ICON_PIN)
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          for(int i = 0; i < app.tag_count; i += 1)
          {
            F32 w = fw_text_w(ui_top_font(), ui_top_font_size(), app.tags[i]) + em*3.4f;
            FwVCenter(ui_px(w, 1))
            {
              ui_push_tag(s("secondary_button"));
              ui_set_next_child_layout_axis(Axis2_X);
              ui_set_next_pref_width(ui_pct(1, 0));
              ui_set_next_pref_height(ui_px(em*1.9f, 1));
              UI_Box *chip = 0;
              UI_CornerRadius(em*0.95f) chip = ui_build_box_from_stringf(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, "###tag_%s", app.tags[i]);
              UI_Parent(chip)
              {
                ui_spacer(ui_px(em*0.8f, 1));
                UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(app.tags[i]));
                char id[40]; raddbg_snprintf(id, sizeof(id), "tag_x_%d", i);
                UI_Signal xs = fw_icon_button(ICON_X, id, 1.6f);
                if(ui_clicked(xs))
                {
                  for(int j = i; j + 1 < app.tag_count; j += 1) { MemoryCopy(app.tags[j], app.tags[j+1], sizeof(app.tags[j])); }
                  app.tag_count -= 1;
                }
              }
              ui_signal_from_box(chip);
              ui_pop_tag();
            }
            ui_spacer(ui_px(em*0.4f, 1));
          }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1))
        {
          UI_Signal ts = {0};
          FwVCenter(ui_pct(1, 0)) { ui_set_next_pref_height(ui_px(em*2.3f, 1)); ts = fw_input(&app.tag_input, app.tag_count >= 8 ? "Tag limit reached" : "Type a tag, press Enter", "in_tag", ICON_ADD, 0); }
          if(ui_committed(ts) && app.tag_input.size > 0 && app.tag_count < 8)
          {
            U64 n = Min(app.tag_input.size, sizeof(app.tags[0]) - 1);
            MemoryZeroArray(app.tags[app.tag_count]);
            MemoryCopy(app.tags[app.tag_count], app.tag_input.buf, n);
            app.tag_count += 1;
            app.tag_input.size = app.tag_input.cursor = app.tag_input.mark = 0;
            app.focus_request = &app.tag_input;
          }
        }
      }
      FwCard("A small form", ICON_CLIPBOARD)
      {
        B32 name_ok = app.name.size > 0;
        B32 can_submit = name_ok && fw_email_valid(&app.email) && app.agree;
        FwFormRow("Title") fw_input(&app.bio_title, "Engineer, designer, ...", "in_bio", ICON_PENCIL, 0);
        UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_px(em*2.3f, 1)) fw_checkbox(&app.agree, "I agree to the terms", "ck_agree");
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Primary, ICON_CHECK, "Submit", "b_submit", !can_submit)))
          {
            app.form_sent = 1;
            fw_toast(Toast_Good, "Submitted: %.*s <%.*s>", (int)app.name.size, app.name.buf, (int)app.email.size, app.email.buf);
          }
          ui_spacer(ui_px(em*0.8f, 1));
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF(can_submit ? "good" : "weak")
            ui_label(can_submit ? s("Ready to submit.") : s("Needs a name, a valid email (left card) and the checkbox."));
        }
      }
      FwCard("Keyboard shortcuts", ICON_GEAR)
      {
        struct { char *keys; char *what; } sc[] =
        {
          {"Ctrl+K", "Command palette"}, {"Ctrl+1..4", "Switch page"}, {"Ctrl+T", "Next theme"},
          {"Ctrl+B", "Toggle status bar"}, {"Tab / Enter", "Move focus / activate"}, {"Esc", "Close palette, dialog, field"},
        };
        for EachIndex(i, ArrayCount(sc))
        {
          FwRow(ui_pct(1, 0), ui_px(em*2.2f, 1))
          {
            FwVCenter(ui_px(em*7.5f, 1)) fw_kbd(sc[i].keys);
            UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak") ui_label(str8_cstring(sc[i].what));
          }
        }
      }
    }
  }
  fw_scroll_page_end();
  scratch_end(scratch);
}

////////////////////////////////
//~ Playground page

typedef struct FwSpinDraw FwSpinDraw;
struct FwSpinDraw { F32 angle; F32 omega; Vec4F32 accent; Vec4F32 track; Vec4F32 text; };

internal UI_BOX_CUSTOM_DRAW(fw_spinner_draw)
{
  FwSpinDraw *d = (FwSpinDraw *)user_data;
  Vec2F32 c = center_2f32(box->rect);
  Vec2F32 dim = dim_2f32(box->rect);
  F32 R = Min(dim.x, dim.y) * 0.34f;
  F32 two_pi = 6.2831853f;
  for(int k = 0; k < 3; k += 1)
  {
    F32 th = (d->angle + (F32)k/3.f) * two_pi;
    Vec2F32 dir = v2f32(cosf(th), sinf(th));
    Vec4F32 col = fw_hsv_color(fmodf((F32)k/3.f + 0.02f, 1.f), 0.0f, 1.f, 1.f);
    col = mix_4f32(d->accent, d->text, 0.0f);
    if(k == 1) { col = mix_4f32(d->accent, v4f32(1, 1, 1, 1), 0.25f); }
    if(k == 2) { col = mix_4f32(d->accent, v4f32(0, 0, 0, 1), 0.2f); }
    // arm: a chain of overlapping discs from the hub out to the lobe
    for(int j = 0; j <= 10; j += 1)
    {
      F32 t = (F32)j/10.f;
      F32 rr = mix_1f32(R*0.30f, R*0.20f, t);
      Vec2F32 p = v2f32(c.x + dir.x*R*t, c.y + dir.y*R*t);
      dr_rect(r2f32p(p.x - rr, p.y - rr + 3, p.x + rr, p.y + rr + 3), v4f32(0, 0, 0, 0.18f), rr, 0, 6.f);
    }
    for(int j = 0; j <= 10; j += 1)
    {
      F32 t = (F32)j/10.f;
      F32 rr = mix_1f32(R*0.30f, R*0.20f, t);
      Vec2F32 p = v2f32(c.x + dir.x*R*t, c.y + dir.y*R*t);
      dr_rect(r2f32p(p.x - rr, p.y - rr, p.x + rr, p.y + rr), col, rr, 0, 1.f);
    }
    Vec2F32 e = v2f32(c.x + dir.x*R, c.y + dir.y*R);
    F32 lr = R*0.36f;
    dr_rect(r2f32p(e.x - lr, e.y - lr, e.x + lr, e.y + lr), col, lr, 0, 1.f);
    F32 hr = R*0.22f;
    dr_rect(r2f32p(e.x - hr, e.y - hr, e.x + hr, e.y + hr), d->track, hr, 0, 1.f);
  }
  F32 hub = R*0.30f;
  dr_rect(r2f32p(c.x - hub, c.y - hub, c.x + hub, c.y + hub), d->track, hub, 0, 1.f);
  F32 pin = R*0.12f;
  dr_rect(r2f32p(c.x - pin, c.y - pin, c.x + pin, c.y + pin), d->text, pin, 0, 1.f);
  Temp scratch = scratch_begin(0, 0);
  String8 rpm = push_str8f(scratch.arena, "%.0f rpm", abs_f32(d->omega) * 60.f);
  F32 size = box->font_size * 0.95f;
  FNT_Metrics m = fnt_metrics_from_tag_size(app.mono, size);
  Vec2F32 td = fnt_dim_from_tag_size_string(app.mono, size, 0, 0, rpm);
  dr_text(app.mono, size, 0, 0, FNT_RasterFlag_Smooth, v2f32(floor_f32(c.x - td.x/2), floor_f32(box->rect.y1 - m.descent - 8.f)), d->text, rpm);
  scratch_end(scratch);
}

internal void
fw_note_body(Note *n)
{
  F32 em = ui_top_font_size();
  UI_PrefWidth(ui_pct(1, 0))
  {
    switch(n->kind)
    {
      case 0:
      {
        UI_PrefHeight(ui_px(em*1.7f, 1)) UI_TagF("weak") { ui_label(s("Drag my title bar.")); ui_label(s("Click to bring me to front.")); ui_label(s("Edges are clamped.")); }
      }break;
      case 1:
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.6f, 1)) { ui_spacer(ui_px(em*0.5f, 1)); fw_stepper(&app.note_counter, -99, 999, "note_step"); }
        UI_PrefHeight(ui_px(em*1.7f, 1)) UI_TagF("weak") ui_labelf("  value squared: %I64d", app.note_counter * app.note_counter);
      }break;
      case 2:
      {
        UI_PrefHeight(ui_px(em*2.2f, 1)) { fw_switch(&app.sw[0], " Repel", "note_sw"); }
        ui_push_pref_height(ui_px(em*2.2f, 1));
        fw_slider(&app.slider[1], 0.f, 1.f, 0.f, "note_sl", "%.2f");
        ui_pop_pref_height();
      }break;
    }
  }
}

internal void
fw_page_playground(void)
{
  F32 em = ui_top_font_size();
  Temp scratch = scratch_begin(0, 0);
  fw_scroll_page_begin("pg_play");
  FwRow(ui_pct(1, 0), ui_children_sum(1))
  {
    FwCol(ui_pct(0.56f, 0), ui_children_sum(1))
    {
      FwCard("Physics arena", ICON_DOT)
      {
        FwRow(ui_pct(1, 0), ui_px(em*2.5f, 1))
        {
          UI_PrefWidth(ui_pct(0.5f, 0)) { fw_switch(&app.phys_run, "Simulate", "ph_run"); }
          ui_spacer(ui_px(em*0.6f, 1));
          UI_PrefWidth(ui_pct(0.5f, 0)) { fw_switch(&app.repel, "Mouse repels", "ph_rep"); }
        }
        FwFormRow("Gravity") fw_slider(&app.gravity, 0.f, 2.f, 0.f, "ph_g", "%.2f");
        FwFormRow("Bounce")  fw_slider(&app.bounce, 0.1f, 1.f, 0.f, "ph_b", "%.2f");
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Primary, ICON_ADD, "25 balls", "ph_add", 0))) { fw_spawn_balls_random(25, dim_2f32(app.arena_rect)); }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, 0, "Shake", "ph_shake", 0)))
          {
            for EachIndex(i, BALL_MAX) { if(app.balls[i].alive) { app.balls[i].v.x += fw_rand_range(-900.f, 900.f)*app.scale; app.balls[i].v.y -= fw_rand_range(300.f, 1400.f)*app.scale; } }
          }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Secondary, ICON_TARGET, "Confetti", "ph_conf", 0))) { fw_run_cmd(Cmd_Confetti); }
          ui_spacer(ui_px(em*0.4f, 1));
          if(ui_clicked(fw_button(Btn_Ghost, ICON_TRASH, "Clear", "ph_clear", 0))) { MemoryZeroArray(app.balls); }
        }
        // seed the arena with a few balls the first time it has a real size
        if(!app.balls_seeded && dim_2f32(app.arena_rect).x > 50.f)
        {
          app.balls_seeded = 1;
          fw_spawn_balls_random(16, dim_2f32(app.arena_rect));
        }
        // the arena
        ui_push_tag(s("input"));
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*21.f, 1));
        UI_Box *arena = 0;
        UI_CornerRadius(em*0.6f)
          arena = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Clip, s("###ball_arena"));
        ui_pop_tag();
        ui_box_equip_custom_draw(arena, fw_arena_draw, 0);
        Rng2F32 ar = arena->rect;
        if(dim_2f32(ar).x > 10.f) { app.arena_rect = ar; }
        UI_Signal as = ui_signal_from_box(arena);
        Vec2F32 local = sub_2f32(ui_mouse(), ar.p0);
        B32 inside = contains_2f32(ar, ui_mouse());
        U64 now = now_time_us();
        if(ui_pressed(as) || (ui_dragging(as) && now >= app.spawn_next_us))
        {
          fw_spawn_ball(local, v2f32(fw_rand_range(-200.f, 200.f)*app.scale, fw_rand_range(-400.f, -100.f)*app.scale));
          app.phys_run = 1;
          app.spawn_next_us = now + 35000;
        }
        if(ui_right_clicked(as)) { fw_spawn_confetti(local, 90); }
        U64 alive = 0; for EachIndex(i, BALL_MAX) { alive += app.balls[i].alive; }
        FwRow(ui_pct(1, 0), ui_px(em*2.0f, 1))
        {
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF("weak")
            ui_labelf("%I64u balls  |  click / drag to spawn  |  right-click for confetti", alive);
        }
        // simulate (positions are arena-local; uses the arena size from the previous layout)
        if(fw_physics_active() || ui_dragging(as))
        {
          F32 dt = Clamp(0.001f, app.real_dt, 0.033f);
          fw_physics_step(dim_2f32(app.arena_rect), local, inside, dt);
          app.frames_requested = 4;
        }
      }
    }
    ui_spacer(ui_px(em*1.0f, 1));
    FwCol(ui_pct(0.44f, 0), ui_children_sum(1))
    {
      FwCard("Fidget spinner", ICON_REFRESH)
      {
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*15.f, 1));
        ui_set_next_hover_cursor(WM_Cursor_HandPoint);
        UI_Box *sp = ui_build_box_from_string(UI_BoxFlag_Clickable, s("###spinner"));
        UI_Signal ss = ui_signal_from_box(sp);
        Vec2F32 c = center_2f32(sp->rect);
        Vec2F32 m = sub_2f32(ui_mouse(), c);
        F32 ang = atan2f(m.y, m.x) / 6.2831853f;
        F32 dt = Clamp(0.001f, app.real_dt, 0.05f);
        if(ui_pressed(ss)) { app.spin_prev_mouse_angle = ang; app.spin_was_dragging = 1; }
        if(ui_dragging(ss))
        {
          F32 delta = ang - app.spin_prev_mouse_angle;
          if(delta > 0.5f) { delta -= 1.f; }
          if(delta < -0.5f) { delta += 1.f; }
          app.spin_angle += delta;
          app.spin_omega = mix_1f32(app.spin_omega, delta / dt, 0.45f);
          app.spin_prev_mouse_angle = ang;
          app.frames_requested = 4;
        }
        else
        {
          app.spin_was_dragging = 0;
          app.spin_angle += app.spin_omega * dt;
          app.spin_omega *= expf(-0.55f * dt);
          if(abs_f32(app.spin_omega) < 0.004f) { app.spin_omega = 0; }
          else { app.frames_requested = 4; }
        }
        FwSpinDraw *d = push_array(ui_build_arena(), FwSpinDraw, 1);
        d->angle = app.spin_angle; d->omega = app.spin_omega;
        d->accent = ui_color_from_name(s("accent"));
        d->track = ui_color_from_name(s("track"));
        d->text = ui_color_from_name(s("text"));
        ui_box_equip_custom_draw(sp, fw_spinner_draw, d);
        FwRow(ui_pct(1, 0), ui_px(em*1.9f, 1))
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*0.85f) UI_TagF("weak") ui_label(s("Grab and flick it: angular velocity carries over, with friction."));
      }
      FwCard("Accent color (live theme edit)", ICON_PALETTE)
      {
        FwRow(ui_pct(1, 0), ui_px(em*9.5f, 1))
        {
          F32 h0 = app.hue, s0 = app.sat, v0 = app.val;
          UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_px(em*9.f, 1)) ui_sat_val_picker(app.hue, &app.sat, &app.val, s("###sv"));
          ui_spacer(ui_px(em*0.7f, 1));
          UI_PrefWidth(ui_px(em*1.9f, 1)) UI_PrefHeight(ui_px(em*9.f, 1)) ui_hue_picker(&app.hue, app.sat, app.val, s("###hue"));
          if(h0 != app.hue || s0 != app.sat || v0 != app.val) { app.accent_override = 1; }
        }
        FwRow(ui_pct(1, 0), ui_px(em*3.f, 1))
        {
          F32 sw_h[] = {0.62f, 0.08f, 0.40f, 0.83f, 0.52f, 0.02f, 0.13f};
          for EachIndex(i, ArrayCount(sw_h))
          {
            FwVCenter(ui_px(em*2.1f, 1))
            {
              char id[24]; raddbg_snprintf(id, sizeof(id), "swatch_%d", (int)i);
              UI_Key k = fw_key(id);
              UI_Box *pb = ui_box_from_key(k);
              ui_set_next_pref_width(ui_pct(1, 0));
              ui_set_next_pref_height(ui_px(em*2.0f + em*0.4f*pb->hot_t, 1));
              ui_set_next_background_color(fw_hsv_color(sw_h[i], 0.65f, 0.95f, 1.f));
              UI_Box *sb = 0;
              UI_CornerRadius(em*1.f) sb = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder, "###%s", id);
              UI_Signal sg = ui_signal_from_box(sb);
              if(ui_clicked(sg)) { app.hue = sw_h[i]; app.sat = 0.65f; app.val = 0.95f; app.accent_override = 1; }
            }
            ui_spacer(ui_px(em*0.35f, 1));
          }
        }
        FwRow(ui_pct(1, 0), ui_px(em*2.8f, 1))
        {
          if(ui_clicked(fw_button(Btn_Secondary, ICON_UNDO, "Reset to palette accent", "acc_reset", !app.accent_override)))
          {
            fw_set_palette(app.palette_idx);
          }
        }
      }
      FwCard("Sticky notes (drag them)", ICON_PIN)
      {
        ui_push_tag(s("input"));
        ui_set_next_child_layout_axis(Axis2_Y);
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*19.f, 1));
        UI_Box *area = 0;
        UI_CornerRadius(em*0.6f)
          area = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_Clip, s("###notes_area"));
        ui_pop_tag();
        Vec2F32 adim = dim_2f32(area->rect);
        int bring_front = -1;
        UI_Parent(area)
        {
          for(int oi = 0; oi < NOTE_COUNT; oi += 1)
          {
            int ni = app.note_order[oi];
            Note *n = &app.notes[ni];
            F32 nw = Min(n->size.x * app.scale, Max(adim.x - 4.f, 50.f)), nh = n->size.y * app.scale;
            Vec2F32 pos = v2f32(n->pos.x * app.scale, n->pos.y * app.scale);
            ui_push_tag(s("card"));
            ui_set_next_fixed_x(pos.x);
            ui_set_next_fixed_y(pos.y);
            ui_set_next_fixed_width(nw);
            ui_set_next_fixed_height(nh);
            ui_set_next_child_layout_axis(Axis2_Y);
            UI_Box *nb = 0;
            UI_CornerRadius(em*0.6f)
              nb = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder|UI_BoxFlag_DrawDropShadow|UI_BoxFlag_Clip, "###note_%d", ni);
            UI_Parent(nb) UI_PrefWidth(ui_pct(1, 0))
            {
              ui_push_tag(s("bar"));
              ui_set_next_child_layout_axis(Axis2_X);
              ui_set_next_pref_height(ui_px(em*2.2f, 1));
              ui_set_next_hover_cursor(WM_Cursor_UpDownLeftRight);
              UI_Box *hd = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawSideBottom, "###note_hd_%d", ni);
              UI_Parent(hd)
              {
                ui_spacer(ui_px(em*0.6f, 1));
                UI_TagF("weak") fw_icon_label(ICON_PIN, em*1.5f);
                UI_PrefWidth(ui_pct(1, 0)) UI_PrefHeight(ui_pct(1, 0)) ui_label(str8_cstring(n->title));
              }
              UI_Signal hs = ui_signal_from_box(hd);
              ui_pop_tag();
              if(ui_pressed(hs)) { ui_store_drag_struct(&n->pos); bring_front = oi; }
              if(ui_dragging(hs))
              {
                Vec2F32 start = *ui_get_drag_struct(Vec2F32);
                Vec2F32 d = ui_drag_delta();
                n->pos.x = Clamp(0.f, start.x + d.x / app.scale, Max(0.f, (adim.x - nw) / app.scale));
                n->pos.y = Clamp(0.f, start.y + d.y / app.scale, Max(0.f, (adim.y - nh) / app.scale));
                app.frames_requested = 4;
              }
              ui_spacer(ui_px(em*0.4f, 1));
              fw_note_body(n);
            }
            UI_Signal ns = ui_signal_from_box(nb);
            ui_pop_tag();
            if(ui_pressed(ns)) { bring_front = oi; }
          }
        }
        ui_signal_from_box(area);
        if(bring_front > 0)
        {
          int v = app.note_order[bring_front];
          for(int j = bring_front; j > 0; j -= 1) { app.note_order[j] = app.note_order[j-1]; }
          app.note_order[0] = v;
        }
      }
    }
  }
  fw_scroll_page_end();
  scratch_end(scratch);
}

////////////////////////////////
//~ Menu bar

global char *fw_menu_names[4] = {"File", "View", "Theme", "Help"};

internal UI_Key
fw_menu_key(int i)
{
  return ui_key_from_stringf(ui_key_zero(), "menu_%d", i);
}

internal void
fw_menubar_build(void)
{
  F32 em = ui_top_font_size();
  ui_push_tag(s("bar"));
  ui_set_next_child_layout_axis(Axis2_X);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_px(em*2.6f, 1));
  UI_Box *bar = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawSideBottom, s("###menubar"));
  UI_Parent(bar)
  {
    ui_spacer(ui_px(em*0.6f, 1));
    UI_TagF("accent") fw_icon_label(ICON_CUBE, em*2.0f);
    UI_PrefWidth(ui_text_dim(em*0.4f, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_FontSize(em*1.05f) ui_label(s("gui_fidget"));
    ui_spacer(ui_px(em*0.8f, 1));
    int hover_open = -1;
    for(int i = 0; i < 4; i += 1)
    {
      UI_Key mk = fw_menu_key(i);
      B32 open = ui_ctx_menu_is_open(mk);
      F32 w = fw_text_w(app.font, em, fw_menu_names[i]) + em*1.6f;
      FwVCenter(ui_px(w, 1))
      {
        ui_push_tag(open ? s("selected") : s("ghost_button"));
        ui_set_next_pref_width(ui_pct(1, 0));
        ui_set_next_pref_height(ui_px(em*1.9f, 1));
        ui_set_next_text_alignment(UI_TextAlign_Center);
        UI_Box *b = 0;
        UI_CornerRadius(em*0.4f)
          b = ui_build_box_from_stringf(UI_BoxFlag_Clickable|UI_BoxFlag_DrawText|UI_BoxFlag_DrawHotEffects|UI_BoxFlag_DrawBackground, "%s###menubtn_%d", fw_menu_names[i], i);
        app.menu_btn_key[i] = b->key;
        UI_Signal sig = ui_signal_from_box(b);
        ui_pop_tag();
        F32 h = dim_2f32(b->rect).y;
        if(ui_clicked(sig))
        {
          if(open) { ui_ctx_menu_close(); app.menu_open = -1; }
          else     { ui_ctx_menu_open(mk, b->key, v2f32(0, h + 4.f)); app.menu_open = i; }
        }
        else if(ui_hovering(sig) && app.menu_open >= 0 && app.menu_open != i && ui_ctx_menu_is_open(fw_menu_key(app.menu_open)))
        {
          hover_open = i;
        }
        if(hover_open == i) { ui_ctx_menu_open(mk, b->key, v2f32(0, h + 4.f)); app.menu_open = i; }
      }
    }
    ui_spacer(ui_pct(1, 0));
    FwVCenter(ui_px(em*19.f, 1))
    {
      UI_Signal ps = fw_button(Btn_Ghost, ICON_FIND, "Command palette", "bar_palette", 0);
      if(ui_clicked(ps)) { fw_palette_toggle(); }
    }
    FwVCenter(ui_px(em*5.f, 1)) fw_kbd("Ctrl+K");
    ui_spacer(ui_px(em*0.8f, 1));
  }
  ui_signal_from_box(bar);
  ui_pop_tag();

  if(app.menu_open >= 0 && !ui_any_ctx_menu_is_open()) { app.menu_open = -1; }
  if(app.debug_open_menu > 0 && app.frame_index == 6)
  {
    int i = app.debug_open_menu - 1;
    UI_Box *b = ui_box_from_key(app.menu_btn_key[i]);
    ui_ctx_menu_open(fw_menu_key(i), b->key, v2f32(0, dim_2f32(b->rect).y + 4.f));
    app.menu_open = i;
  }

  F32 w = em*19.f;
  UI_CtxMenu(fw_menu_key(0))
  {
    if(fw_menu_item(ICON_UNDO, "Reset all values", "", 0, w))   { fw_run_cmd(Cmd_ResetValues); }
    if(fw_menu_item(ICON_X, "Dismiss all toasts", "", 0, w))    { fw_run_cmd(Cmd_ClearToasts); }
    fw_hline_px(w);
    if(fw_menu_item(ICON_STOP, "Quit", "Alt+F4", 0, w))         { fw_run_cmd(Cmd_Quit); }
  }
  UI_CtxMenu(fw_menu_key(1))
  {
    if(fw_menu_item(0, "Status bar", "Ctrl+B", app.show_status, w))     { app.show_status ^= 1; }
    if(fw_menu_item(0, "Disable animations", "Ctrl+J", app.no_anim, w)) { fw_run_cmd(Cmd_ToggleNoAnim); }
    if(fw_menu_item(0, "Slow-motion animations", "", app.slowmo, w))    { fw_run_cmd(Cmd_ToggleSlowmo); }
    if(fw_menu_item(0, "UI box outlines", "", app.show_boxes, w))       { app.show_boxes ^= 1; }
    fw_hline_px(w);
    for(int i = 0; i < Page_COUNT; i += 1)
    {
      char sc[16]; raddbg_snprintf(sc, sizeof(sc), "Ctrl+%d", i + 1);
      if(fw_menu_item(page_icons[i], page_names[i], sc, app.page == (PageKind)i, w)) { app.page = (PageKind)i; }
    }
  }
  UI_CtxMenu(fw_menu_key(2))
  {
    for(int i = 0; i < PALETTE_COUNT; i += 1)
    {
      if(fw_menu_item(0, palettes[i].name, "", app.palette_idx == i, w)) { fw_run_cmd((CmdKind)(Cmd_ThemeMidnight + i)); }
    }
    fw_hline_px(w);
    if(fw_menu_item(ICON_REFRESH, "Cycle next", "Ctrl+T", 0, w)) { fw_run_cmd(Cmd_NextTheme); }
  }
  UI_CtxMenu(fw_menu_key(3))
  {
    if(fw_menu_item(ICON_FIND, "Command palette", "Ctrl+K", 0, w))    { fw_palette_toggle(); }
    if(fw_menu_item(ICON_QUESTION, "Confirm dialog demo", "", 0, w))  { app.modal_open = 1; }
    if(fw_menu_item(ICON_INFO, "About", "", 0, w))                    { fw_toast(Toast_Info, "gui_fidget: a widget playground on the RAD Debugger UI stack"); }
  }
}

////////////////////////////////
//~ Status bar

typedef struct FwSparkDraw FwSparkDraw;
struct FwSparkDraw { F32 v[64]; U64 idx; Vec4F32 color; };

internal UI_BOX_CUSTOM_DRAW(fw_spark_draw)
{
  FwSparkDraw *d = (FwSparkDraw *)user_data;
  Rng2F32 r = box->rect;
  F32 w = (r.x1 - r.x0) / 64.f;
  F32 h = r.y1 - r.y0;
  for(int i = 0; i < 64; i += 1)
  {
    F32 ms = d->v[(d->idx + i) % 64];
    F32 bh = Clamp(1.f, ms / 8.f * h, h);
    Vec4F32 c = d->color; c.w = 0.35f + 0.65f*((F32)i/63.f);
    dr_rect(r2f32p(r.x0 + (F32)i*w, r.y1 - bh, r.x0 + (F32)i*w + Max(1.f, w - 1.f), r.y1), c, 0, 0, 0);
  }
}

internal void
fw_statusbar_build(void)
{
  F32 em = ui_top_font_size();
  if(!app.show_status) { return; }
  ui_push_tag(s("bar"));
  ui_set_next_child_layout_axis(Axis2_X);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_px(em*2.1f, 1));
  UI_Box *bar = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawSideTop, s("###statusbar"));
  UI_Parent(bar) UI_FontSize(em*0.85f)
  {
    ui_spacer(ui_px(em*0.9f, 1));
    UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_TagF("weak") ui_labelf("%s  |  %s", page_names[app.page], palettes[app.palette_idx].name);
    ui_spacer(ui_pct(1, 0));
    FwVCenter(ui_px(em*8.f, 1))
    {
      ui_set_next_pref_width(ui_pct(1, 0));
      ui_set_next_pref_height(ui_px(em*1.2f, 1));
      UI_Box *sp = ui_build_box_from_string(0, s("###spark"));
      FwSparkDraw *d = push_array(ui_build_arena(), FwSparkDraw, 1);
      MemoryCopyArray(d->v, app.ms_hist);
      d->idx = app.ms_hist_idx;
      d->color = ui_color_from_name(s("accent"));
      ui_box_equip_custom_draw(sp, fw_spark_draw, d);
    }
    ui_spacer(ui_px(em*0.8f, 1));
    UI_PrefWidth(ui_text_dim(0, 1)) UI_PrefHeight(ui_pct(1, 0)) UI_Font(app.mono) UI_TagF("weak")
      ui_labelf("%I64u boxes  |  UI %.2f ms", app.ui_boxes_total, app.ui_ms);
    ui_spacer(ui_px(em*0.9f, 1));
  }
  ui_signal_from_box(bar);
  ui_pop_tag();
}

////////////////////////////////
//~ Shell

internal void
fw_shell_build(void)
{
  F32 em = ui_top_font_size();
  ui_set_next_focus_hot(UI_FocusKind_On);
  ui_set_next_focus_active(UI_FocusKind_On);
  ui_set_next_child_layout_axis(Axis2_Y);
  ui_set_next_pref_width(ui_pct(1, 0));
  ui_set_next_pref_height(ui_pct(1, 0));
  UI_Box *page = ui_build_box_from_string(UI_BoxFlag_DrawBackground|UI_BoxFlag_DefaultFocusNav, s("###page"));
  app.page_key = page->key;
  app.nav_hot_key = page->default_nav_focus_hot_key;
  UI_Parent(page)
  {
    fw_menubar_build();

    //- page tabs
    ui_set_next_child_layout_axis(Axis2_X);
    ui_set_next_pref_width(ui_pct(1, 0));
    ui_set_next_pref_height(ui_px(em*2.9f, 1));
    UI_Box *tabs = ui_build_box_from_string(0, s("###tabs"));
    UI_Parent(tabs)
    {
      ui_spacer(ui_px(em*0.8f, 1));
      int sel = (int)app.page;
      fw_tabs(&sel, page_names, page_icons, Page_COUNT, "tab");
      app.page = (PageKind)sel;
    }

    //- content
    ui_set_next_child_layout_axis(Axis2_Y);
    ui_set_next_pref_width(ui_pct(1, 0));
    ui_set_next_pref_height(ui_pct(1, 0));
    UI_Box *content = ui_build_box_from_string(UI_BoxFlag_Clip, s("###content"));
    UI_Parent(content) UI_PrefWidth(ui_pct(1, 0))
    {
      switch(app.page)
      {
        default:
        case Page_Controls:   fw_page_controls();   break;
        case Page_Inputs:     fw_page_inputs();     break;
        case Page_Data:       fw_page_data();       break;
        case Page_Playground: fw_page_playground(); break;
      }
    }
    fw_statusbar_build();
  }
}
