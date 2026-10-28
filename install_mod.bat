@echo off
setlocal
set "EARTHVR_DIR=C:\Program Files (x86)\Steam\steamapps\common\EarthVR"

if not exist "build\version.dll" (
    call "%~dp0build.bat"
    if errorlevel 1 exit /b 1
)

echo Installing Google Earth VR Multiplayer Mod to "%EARTHVR_DIR%"...
copy /Y "%~dp0build\version.dll" "%EARTHVR_DIR%\version.dll"
copy /Y "%~dp0build\version.dll" "%EARTHVR_DIR%\avrt.dll"
if not exist "%EARTHVR_DIR%\earthvr_mp.ini" (
    copy /Y "%~dp0earthvr_mp.ini" "%EARTHVR_DIR%\earthvr_mp.ini"
)
if not exist "%EARTHVR_DIR%\steam_appid.txt" (
    echo 348250> "%EARTHVR_DIR%\steam_appid.txt"
)
if exist "%~dp0steam_api64.dll" (
    if not exist "%EARTHVR_DIR%\steam_api64.dll" (
        copy /Y "%~dp0steam_api64.dll" "%EARTHVR_DIR%\steam_api64.dll"
    )
)

echo Mod successfully installed to "%EARTHVR_DIR%"!
