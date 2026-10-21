@echo off
setlocal EnableExtensions DisableDelayedExpansion
call "%~dp0..\..\FreeEEG128.cmd" setup %*
set "FREEEEG128_RESULT=%ERRORLEVEL%"
if "%~1"=="" pause
exit /b %FREEEEG128_RESULT%
