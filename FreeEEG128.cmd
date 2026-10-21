@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "FREEEEG32_LAUNCH=%~dp0Firmware\tools\launch.ps1"
if not exist "%FREEEEG32_LAUNCH%" set "FREEEEG32_LAUNCH=%~dp0tools\launch.ps1"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%FREEEEG32_LAUNCH%" %*
set "FREEEEG32_EXIT=%ERRORLEVEL%"
if "%~1"=="" pause
exit /b %FREEEEG32_EXIT%
