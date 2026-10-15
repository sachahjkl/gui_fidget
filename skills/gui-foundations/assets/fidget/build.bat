@echo off
:: gui_fidget build: MSVC x64, one translation unit.   build.bat [release|debug]
:: Run from an "x64 Native Tools" prompt, or use run.bat which sets the environment up.
setlocal
cd /D "%~dp0"
:: the UI kit lives in <repo>\kit (or in .\kit when this folder is copied out on its own); override with KIT=...
if "%KIT%"=="" set "KIT=%~dp0..\..\kit"
if not exist "%KIT%\base\base_inc.h" set "KIT=%~dp0kit"
if not exist build mkdir build
set mode=%1
if "%mode%"=="" set mode=release
if "%mode%"=="debug" (
  set opt=/Od /Ob1 /DBUILD_DEBUG=1
) else (
  set opt=/O2 /DBUILD_DEBUG=0
)
copy /y data\icons.ttf build\icons.ttf >nul
pushd build
:: /Zc:preprocessor is required (variadic + designated-initializer option macros)
call cl %opt% /I"%KIT%" /nologo /FC /Z7 /Zc:preprocessor ..\src\main.c ^
  /link /INCREMENTAL:NO /opt:ref /opt:icf /out:gui_fidget.exe || (popd & exit /b 1)
popd
echo built build\gui_fidget.exe
