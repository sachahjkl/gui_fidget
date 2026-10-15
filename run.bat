@echo off
:: Build and launch one of the apps.
::   run.bat [fidget|todo|minimal] [release|debug]        (default: fidget)
setlocal
set "APP=%1"
if "%APP%"=="" set "APP=fidget"
if "%APP%"=="debug" ( set "APP=fidget" & set "MODE=debug" ) else set "MODE=%2"
if not exist "%~dp0apps\%APP%\run.bat" ( echo unknown app "%APP%" ^(fidget, todo, minimal^) & exit /b 1 )
call "%~dp0apps\%APP%\run.bat" %MODE%
