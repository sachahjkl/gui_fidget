# The immediate-mode UI (`src/ui/`)

Contents: mental model · frame lifecycle · keys & identity · boxes & flags · style stacks ·
sizing & layout · interaction (signals/events/focus) · animation · themes (tags) ·
tooltips/context menus · custom draw · writing widgets · performance notes

Files: `ui_core.h/.c` (~3.5k lines, the engine), `ui_basic_widgets.h/.c` (label, button,
line edit, scroll list, tables, color pickers), `ui.mdesk` + `generated/ui.meta.*` (style
stacks). raddbg-specific widgets live in `raddbg/raddbg_widgets.c` (icon buttons, code
slices, tabs…) — copy those for ideas, not as a dependency.

## Mental model

Every frame you run ordinary C that *declares* the UI by calling `ui_build_box_*`; no
retained widget objects of your own. The library keeps one persistent `UI_Box` per key in a
hash table, so each box remembers: last-frame rect, hot/active/focus animation values,
scroll offsets, press history. Interaction (`ui_signal_from_box`) is evaluated **at declaration
time** against that remembered rect; layout is solved once at `ui_end_build`; drawing is a
separate walk of the tree (in your app) afterwards.

Consequence: a click always lands on what the user *saw* last frame (consistent), and the
frame loop requests ~4 frames after any change so one-frame-latency state converges.

## Frame lifecycle

```c
dr_begin_frame(icon_font);          // MUST precede the build (dr_thread_ctx is read during build)
ui_select_state(window_ui);         // UI_State is thread_static; one per window
ui_begin_build(window, &ui_events, &icon_info, &theme, &anim_info, real_dt, anim_dt);
  ui_push_font(font); ui_push_font_size(16); ui_push_text_padding(5);
  ui_push_pref_width(ui_px(300,1)); ui_push_pref_height(ui_px(40,1));   // defaults for the frame
  ... build boxes ...
ui_end_build();                     // prune, layout (x then y), animate, tooltips/menus placement
if(ui_animating_from_state(ui)) request_more_frames();
// then: walk ui_root_from_state(ui) and emit draw calls (see rendering-and-frame-loop.md)
```
`ui_begin_build` creates the root (fixed to window client size, `child_layout_axis = X`),
the tooltip root and the context-menu root, resets stacks, handles default keyboard
navigation, clears stale hot/active keys. `ui_end_build` removes untouched boxes,
runs layout, animates `*_t` values, clamps floating roots to the window, resolves clipboard
copy and "hover shows full truncated string" state, bumps `build_index`, clears the
build arena.

## Keys and identity

A key is `UI_Key {U64}` = 64-bit hash (`u64_hash_from_seed_str8`) of a string seeded by the key
of the nearest *keyed ancestor* (`ui_active_seed_key`). That makes IDs hierarchical: the same
label string inside two different parents does not collide.

String conventions for `ui_build_box_from_string` / `ui_button` / `ui_label`:
- `"Label"` → displays and hashes `Label`.
- `"Label##suffix"` → displays `Label`, hashes the whole string (`##` hides part of the display).
- `"Label###stable_id"` → displays `Label`, hashes **only** `###stable_id`. Use this when the
  text changes but identity (hover/anim state) must not: `ui_buttonf("%S###btn_%p", name, ptr)`.
- Empty string / `ui_key_zero()` → **transient** box (not cached; lives in the build arena;
  no persistent state). Use for layout containers, spacers, decorative boxes.
- Duplicate key in one frame → the second box silently becomes transient. If a widget
  "doesn't react", check for duplicate ids (e.g. two buttons with the same label in the same parent).

```c
UI_Key key = ui_key_from_stringf(ui_active_seed_key(), "row_%I64u", idx);   // per-item ids
UI_Box *b  = ui_build_box_from_key(UI_BoxFlag_Clickable, key);
```

## Boxes and flags

`ui_build_box_from_string(flags, string)` / `ui_build_box_from_key(flags, key)`. Flags are a
`U64` bitset: interaction (`MouseClickable`, `KeyboardClickable`, `ClickToFocus`, `Scroll`,
`ViewScroll*`, `ViewClamp*`, `DropSite`), layout (`Floating*`, `FixedWidth/Height`,
`AllowOverflow*`), appearance (`DrawBackground`, `DrawBorder`, `DrawText`, `DrawSide*`,
`DrawHotEffects`, `DrawActiveEffects`, `DrawDropShadow`, `DrawBackgroundBlur`, `Clip`,
`AnimatePos*`, `DisableTextTrunc`, `RoundChildrenByParent`, `Debug`, …) with bundles
(`UI_BoxFlag_Clickable`, `Floating`, `FixedSize`, …). Flags in the `UI_Flags(...)` stack are
OR-ed in; `UI_OmitFlags` masks some out for a subtree.

Text: `ui_box_equip_display_string(box, str)` or `ui_box_equip_display_fstrs(box, &fstrs)` for
"fancy strings" (runs with individual font/size/color/underline; used for syntax-colored code,
titles with icons). `ui_box_equip_custom_draw(box, fn, user_data)` for arbitrary drawing.

## Style stacks (generated from `ui.mdesk`)

A table row `{ PrefWidth pref_width UI_Size \`ui_px(250.f, 1.f)\` }` generates:
`ui_push_pref_width(v)`, `ui_pop_pref_width()`, `ui_top_pref_width()`, `ui_bottom_…`,
`ui_set_next_pref_width(v)` (applies to the **next box only**, then auto-pops), and the macro
`UI_PrefWidth(v)`. ~40 stacks: parent, child_layout_axis, fixed_x/y/width/height,
pref_width/height, min_width/height, permission_flags, flags, omit_flags, focus_hot/active,
fastpath_codepoint, group_key, transparency, tag, background/text/border color, squish,
hover_cursor, font, font_size, text_raster_flags, tab_size, 4 corner radii, visual margin,
blur_size, text_padding, text_alignment.

To add a stack: add one table row in `ui.mdesk`, rerun metagen, done (box struct field + copy
in `ui_build_box_from_key` are the only hand-written parts).
Implementation core (all stacks share these four macros in `ui_core.c`):
```c
#define UI_StackPushImpl(state, name_upper, name_lower, type, new_value) \
  UI_##name_upper##Node *node = state->name_lower##_stack.free; \
  if(node != 0) {SLLStackPop(state->name_lower##_stack.free);} \
  else {node = push_array(ui_build_arena(), UI_##name_upper##Node, 1);} \
  type old_value = state->name_lower##_stack.top->v; \
  node->v = new_value; SLLStackPush(state->name_lower##_stack.top, node); ...
```
Composite helpers: `UI_FixedPos`, `UI_FixedSize`, `UI_WidthFill` (=`UI_PrefWidth(ui_pct(1,0))`),
`UI_HeightFill`, `UI_Focus(kind)`, `UI_FlagsAdd(f)`, `UI_TagF("fmt", …)`, `UI_Padding(size)`,
`UI_Center`, `UI_Row`, `UI_Column`.

## Sizing and layout

Each box has `pref_size[axis] = {kind, value, strictness}`:

| Kind | Meaning | Helper |
|---|---|---|
| `Pixels` | fixed px | `ui_px(v, s)`, `ui_em(v, s)` (= v × current font size) |
| `TextContent` | text width + padding (+`text_padding`*2) | `ui_text_dim(pad, s)` |
| `ParentPct` | fraction of nearest ancestor with a *known* size | `ui_pct(0..1, s)` |
| `ChildrenSum` | sum (layout axis) or max (cross axis) of children | `ui_children_sum(s)` |

`strictness` ∈ [0,1]: how much a box resists shrinking when its parent overflows (1 = never
shrink, 0 = shrink freely). Layout per axis (X then Y), each a tree pass in `ui_layout_root`:
1. `calc_sizes_standalone` – Pixels, TextContent.
2. `calc_sizes_upwards_dependent` – ParentPct.
3. `calc_sizes_downwards_dependent` – ChildrenSum (post-order).
4. `enforce_constraints` – if children overflow along the layout axis, remove the violation
   proportionally to `size*(1-strictness)`; cross-axis children are clamped to the parent;
   `min_size` re-applied. `AllowOverflow` opts out.
5. `position` – children placed sequentially along `child_layout_axis` (X or Y);
   `Floating` children placed at `fixed_position`; rects floored to integer pixels (crisp
   borders/text); `view_off` subtracts for scrollable parents; `view_bounds` recorded.

```c
// a row: [label (text width)] [spacer (fills)] [right label]
UI_Row UI_Padding(ui_em(1,1)) UI_PrefWidth(ui_text_dim(10,1))
{
  ui_label(s("Name"));
  ui_spacer(ui_pct(1,0));        // strictness 0: shrinks/grows to absorb the slack
  ui_label(s("value"));
}
```
Child layout axis is a property of the *parent*: `ui_set_next_child_layout_axis(Axis2_Y)` before
building the container, or use `ui_row_begin/ui_column_begin`.

## Interaction

`UI_Signal sig = ui_signal_from_box(box);` after building the box (and its children if the
signal depends on them). It consumes matching events from the frame's `UI_EventList`, so a
nested/overlapping widget handled first "eats" the click:

```c
if(ui_clicked(sig))        ...   // left press+release inside
if(ui_double_clicked(sig)) ...
if(ui_dragging(sig))       ...   // ui_drag_delta(), ui_store_drag_struct(&x) to remember start values
if(ui_hovering(sig))       ...   // this box is the hot one
if(ui_right_clicked(sig))  ...
sig.scroll                 // for UI_BoxFlag_Scroll boxes, in line units
sig.event_flags            // modifiers during the interaction
```
Mechanics worth knowing: press sets `hot_box_key` + `active_box_key[button]`; releases inside
→ Clicked, outside → just Released; double/triple click via a 3-deep press history with the
system double-click time; `group_key` lets a drag continue across sibling boxes (tabs
reordering, paint-drag toggles); mouse hit-testing uses the box rect intersected with all
`Clip` ancestors and excludes the context-menu rect when you're not inside it.

Events: you convert OS events (`WM_Event`) to `UI_Event` (`Press/Release/MouseMove/Text/Scroll/FileDrop`
plus `Navigate/Edit` for text-editing semantics) and "action slots" (`Accept/Cancel/Edit/FocusMenu/
Lock…`) — i.e. the UI reacts to *intent*, your app maps keys → intent (raddbg maps them through its
rebindable command table). `ui_key_press(mods, key)`, `ui_text(codepoint)`, `ui_slot_press(slot)`
consume events from anywhere. `UI_PermissionFlags` (a style stack, default all) gate which event classes (left/middle/right click,
scroll X/Y, primary/secondary keyboard, text) `ui_next_event` will show to the code running under it.

Focus: two independent notions, **hot** (keyboard focus candidate) and **active** (editing).
`UI_Focus(UI_FocusKind_On/Off/Root)` stacks mark subtrees; boxes with `DefaultFocusNav*` flags get
arrow/tab navigation for free; `ui_set_auto_focus_active_key(key)` programmatically focuses a box
(that is how `ui_line_edit` enters editing on double-click or Enter).

## Animation

Per-box eased values, updated each `ui_end_build`:
`hot_t`, `active_t`, `focus_hot_t`, `focus_active_t`, `disabled_t`, `view_off → view_off_target`,
`fixed_position_animated` (with `UI_BoxFlag_AnimatePos`). Rates come from `UI_AnimationInfo`;
the recommended frame-rate independent form (used by `ui_state->default_animation_rate`):
```c
rate = 1 - pow_f32(2, -k * dt);       // k≈50..60 snappy, 30 slow
value += rate * (target - value);
```
Draw code multiplies alpha/offset by these `*_t` values — that is the whole hover/press look.
Arbitrary animated scalars: `F32 t = ui_anim(key, target, .rate = r, .initial = x, .reset = 0);`
(cached by key, pruned when unused for 2 builds). Anything still moving sets
`ui_state->is_animating`, which the host loop uses to keep requesting frames. Epsilons
(0.005–0.01) end animations so the loop can sleep.

## Themes: tags + patterns

The theme is a flat array of `UI_ThemePattern {String8Array tags; Vec4F32 linear;}`; colors are
looked up by `ui_color_from_name("text")` using the current **tag stack**:
```c
UI_TagF("weak") ui_label(s("secondary text"));   // pattern {weak,text} if present, else {text}
UI_TagF("floating")  // tooltips/menus push "." (reset) then "floating"
```
Match rule: pattern qualifies if the queried name equals its **last** tag and all its other tags are
in the current tag set; the qualifying pattern with the most tag matches wins. Results are cached
per `(tags_key, name)` and the cached color **eases toward the new target** each frame, so changing
theme cross-fades. Colors are stored linear (`linear_from_srgba(rgba_from_u32(0xRRGGBBAA))`).
Standard names used by the engine/draw code: `background text border hover drop_shadow selection
cursor`, plus `focus overlay`, `focus border`. Use arbitrary extra tags for semantic variants:
`good_pop`, `bad`, `weak`, `match`, … No CSS, no cascade engine, ~100 lines.

## Tooltips and context menus

Both are *floating roots* created in `ui_begin_build`, so you can add to them from anywhere:
```c
if(ui_hovering(sig)) UI_Tooltip { ui_label(s("shown near the mouse, squished-in animation")); }
// context menu
UI_Key menu = ui_key_from_string(ui_key_zero(), s("my_menu"));
if(ui_right_clicked(sig)) ui_ctx_menu_open(menu, box->key, v2f32(0, dim_2f32(box->rect).y));
UI_CtxMenu(menu) { if(ui_clicked(ui_button(s("Delete")))) { ...; ui_ctx_menu_close(); } }
```
`ui_end_build` anchors the menu to the anchor box, keeps both inside the window, and closes the menu
if it wasn't touched this frame or a click lands elsewhere. Tooltips also give a free
"hover over truncated text to see it all" feature (`ui_string_hover_*`).

## Custom drawing

```c
internal UI_BOX_CUSTOM_DRAW(my_draw)      // void my_draw(UI_Box *box, void *user_data)
{
  MyData *d = user_data;
  dr_rect(box->rect, d->color, 4.f /*radius*/, 0 /*border*/, 1.f /*softness*/);
}
...
MyData *d = push_array(ui_build_arena(), MyData, 1);   // must live until draw => build arena
UI_Box *box = ui_build_box_from_string(0, s("canvas"));
ui_box_equip_custom_draw(box, my_draw, d);
```
`ui_line_edit_draw` (cursor with an animated trail via `ui_anim`) is the best small example.
`UI_BoxFlag_DrawBucket` + `ui_box_equip_draw_bucket` lets a view pre-record a whole `DR_Bucket`
(e.g. a code view or 3D viewport) that the draw walk splices in with a translate transform.

## Writing a widget (template)

```c
internal UI_Signal
my_toggle(B32 *value, String8 label)       // label may contain ###id
{
  ui_set_next_child_layout_axis(Axis2_X);
  UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable|UI_BoxFlag_DrawBorder|
                                         UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawHotEffects|
                                         UI_BoxFlag_DrawActiveEffects, label);
  UI_Parent(box)
  {
    UI_PrefWidth(ui_em(2,1)) UI_Font(ui_icon_font()) UI_TextAlignment(UI_TextAlign_Center)
      ui_label(ui_icon_string_from_kind(*value ? UI_IconKind_CheckFilled : UI_IconKind_CheckHollow));
    UI_PrefWidth(ui_pct(1,0)) ui_label(ui_display_part_from_key_string(label));
  }
  UI_Signal sig = ui_signal_from_box(box);
  if(ui_clicked(sig)) { *value ^= 1; }
  return sig;
}
```
Pattern: build box → build children under `UI_Parent` → signal → mutate caller-owned state.
**State lives in your structs, never in the UI**; the UI only owns ephemeral interaction state.
Text editing is the one place the caller owns edit buffers (`ui_line_edit(&cursor,&mark,buf,cap,&size,
pre_edit_value,"key")`), mirroring that rule.

## Performance notes

- Boxes are allocated from free lists; typical frames allocate nothing from the OS.
- Layout is 5 linear passes × 2 axes over only the live tree; no recursion, `ui_box_rec_df_pre/post`
  iterate depth-first with explicit push/pop counts (also used by the draw walk).
- The font cache keeps a per-style direct map for ASCII glyphs and a per-frame run cache of shaped
  strings (`fnt_run_from_string`), so repeated labels cost a hash lookup.
- `UI_BoxFlag_DrawTextFastpathCodepoint` + `UI_FastpathCodepoint(c)` is *not* a perf flag: it marks the
  menu hotkey letter (pressing that character while the menu is focused clicks the box).
- Truncation uses `...` computed at draw time from the box rect (no layout feedback loop).
- raddbg has `DEV_draw_ui_box_heatmap`, `DEV_draw_ui_focus_debug`, and `UI_Debug` (hover shows key/string)
  — copy those switches into your app; they find "why is this tree so deep?" fast.
