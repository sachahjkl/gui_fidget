@echo off
:: Build one app (or all of them). Run from an "x64 Native Tools" prompt.
::   build.bat [fidget|todo|minimal|all] [release|debug]       (default: all)
setlocal
set "APP=%1"
if "%APP%"=="" set "APP=all"
set "MODE=%2"
if "%APP%"=="all" (
  for %%a in (fidget todo minimal) do call "%~dp0apps\%%a\build.bat" %MODE% || exit /b 1
  exit /b 0
)
if not exist "%~dp0apps\%APP%\build.bat" ( echo unknown app "%APP%" ^(fidget, todo, minimal, all^) & exit /b 1 )
call "%~dp0apps\%APP%\build.bat" %MODE%
