@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist build mkdir build

echo [1/5] Compiling version.dll (Google Earth VR Multiplayer Proxy Mod)...
cl.exe /nologo /O2 /Brepro /std:c++17 /EHa /MT /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD ^
    /I"src" ^
    src\version_proxy.cpp ^
    src\earth_hook.cpp ^
    src\network.cpp ^
    src\voice.cpp ^
    src\renderer.cpp ^
    /Fe"build\version.dll" ^
    /Fd"build\version.pdb" ^
    /Fo"build\\" ^
    /link /PDBALTPATH:version.pdb /DEF:"src\version.def" opengl32.lib winmm.lib ws2_32.lib bcrypt.lib user32.lib kernel32.lib
if errorlevel 1 exit /b 1

echo [2/5] Compiling test_multiplayer.exe...
cl.exe /nologo /O2 /Brepro /std:c++17 /EHsc /MT /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
    /I"src" ^
    tests\test_multiplayer.cpp ^
    src\network.cpp ^
    src\voice.cpp ^
    /Fe"build\test_multiplayer.exe" ^
    /Fo"build\\" ^
    /link /PDBALTPATH:test_multiplayer.pdb winmm.lib ws2_32.lib bcrypt.lib user32.lib kernel32.lib
if errorlevel 1 exit /b 1

echo [3/5] Running multiplayer and security verification suite...
"build\test_multiplayer.exe"
if errorlevel 1 exit /b 1

echo [4/5] Packing version.dll and steam_api64.dll into standalone installer payload...
python installer\pack_payload.py "build\version.dll" "C:\Program Files (x86)\Steam\steamapps\common\EarthVR\steam_api64.dll" "installer\embedded_payload.h"
if errorlevel 1 exit /b 1

echo [5/5] Compiling sleek standalone GUI client (EarthVR_Multiplayer_Installer.exe)...
cl.exe /nologo /O2 /Brepro /std:c++17 /EHsc /MT ^
    /I"installer" ^
    installer\installer_main.cpp ^
    /Fe"EarthVR_Multiplayer_Installer.exe" ^
    /Fo"build\\" ^
    /link /PDBALTPATH:EarthVR_Multiplayer_Installer.pdb /SUBSYSTEM:WINDOWS gdiplus.lib shlwapi.lib ole32.lib shell32.lib user32.lib gdi32.lib advapi32.lib
if errorlevel 1 exit /b 1

copy /Y "EarthVR_Multiplayer_Installer.exe" "build\EarthVR_Multiplayer_Installer.exe" >nul

echo Verifying installer auto-detection (--self-test)...
"EarthVR_Multiplayer_Installer.exe" --self-test
if errorlevel 1 exit /b 1

echo Build, security verification, and standalone installer packaging succeeded!
