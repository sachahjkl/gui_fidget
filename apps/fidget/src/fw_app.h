////////////////////////////////
//~ fidget widgets: shared types & global app state

#ifndef FW_APP_H
#define FW_APP_H

////////////////////////////////
//~ Icons (data/icons.ttf; characters from raddbg's rd_icon_kind_text_table)

#define ICON_FOLDER_OPEN "b"
#define ICON_FOLDER      "c"
#define ICON_FILE        "f"
#define ICON_PLAY        "g"
#define ICON_PAUSE       "r"
#define ICON_STOP        "s"
#define ICON_INFO        "i"
#define ICON_WARN        "w"
#define ICON_WARN_BIG    "W"
#define ICON_LOCK        "K"
#define ICON_UP          "^"
#define ICON_DOWN        "v"
#define ICON_RIGHT       ">"
#define ICON_GEAR        "G"
#define ICON_PENCIL      "P"
#define ICON_TRASH       "3"
#define ICON_PIN         "p"
#define ICON_RADIO       "O"
#define ICON_RADIO_ON    "o"
#define ICON_CHECK       "V"
#define ICON_ADD         "+"
#define ICON_MINUS       "-"
#define ICON_DOT         "."
#define ICON_X           "x"
#define ICON_REFRESH     "q"
#define ICON_UNDO        "j"
#define ICON_SAVE        "m"
#define ICON_WINDOW      "n"
#define ICON_TARGET      "l"
#define ICON_CLIPBOARD   "a"
#define ICON_FIND        "N"
#define ICON_PALETTE     "E"
#define ICON_LIST        "J"
#define ICON_GRID        "A"
#define ICON_CUBE        "*"
#define ICON_PERSON      "4"
#define ICON_QUESTION    "?"

////////////////////////////////
//~ Pages

typedef enum PageKind
{
  Page_Controls,
  Page_Inputs,
  Page_Data,
  Page_Playground,
  Page_COUNT
}
PageKind;

global char *page_names[Page_COUNT] = {"Controls", "Inputs", "Data", "Playground"};
global char *page_icons[Page_COUNT] = {ICON_GEAR, ICON_PENCIL, ICON_LIST, ICON_CUBE};

////////////////////////////////
//~ Text field state (buffers live in app structs, never in the UI)

typedef struct TextField TextField;
struct TextField
{
  U8 buf[256];
  U64 size;
  U64 cursor;
  U64 mark;
  U64 last_hash;
  B32 changed;
};

////////////////////////////////
//~ Palettes -> themes

typedef struct Palette Palette;
struct Palette
{
  char *name;
  U32 bg, panel, panel2, text, weak, border, accent, accent_text, good, warn, bad;
  B32 dark;
};

global Palette palettes[] =
{
  {"Midnight", 0x0f1220ff, 0x171b2fff, 0x20253dff, 0xe6e9f5ff, 0x8d95b5ff, 0x2b3150ff, 0x7c8cffff, 0x0b0e1cff, 0x4ade80ff, 0xfbbf24ff, 0xf87171ff, 1},
  {"Graphite", 0x151515ff, 0x1e1e1eff, 0x2a2a2aff, 0xeaeaeaff, 0x8e8e8eff, 0x353535ff, 0x4cc2ffff, 0x06222fff, 0x5dd39eff, 0xf4c95dff, 0xff6b6bff, 1},
  {"Daylight", 0xf3f4f7ff, 0xffffffff, 0xe9ecf2ff, 0x1f2430ff, 0x6b7380ff, 0xd9dde4ff, 0x3b5bdbff, 0xffffffff, 0x2f9e5aff, 0xc98a10ff, 0xd6333fff, 0},
  {"Nord",     0x2e3440ff, 0x3b4252ff, 0x434c5eff, 0xeceff4ff, 0x9aa5b8ff, 0x4c566aff, 0x88c0d0ff, 0x1d2430ff, 0xa3be8cff, 0xebcb8bff, 0xbf616aff, 1},
  {"Dracula",  0x1e1f29ff, 0x282a36ff, 0x343746ff, 0xf8f8f2ff, 0x9aa0c8ff, 0x44475aff, 0xbd93f9ff, 0x1a1326ff, 0x50fa7bff, 0xf1fa8cff, 0xff5555ff, 1},
  {"Amber",    0x16120bff, 0x201a10ff, 0x2d2415ff, 0xffe9c2ff, 0xb59a66ff, 0x3d311cff, 0xffb000ff, 0x1a1200ff, 0x9acd32ff, 0xffd54fff, 0xff7043ff, 1},
};
#define PALETTE_COUNT ((int)ArrayCount(palettes))

////////////////////////////////
//~ Toasts

typedef enum ToastKind { Toast_Info, Toast_Good, Toast_Warn, Toast_Bad } ToastKind;

typedef struct Toast Toast;
struct Toast
{
  B32 alive;
  ToastKind kind;
  char text[112];
  U64 born_us;
  U64 life_us;
};
#define TOAST_MAX 6

////////////////////////////////
//~ Playground physics

typedef struct Ball Ball;
struct Ball { B32 alive; Vec2F32 p; Vec2F32 v; F32 r; Vec4F32 c; };
#define BALL_MAX 300

typedef struct Particle Particle;
struct Particle { B32 alive; Vec2F32 p; Vec2F32 v; F32 life; F32 max_life; F32 size; Vec4F32 c; };
#define PARTICLE_MAX 600

typedef struct Note Note;
struct Note { Vec2F32 pos; Vec2F32 size; char *title; U32 kind; };
#define NOTE_COUNT 3

////////////////////////////////
//~ Data page

typedef struct DataRow DataRow;
struct DataRow
{
  U32 id;
  char name[32];
  U8 dept;
  U8 status;
  U16 score;
  U16 year;
};
#define DATA_ROW_COUNT 20000
#define DATA_COLS 5

typedef struct TreeNode TreeNode;
struct TreeNode { char *name; int parent; B32 dir; };

////////////////////////////////
//~ Commands (X-macro table: one row drives the enum, palette title, category, shortcut)

#define CMD_TABLE(X) \
X(GoControls,   "Go to Controls",             "Navigate", "Ctrl+1", ICON_GEAR)     \
X(GoInputs,     "Go to Inputs",               "Navigate", "Ctrl+2", ICON_PENCIL)   \
X(GoData,       "Go to Data",                 "Navigate", "Ctrl+3", ICON_LIST)     \
X(GoPlayground, "Go to Playground",           "Navigate", "Ctrl+4", ICON_CUBE)     \
X(ThemeMidnight,"Theme: Midnight",            "Theme",    "",       ICON_PALETTE)  \
X(ThemeGraphite,"Theme: Graphite",            "Theme",    "",       ICON_PALETTE)  \
X(ThemeDaylight,"Theme: Daylight",            "Theme",    "",       ICON_PALETTE)  \
X(ThemeNord,    "Theme: Nord",                "Theme",    "",       ICON_PALETTE)  \
X(ThemeDracula, "Theme: Dracula",             "Theme",    "",       ICON_PALETTE)  \
X(ThemeAmber,   "Theme: Amber",               "Theme",    "",       ICON_PALETTE)  \
X(NextTheme,    "Theme: Cycle next",          "Theme",    "Ctrl+T", ICON_REFRESH)  \
X(ToggleStatus, "View: Toggle status bar",    "View",     "Ctrl+B", ICON_WINDOW)   \
X(ToggleNoAnim, "View: Disable animations",   "View",     "Ctrl+J", ICON_PAUSE)    \
X(ToggleSlowmo, "View: Slow-motion animations","View",    "",       ICON_PAUSE)    \
X(ToggleBoxes,  "View: Show UI box outlines", "View",     "",       ICON_GRID)     \
X(ToastInfo,    "Toast: Info",                "Feedback", "",       ICON_INFO)     \
X(ToastGood,    "Toast: Success",             "Feedback", "",       ICON_CHECK)    \
X(ToastWarn,    "Toast: Warning",             "Feedback", "",       ICON_WARN)     \
X(ToastBad,     "Toast: Error",               "Feedback", "",       ICON_WARN_BIG) \
X(ClearToasts,  "Toast: Dismiss all",         "Feedback", "",       ICON_X)        \
X(OpenModal,    "Dialog: Open confirm dialog","Feedback", "",       ICON_QUESTION) \
X(SpawnBalls,   "Playground: Spawn 25 balls", "Playground","",      ICON_DOT)      \
X(Confetti,     "Playground: Confetti",       "Playground","",      ICON_TARGET)   \
X(ClearBalls,   "Playground: Clear balls",    "Playground","",      ICON_TRASH)    \
X(ResetValues,  "Reset all widget values",    "General",  "",       ICON_UNDO)     \
X(Quit,         "Quit",                       "General",  "Alt+F4", ICON_STOP)

typedef enum CmdKind
{
#define X(name, title, cat, key, icon) Cmd_##name,
  CMD_TABLE(X)
#undef X
  Cmd_COUNT
}
CmdKind;

typedef struct CmdInfo CmdInfo;
struct CmdInfo { char *title; char *category; char *shortcut; char *icon; };

global CmdInfo cmd_info[Cmd_COUNT] =
{
#define X(name, title, cat, key, icon) {title, cat, key, icon},
  CMD_TABLE(X)
#undef X
};

////////////////////////////////
//~ App state

typedef struct App App;
struct App
{
  Arena *arena;
  WM_Window window;
  R_Handle r_window;
  UI_State *ui;
  FNT_Tag font;
  FNT_Tag mono;
  FNT_Tag icons;
  DR_Bucket *bucket;
  B32 want_quit;

  // frame pacing / stats
  U64 frame_index;
  U64 frames_requested;
  U64 frame_depth;
  U64 last_frame_us;
  F32 real_dt;
  F32 scale;                    // dpi / 96
  F32 ui_ms;
  F32 ms_hist[64];
  U64 ms_hist_idx;
  U64 boxes_built;
  U64 ui_boxes_total;

  // theme
  UI_Theme theme;
  int palette_idx;
  B32 accent_override;
  F32 hue, sat, val;      // accent picker (hsv)

  // view toggles
  PageKind page;
  B32 show_status;
  B32 slowmo;
  B32 no_anim;
  B32 show_boxes;

  // overlays
  B32 pal_open;
  TextField pal_field;
  int pal_sel;
  int pal_scroll;
  U64 pal_open_frames;
  Vec2F32 pal_last_mouse;
  B32 modal_open;
  Toast toasts[TOAST_MAX];
  TextField *focus_request;

  // menu bar
  int menu_open;                // -1 = none
  UI_Key menu_btn_key[4];
  int debug_open_menu;          // test hook (--menu=N)

  // controls page
  B32 sw[3];
  B32 chk[3];
  int radio;
  F32 slider[3];
  F32 range_lo, range_hi;
  S64 stepper;
  U64 stepper_repeat_us;
  int seg;
  U32 chips;
  int dd[2];
  int rating;
  B32 progress_run;
  F32 progress;

  // inputs page
  TextField name, email, number, search, tag_input, bio_title;
  char tags[8][24];
  int tag_count;
  B32 agree;
  B32 form_sent;

  // data page
  DataRow *rows;
  U32 *order;
  U64 order_count;
  int sort_col;
  B32 sort_asc;
  TextField filter;
  U64 order_key;                // hash of (filter, sort) the order was built for
  S64 selected_row;             // index into order, -1 = none
  B32 table_focus;
  F32 col_w[DATA_COLS];
  B32 tree_open[32];
  int tree_sel;

  // playground
  Ball balls[BALL_MAX];
  Particle parts[PARTICLE_MAX];
  B32 phys_run;
  B32 balls_seeded;
  B32 repel;
  F32 gravity;
  F32 bounce;
  F32 spin_angle;               // turns
  F32 spin_omega;               // turns / second
  F32 spin_prev_mouse_angle;
  B32 spin_was_dragging;
  Note notes[NOTE_COUNT];
  int note_order[NOTE_COUNT];   // front -> back
  U64 spawn_next_us;
  S64 note_counter;

  // per-frame bookkeeping
  UI_Key page_key;
  UI_Key nav_hot_key;
  Rng2F32 arena_rect;           // ball arena (last frame)
  Vec2F32 window_dim;
  char last_title[512];
  B32 debug_title;
  U64 dbg_wheel_events;
};
global App app = {0};

////////////////////////////////
//~ Small shared helpers

global U64 fw_rng_state = 0x9e3779b97f4a7c15ull;

internal U64
fw_rand_u64(void)
{
  U64 x = fw_rng_state;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
  fw_rng_state = x;
  return x * 0x2545f4914f6cdd1dull;
}

internal F32
fw_rand01(void)
{
  return (F32)((fw_rand_u64() >> 40) & 0xffffff) / (F32)0x1000000;
}

internal F32
fw_rand_range(F32 lo, F32 hi)
{
  return lo + (hi - lo) * fw_rand01();
}

internal F32
fw_clamp01(F32 x)
{
  return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
}

internal F32
fw_rate(F32 k)
{
  if(app.no_anim) { return 1.f; }
  return 1.f - pow_f32(2.f, -k * ui_dt());
}

internal F32
fw_squish(F32 amount)
{
  return app.no_anim ? 0.f : amount;
}

internal Vec4F32
fw_hsv_color(F32 h, F32 s_, F32 v, F32 a)
{
  Vec3F32 rgb = rgb_from_hsv(v3f32(h, s_, v));
  Vec3F32 lin = linear_from_srgb(rgb);
  return v4f32(lin.x, lin.y, lin.z, a);
}

#endif // FW_APP_H
