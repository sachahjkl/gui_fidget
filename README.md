<p align="center"><img src=".project/image.png" width="128" alt="gui_fidget logo"></p>

<h1 align="center">gui_fidget</h1>

Fast, small, native GUI apps in C, built on the layers of the RAD Debugger codebase
(arena allocators, immediate-mode retained-state UI, D3D11 SDF renderer, one translation unit).
Each app is ~0.3-0.5 MB, shows its window in ~0.3 s, and uses 0 % CPU when idle.

| App | What it is |
|---|---|
| `apps/fidget` | **gui_fidget**: widget playground. Command palette (Ctrl+K), menus, dropdowns, switches, sliders, toasts, modal, a 20,000-row virtualized table, physics balls, a fidget spinner, a live accent color picker, 6 cross-fading themes, an option to disable animations (Ctrl+J). |
| `apps/todo` | **gui_todo**: a todo app (inline editing, priorities, filters, search, context menu, light/dark theme, persistence, DPI aware). |
| `apps/minimal` | **gui_minimal**: the smallest app that exercises the whole stack. Start here for a new project. |
| `kit/` | The vendored RAD Debugger layers the apps compile against (see NOTICE.md). |
| `skills/gui-foundations` | An AI-agent skill documenting the stack: principles, code patterns, gotchas, extraction script. |

## Build and run (Windows x64, MSVC 2022)

    run.bat                 # build + launch gui_fidget
    run.bat todo            # build + launch gui_todo
    run.bat minimal debug   # debug build of gui_minimal
    build.bat all           # build everything (from an "x64 Native Tools" prompt)

`run.bat` finds Visual Studio with `vswhere` and sets up the x64 environment itself. Outputs go to `apps/<app>/build/`.
Each app folder also builds on its own: copy it next to a `kit/` folder (or set `KIT=<path>`).

## Use the stack in your own project

1. Copy `kit/` (or refresh it from a RAD Debugger checkout with `skills/gui-foundations/scripts/extract_kit.ps1`).
2. Start from `apps/minimal`; look at `apps/todo` for text input and focus, and `apps/fidget` for a widget library.
3. Install the skill so an AI agent knows the patterns and traps: `powershell -File tools/install_skill.ps1`
   (copies it to `~/.agents/skills/gui-foundations`).

## Layout

    apps/<app>/src, data, build.bat, run.bat
    kit/                           vendored upstream layers (do not edit)
    skills/gui-foundations/        SKILL.md, references/, scripts/, assets/ (assets are generated, see below)
    tools/sync_skill.ps1           refresh skill assets from apps/ (apps are the source of truth)
    tools/install_skill.ps1        sync + install the skill

Linux: the kit contains the X11/OpenGL backends, but only Windows has been built and tested here.

## License

MIT for the code outside `kit/`; `kit/` is MIT, copyright Epic Games Tools (see NOTICE.md).
