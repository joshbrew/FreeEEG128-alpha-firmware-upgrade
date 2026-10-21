@echo off
rem Install missing tools, then flash. Works in both the checkout and the release ZIP.
set "FREEEEG128_LAUNCH=%~dp0..\tools\launch.ps1"
if not exist "%FREEEEG128_LAUNCH%" set "FREEEEG128_LAUNCH=%~dp0tools\launch.ps1"
if "%~1"=="" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%FREEEEG128_LAUNCH%" -Action flash
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%FREEEEG128_LAUNCH%" -Action flash -Image "%~f1"
)
if errorlevel 1 (
  echo Flash failed. Check the connections and STM32CubeProgrammer installation.
  pause
  exit /b 1
)
echo Firmware verified and board reset.
pause
