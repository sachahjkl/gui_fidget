@echo off
:: Sets up the MSVC x64 environment if needed, builds, then launches gui_minimal.
::   run.bat [release|debug]
setlocal
if not "%VSCMD_ARG_TGT_ARCH%"=="" goto have_env
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
call "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
:have_env
call "%~dp0build.bat" %1 || exit /b 1
start "" "%~dp0build\gui_minimal.exe"
