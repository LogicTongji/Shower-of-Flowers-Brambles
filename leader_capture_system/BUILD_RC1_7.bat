@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ============================================================
echo HOI3 Leader Capture RC1.7 - Windows CRLF clean build
echo Target process default: hoi3_tfh.exe
echo ============================================================
echo.
where cl >nul 2>nul
if not errorlevel 1 goto TOOL_READY
set "VSROOT="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto TRY_FALLBACK
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if defined VSROOT goto INIT_VS
:TRY_FALLBACK
if exist "D:\VisualStudio\VS\Common7\Tools\VsDevCmd.bat" set "VSROOT=D:\VisualStudio\VS"
if not defined VSROOT goto NO_VS
:INIT_VS
echo [INFO] Visual Studio root: %VSROOT%
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 goto VS_INIT_FAIL
where cl >nul 2>nul
if errorlevel 1 goto NO_CL
:TOOL_READY
echo [PASS] x86 MSVC environment is available.
if not exist release mkdir release
del /q "release\HOI3_LeaderCapture_Launcher.exe" >nul 2>nul
del /q "release\HOI3_LeaderCapture.dll" >nul 2>nul
copy /y "leader_capture.ini" "release\leader_capture.ini" >nul
copy /y "START_LEADER_CAPTURE.bat" "release\START_LEADER_CAPTURE.bat" >nul
copy /y "README_CN.txt" "release\README_CN.txt" >nul
echo.
echo [1/2] Building exact R06B mechanism DLL...
cl /nologo /O2 /EHsc /LD /DWIN32 /D_WINDOWS src\leader_capture_auto_transfer_r06b.cpp /link /OUT:release\HOI3_LeaderCapture.dll
if errorlevel 1 goto BUILD_FAIL
echo.
echo [2/2] Building RC1.7 launcher...
cl /nologo /O2 /EHsc /DWIN32 /D_WINDOWS src\launcher_rc1_6_clean.cpp advapi32.lib /link /OUT:release\HOI3_LeaderCapture_Launcher.exe
if errorlevel 1 goto BUILD_FAIL
echo.
echo [SELF-CHECK] scanning launcher for legacy GAME-name and D328 hard gate...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='release\HOI3_LeaderCapture_Launcher.exe';$s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p));if($s.Contains('hoi3_tfh_GAME.exe') -or $s.Contains('d328ab9f567826a4adb18d99c783dd86bc8d8b773279c805ca5bf40d18e4eca4')){exit 9}else{exit 0}"
if errorlevel 1 goto LEGACY_FAIL
echo [PASS] No legacy GAME-name or D328 hash gate found in launcher.
echo.
echo ====================== BUILD PASS ======================
echo Output folder: release
echo Start hoi3_tfh.exe manually, reach the main menu,
echo then run release\START_LEADER_CAPTURE.bat
echo ========================================================
pause
exit /b 0
:NO_VS
echo.
echo [FAIL] Visual Studio C++ x86 tools were not found.
echo Install Visual Studio or Build Tools with Desktop development with C++.
goto END_FAIL
:VS_INIT_FAIL
echo.
echo [FAIL] VsDevCmd.bat failed to initialize the x86 environment.
goto END_FAIL
:NO_CL
echo.
echo [FAIL] cl.exe is still unavailable after Visual Studio initialization.
goto END_FAIL
:LEGACY_FAIL
echo.
echo [FAIL] Legacy hoi3_tfh_GAME.exe or D328 hash binding was found.
goto END_FAIL
:BUILD_FAIL
echo.
echo [FAIL] C++ compilation failed. Copy the compiler error lines back to ChatGPT.
:END_FAIL
echo.
echo ====================== BUILD FAIL ======================
pause
exit /b 1
