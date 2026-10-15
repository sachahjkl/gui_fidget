////////////////////////////////
//~ Theme: a flat list of (tags -> palette slot * alpha); rebuilt (values only) every frame
//
// Tag strings are parsed once. Each frame the `linear` colors are refreshed from the current palette
// (+ live accent override from the color picker), and the UI layer eases the cached colors toward them,
// so palette switches and accent edits cross-fade for free.
//
// TIE-BREAKING (learned the hard way): a pattern qualifies when its LAST tag is the queried name and all its
// other tags are in the current tag stack; the pattern with the MOST matching tags wins, and on a tie the
// FIRST one in this list wins. A button inside a card has tags [card, accent_button], so both
// "card background" and "accent_button background" match with 2 tags: list variants (button kinds, selected,
// zebra, input, kbd, weak/bad/good...) BEFORE containers (card, bar, floating), or the container wins.

typedef enum Slot
{
  Slot_Bg, Slot_Panel, Slot_Panel2, Slot_Text, Slot_Weak, Slot_Border, Slot_Accent, Slot_AccentText,
  Slot_Good, Slot_Warn, Slot_Bad, Slot_White, Slot_Black, Slot_Hover,
}
Slot;

typedef struct ThemeSpec ThemeSpec;
struct ThemeSpec { char *tags; Slot slot; F32 alpha; };

global ThemeSpec theme_spec[] =
{
  //- generic names
  {"background",                 Slot_Bg,         1.0f},
  {"text",                       Slot_Text,       1.0f},
  {"border",                     Slot_Border,     1.0f},
  {"hover",                      Slot_Hover,      1.0f},
  {"drop_shadow",                Slot_Black,      0.60f},
  {"selection",                  Slot_Accent,     0.35f},
  {"cursor",                     Slot_Text,       1.0f},
  {"focus overlay",              Slot_Hover,      0.05f},
  {"focus border",               Slot_Accent,     1.0f},
  {"accent",                     Slot_Accent,     1.0f},
  {"track",                      Slot_Panel2,     1.0f},

  //- variants (must precede containers)
  {"weak text",                  Slot_Weak,       1.0f},
  {"bad text",                   Slot_Bad,        1.0f},
  {"good text",                  Slot_Good,       1.0f},
  {"warn text",                  Slot_Warn,       1.0f},
  {"bad border",                 Slot_Bad,        1.0f},
  {"good border",                Slot_Good,       1.0f},
  {"accent_button background",   Slot_Accent,     1.0f},
  {"accent_button text",         Slot_AccentText, 1.0f},
  {"accent_button border",       Slot_Accent,     1.0f},
  {"danger_button background",   Slot_Bad,        1.0f},
  {"danger_button text",         Slot_White,      1.0f},
  {"danger_button border",       Slot_Bad,        1.0f},
  {"ghost_button background",    Slot_Panel,      0.0f},
  {"ghost_button border",        Slot_Panel,      0.0f},
  {"secondary_button background",Slot_Panel2,     1.0f},
  {"secondary_button border",    Slot_Border,     1.0f},
  {"selected background",        Slot_Accent,     0.28f},
  {"zebra background",           Slot_Panel2,     0.35f},
  {"input background",           Slot_Bg,         1.0f},
  {"input border",               Slot_Border,     1.0f},
  {"kbd background",             Slot_Panel2,     1.0f},
  {"kbd text",                   Slot_Weak,       1.0f},
  {"kbd border",                 Slot_Border,     1.0f},
  {"scrim background",           Slot_Black,      0.55f},
  {"stripe_info background",     Slot_Accent,     1.0f},
  {"stripe_good background",     Slot_Good,       1.0f},
  {"stripe_warn background",     Slot_Warn,       1.0f},
  {"stripe_bad background",      Slot_Bad,        1.0f},

  //- containers (last)
  {"card background",            Slot_Panel,      1.0f},
  {"card border",                Slot_Border,     1.0f},
  {"bar background",             Slot_Panel,      1.0f},
  {"bar border",                 Slot_Border,     1.0f},
  {"floating background",        Slot_Panel2,     1.0f},
  {"floating text",              Slot_Text,       1.0f},
  {"floating border",            Slot_Border,     1.0f},
};

internal Vec4F32
fw_slot_color(Palette *p, Slot slot)
{
  U32 hex = 0xffffffff;
  switch(slot)
  {
    case Slot_Bg:         hex = p->bg;          break;
    case Slot_Panel:      hex = p->panel;       break;
    case Slot_Panel2:     hex = p->panel2;      break;
    case Slot_Text:       hex = p->text;        break;
    case Slot_Weak:       hex = p->weak;        break;
    case Slot_Border:     hex = p->border;      break;
    case Slot_Accent:     hex = p->accent;      break;
    case Slot_AccentText: hex = p->accent_text; break;
    case Slot_Good:       hex = p->good;        break;
    case Slot_Warn:       hex = p->warn;        break;
    case Slot_Bad:        hex = p->bad;         break;
    case Slot_White:      hex = 0xffffffff;     break;
    case Slot_Black:      hex = 0x000000ff;     break;
    case Slot_Hover:      hex = p->dark ? 0xffffffff : 0x000000ff; break;
  }
  return linear_from_srgba(rgba_from_u32(hex));
}

internal void
fw_theme_init(void)
{
  U64 count = ArrayCount(theme_spec);
  app.theme.patterns_count = count;
  app.theme.patterns = push_array(app.arena, UI_ThemePattern, count);
  for EachIndex(idx, count)
  {
    String8List tags = str8_split(app.arena, str8_cstring(theme_spec[idx].tags), (U8 *)" ", 1, 0);
    app.theme.patterns[idx].tags = str8_array_from_list(app.arena, &tags);
  }
}

internal Vec4F32
fw_accent_color(void)
{
  Palette *p = &palettes[app.palette_idx];
  Vec4F32 c = fw_slot_color(p, Slot_Accent);
  if(app.accent_override) { c = fw_hsv_color(app.hue, app.sat, app.val, 1.f); }
  return c;
}

internal void
fw_theme_update(void)
{
  Palette *p = &palettes[app.palette_idx];
  Vec4F32 accent = fw_accent_color();
  for EachIndex(idx, ArrayCount(theme_spec))
  {
    Vec4F32 c = fw_slot_color(p, theme_spec[idx].slot);
    if(theme_spec[idx].slot == Slot_Accent) { c = accent; }
    c.w *= theme_spec[idx].alpha;
    app.theme.patterns[idx].linear = c;
  }
}

internal void
fw_set_palette(int idx)
{
  app.palette_idx = (idx % PALETTE_COUNT + PALETTE_COUNT) % PALETTE_COUNT;
  app.accent_override = 0;
  Vec3F32 hsv = hsv_from_rgb(rgba_from_u32(palettes[app.palette_idx].accent).xyz);
  app.hue = hsv.x; app.sat = hsv.y; app.val = hsv.z;
}
