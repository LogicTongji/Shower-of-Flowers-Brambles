@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist "HOI3_LeaderCapture_Launcher.exe" goto MISSING_LAUNCHER
if not exist "HOI3_LeaderCapture.dll" goto MISSING_DLL
echo Starting HOI3 Leader Capture launcher...
"%~dp0HOI3_LeaderCapture_Launcher.exe"
set "RC=%ERRORLEVEL%"
echo.
echo Launcher exit code: %RC%
pause
exit /b %RC%
:MISSING_LAUNCHER
echo [ERROR] HOI3_LeaderCapture_Launcher.exe not found.
echo Run BUILD_RC1_7.bat first.
pause
exit /b 1
:MISSING_DLL
echo [ERROR] HOI3_LeaderCapture.dll not found.
echo Run BUILD_RC1_7.bat first.
pause
exit /b 1
