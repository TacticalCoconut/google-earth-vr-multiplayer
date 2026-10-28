@echo off
setlocal
set "EARTHVR_DIR=C:\Program Files (x86)\Steam\steamapps\common\EarthVR"

echo Removing Google Earth VR Multiplayer Mod from "%EARTHVR_DIR%"...
if exist "%EARTHVR_DIR%\version.dll" del /F /Q "%EARTHVR_DIR%\version.dll"
if exist "%EARTHVR_DIR%\avrt.dll" del /F /Q "%EARTHVR_DIR%\avrt.dll"
echo Mod uninstalled (earthvr_mp.ini preserved).
