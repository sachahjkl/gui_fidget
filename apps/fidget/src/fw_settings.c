////////////////////////////////
//~ Persisted settings (%APPDATA%\gui_fidget\settings.txt, plain key=value lines) + OS animation preference
//
// Animations follow the OS "show animations" setting by default (Windows: Settings > Accessibility > Visual effects),
// and can be forced on or off. The effective result is app.no_anim, refreshed once per frame by fw_anim_resolve().

internal B32
fw_os_animations_enabled(void)
{
#if OS_WINDOWS
  BOOL on = 1;
  if(!SystemParametersInfoW(0x1042 /*SPI_GETCLIENTAREAANIMATION*/, 0, &on, 0)) { on = 1; }
  return !!on;
#else
  return 1;
#endif
}

internal void
fw_anim_resolve(void)
{
  if(app.anim_mode == Anim_System)
  {
    U64 now = now_time_us();
    if(app.os_anim_checked_us == 0 || now - app.os_anim_checked_us > 500000)   // cheap, but no need to ask every frame
    {
      app.os_anim_on = fw_os_animations_enabled();
      app.os_anim_checked_us = now;
    }
    app.no_anim = !app.os_anim_on;
  }
  else
  {
    app.no_anim = (app.anim_mode == Anim_Off);
  }
}

internal Settings
fw_settings_snapshot(void)
{
  Settings s_ = {0};
  s_.anim_mode = app.anim_mode;
  s_.palette = app.palette_idx;
  s_.status_bar = app.show_status;
  return s_;
}

internal String8
fw_settings_dir(Arena *arena)
{
  return push_str8f(arena, "%S/gui_fidget", get_process_info()->user_program_config_data_path);
}

internal void
fw_settings_save(void)
{
  Temp scratch = scratch_begin(0, 0);
  char *anim_names[] = {"system", "on", "off"};
  String8 dir = fw_settings_dir(scratch.arena);
  make_directory(dir);
  String8 data = push_str8f(scratch.arena, "animations=%s\npalette=%d\nstatus_bar=%d\n", anim_names[app.anim_mode], app.palette_idx, (int)app.show_status);
  write_data_to_file_path(push_str8f(scratch.arena, "%S/settings.txt", dir), data);
  app.saved_settings = fw_settings_snapshot();
  scratch_end(scratch);
}

internal void
fw_settings_load(void)
{
  Temp scratch = scratch_begin(0, 0);
  String8 dir = fw_settings_dir(scratch.arena);
  String8 data = data_from_file_path(scratch.arena, push_str8f(scratch.arena, "%S/settings.txt", dir));
  String8List lines = str8_split(scratch.arena, data, (U8 *)"\n", 1, 0);
  for(String8Node *n = lines.first; n != 0; n = n->next)
  {
    String8 line = str8_skip_chop_whitespace(n->string);
    U64 eq = str8_find_needle(line, 0, s("="), 0);
    if(eq >= line.size) { continue; }
    String8 key = str8_prefix(line, eq);
    String8 val = str8_skip(line, eq + 1);
    if(str8_match(key, s("animations"), 0))
    {
      app.anim_mode = str8_match(val, s("on"), 0) ? Anim_On : (str8_match(val, s("off"), 0) ? Anim_Off : Anim_System);
    }
    else if(str8_match(key, s("palette"), 0))    { fw_set_palette((int)u64_from_str8(val, 10)); }
    else if(str8_match(key, s("status_bar"), 0)) { app.show_status = (u64_from_str8(val, 10) != 0); }
  }
  scratch_end(scratch);
}

// call once per frame: writes the file only when something persisted actually changed
internal void
fw_settings_autosave(void)
{
  Settings cur = fw_settings_snapshot();
  if(!MemoryMatch(&cur, &app.saved_settings, sizeof(cur))) { fw_settings_save(); }
}
