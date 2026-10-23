#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <unknwn.h>
#include <gdiplus.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>

#include "embedded_payload.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

constexpr const wchar_t* kWindowClassName = L"EarthVRMultiplayerInstallerWnd";
constexpr const wchar_t* kWindowTitle     = L"Google Earth VR Multiplayer — Setup";

constexpr const char* kDefaultIniContents =
    "[Multiplayer]\r\n"
    "; Google Earth VR Multiplayer Mod Configuration\r\n"
    "; Use the in-game MULTIPLAYER tab on the Google Earth VR menu (or press TAB / F4) to Host, Lock, Kick, or Join sessions.\r\n"
    "UDPPort=27088\r\n"
    "\r\n"
    "[Visuals]\r\n"
    "Show3DAvatars=1\r\n"
    "ShowXRayThroughEarth=1\r\n"
    "ShowFloatingMarkers=1\r\n";

struct AppState {
    HWND hwnd = nullptr;
    std::wstring install_dir;
    bool earth_exe_found = false;
    bool mod_installed = false;
    std::wstring status_message = L"Ready.";
    bool status_is_error = false;
    bool status_is_success = false;

    int hovered_btn = 0; // 0=none, 1=Browse, 2=Install/Reinstall, 3=Uninstall
    int pressed_btn = 0;

    Gdiplus::RectF rect_browse{468.0f, 152.0f, 108.0f, 38.0f};
    Gdiplus::RectF rect_primary{44.0f, 268.0f, 532.0f, 52.0f};
    Gdiplus::RectF rect_reinstall{44.0f, 268.0f, 258.0f, 52.0f};
    Gdiplus::RectF rect_uninstall{318.0f, 268.0f, 258.0f, 52.0f};
} g_app;

bool FileExistsW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool DirExistsW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring TrimSlashes(std::wstring p) {
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) {
        p.pop_back();
    }
    for (auto& c : p) {
        if (c == L'/') c = L'\\';
    }
    return p;
}

bool IsValidEarthVRFolder(const std::wstring& dir) {
    if (dir.empty()) return false;
    std::wstring clean = TrimSlashes(dir);
    return FileExistsW(clean + L"\\Earth.exe");
}

bool IsModInstalledInFolder(const std::wstring& dir) {
    if (dir.empty()) return false;
    std::wstring clean = TrimSlashes(dir);
    return FileExistsW(clean + L"\\version.dll");
}

// Parse Steam's libraryfolders.vdf to discover all Steam library paths across drives
std::vector<std::wstring> GetSteamLibraryFolders(const std::wstring& steam_root) {
    std::vector<std::wstring> libs;
    if (!steam_root.empty()) {
        libs.push_back(TrimSlashes(steam_root));
    }
    std::wstring vdf_path = TrimSlashes(steam_root) + L"\\steamapps\\libraryfolders.vdf";
    std::ifstream f(vdf_path);
    if (!f.is_open()) return libs;

    std::string line;
    while (std::getline(f, line)) {
        size_t pos = line.find("\"path\"");
        if (pos != std::string::npos) {
            size_t q1 = line.find('"', pos + 6);
            size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1) {
                std::string raw = line.substr(q1 + 1, q2 - q1 - 1);
                std::wstring wpath;
                for (size_t i = 0; i < raw.size(); ++i) {
                    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') {
                        wpath.push_back(L'\\');
                        ++i;
                    } else {
                        wpath.push_back(static_cast<wchar_t>(static_cast<unsigned char>(raw[i])));
                    }
                }
                if (!wpath.empty()) {
                    libs.push_back(TrimSlashes(wpath));
                }
            }
        }
    }
    return libs;
}

std::wstring AutoDetectEarthVRFolder() {
    // 1. Check Steam Registry keys + libraryfolders.vdf
    HKEY hKey = nullptr;
    wchar_t steam_path[MAX_PATH] = {};
    DWORD sz = sizeof(steam_path);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steam_path), &sz) == ERROR_SUCCESS) {
            for (const auto& lib : GetSteamLibraryFolders(steam_path)) {
                std::wstring candidate = lib + L"\\steamapps\\common\\EarthVR";
                if (IsValidEarthVRFolder(candidate)) {
                    RegCloseKey(hKey);
                    return candidate;
                }
            }
        }
        RegCloseKey(hKey);
    }

    sz = sizeof(steam_path);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"InstallPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steam_path), &sz) == ERROR_SUCCESS) {
            for (const auto& lib : GetSteamLibraryFolders(steam_path)) {
                std::wstring candidate = lib + L"\\steamapps\\common\\EarthVR";
                if (IsValidEarthVRFolder(candidate)) {
                    RegCloseKey(hKey);
                    return candidate;
                }
            }
        }
        RegCloseKey(hKey);
    }

    // 2. Scan fixed drives C:..Z: for common SteamLibrary locations
    for (wchar_t drive = L'C'; drive <= L'Z'; ++drive) {
        wchar_t root[4] = { drive, L':', L'\\', L'\0' };
        UINT dtype = GetDriveTypeW(root);
        if (dtype != DRIVE_FIXED && dtype != DRIVE_REMOVABLE) continue;

        const wchar_t* suffixes[] = {
            L":\\SteamLibrary\\steamapps\\common\\EarthVR",
            L":\\Program Files (x86)\\Steam\\steamapps\\common\\EarthVR",
            L":\\Program Files\\Steam\\steamapps\\common\\EarthVR",
            L":\\Steam\\steamapps\\common\\EarthVR",
            L":\\Games\\Steam\\steamapps\\common\\EarthVR",
            L":\\Games\\SteamLibrary\\steamapps\\common\\EarthVR"
        };
        for (const wchar_t* sfx : suffixes) {
            std::wstring candidate = std::wstring(1, drive) + sfx;
            if (IsValidEarthVRFolder(candidate)) {
                return candidate;
            }
        }
    }

    return L"";
}

void RefreshFolderState() {
    g_app.install_dir = TrimSlashes(g_app.install_dir);
    g_app.earth_exe_found = IsValidEarthVRFolder(g_app.install_dir);
    g_app.mod_installed   = g_app.earth_exe_found && IsModInstalledInFolder(g_app.install_dir);

    if (g_app.install_dir.empty()) {
        g_app.status_message = L"Please click Browse... and select your Google Earth VR installation folder.";
        g_app.status_is_error = false;
        g_app.status_is_success = false;
    } else if (!g_app.earth_exe_found) {
        g_app.status_message = L"Earth.exe was not found in the selected folder. Please select the 'EarthVR' folder.";
        g_app.status_is_error = true;
        g_app.status_is_success = false;
    } else if (g_app.mod_installed) {
        g_app.status_message = L"Google Earth VR Multiplayer is currently installed in this folder.";
        g_app.status_is_error = false;
        g_app.status_is_success = true;
    } else {
        g_app.status_message = L"Google Earth VR installation verified. Ready to install Multiplayer.";
        g_app.status_is_error = false;
        g_app.status_is_success = false;
    }
}

bool WriteBinaryFile(const std::wstring& path, const void* data, size_t size) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(hFile);
    return (ok && written == static_cast<DWORD>(size));
}

void PerformInstallOrReinstall(bool is_reinstall) {
    if (!g_app.earth_exe_found) {
        g_app.status_message = L"Cannot install: Earth.exe not found in the selected folder.";
        g_app.status_is_error = true;
        g_app.status_is_success = false;
        InvalidateRect(g_app.hwnd, nullptr, FALSE);
        return;
    }

    std::wstring dir = TrimSlashes(g_app.install_dir);
    std::wstring version_path = dir + L"\\version.dll";
    std::wstring avrt_path    = dir + L"\\avrt.dll";
    std::wstring steam_path   = dir + L"\\steam_api64.dll";
    std::wstring appid_path   = dir + L"\\steam_appid.txt";
    std::wstring ini_path     = dir + L"\\earthvr_mp.ini";

    if (!WriteBinaryFile(version_path, EmbeddedPayload::kVersionDllBytes, sizeof(EmbeddedPayload::kVersionDllBytes))) {
        g_app.status_message = L"Failed to write version.dll. Make sure Google Earth VR is closed and try again.";
        g_app.status_is_error = true;
        g_app.status_is_success = false;
        InvalidateRect(g_app.hwnd, nullptr, FALSE);
        return;
    }

    WriteBinaryFile(avrt_path, EmbeddedPayload::kVersionDllBytes, sizeof(EmbeddedPayload::kVersionDllBytes));

    if (!WriteBinaryFile(steam_path, EmbeddedPayload::kSteamApi64DllBytes, sizeof(EmbeddedPayload::kSteamApi64DllBytes))) {
        g_app.status_message = L"Failed to write steam_api64.dll. Make sure Google Earth VR is closed.";
        g_app.status_is_error = true;
        g_app.status_is_success = false;
        InvalidateRect(g_app.hwnd, nullptr, FALSE);
        return;
    }

    const char* appid_str = "348250\r\n";
    WriteBinaryFile(appid_path, appid_str, std::strlen(appid_str));

    if (!FileExistsW(ini_path)) {
        WriteBinaryFile(ini_path, kDefaultIniContents, std::strlen(kDefaultIniContents));
    }

    g_app.mod_installed = IsModInstalledInFolder(dir);
    g_app.status_is_error = false;
    g_app.status_is_success = true;
    g_app.status_message = is_reinstall
        ? L"Successfully reinstalled Google Earth VR Multiplayer! Launch Earth VR and open the in-game Menu."
        : L"Successfully installed Google Earth VR Multiplayer! Launch Earth VR and open the in-game Menu.";
    InvalidateRect(g_app.hwnd, nullptr, FALSE);
}

void PerformUninstall() {
    if (g_app.install_dir.empty()) return;
    std::wstring dir = TrimSlashes(g_app.install_dir);

    std::wstring version_path = dir + L"\\version.dll";
    std::wstring avrt_path    = dir + L"\\avrt.dll";
    std::wstring steam_path   = dir + L"\\steam_api64.dll";
    std::wstring appid_path   = dir + L"\\steam_appid.txt";
    std::wstring ini_path     = dir + L"\\earthvr_mp.ini";
    std::wstring log_path     = dir + L"\\earthvr_mp.log";

    if (FileExistsW(version_path) && !DeleteFileW(version_path.c_str())) {
        g_app.status_message = L"Could not remove version.dll. Please close Google Earth VR first and try again.";
        g_app.status_is_error = true;
        g_app.status_is_success = false;
        InvalidateRect(g_app.hwnd, nullptr, FALSE);
        return;
    }

    DeleteFileW(avrt_path.c_str());
    DeleteFileW(steam_path.c_str());
    DeleteFileW(appid_path.c_str());
    DeleteFileW(ini_path.c_str());
    DeleteFileW(log_path.c_str());

    g_app.mod_installed = IsModInstalledInFolder(dir);
    g_app.status_is_error = false;
    g_app.status_is_success = true;
    g_app.status_message = L"Uninstalled Google Earth VR Multiplayer cleanly from the installation folder.";
    InvalidateRect(g_app.hwnd, nullptr, FALSE);
}

void OpenFolderBrowserDialog(HWND owner) {
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr) && pfd) {
        DWORD options = 0;
        if (SUCCEEDED(pfd->GetOptions(&options))) {
            pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        }
        pfd->SetTitle(L"Select Google Earth VR Installation Folder (containing Earth.exe)");

        if (!g_app.install_dir.empty() && DirExistsW(g_app.install_dir)) {
            IShellItem* psiFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(g_app.install_dir.c_str(), nullptr, IID_PPV_ARGS(&psiFolder))) && psiFolder) {
                pfd->SetFolder(psiFolder);
                psiFolder->Release();
            }
        }

        if (SUCCEEDED(pfd->Show(owner))) {
            IShellItem* psiResult = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psiResult)) && psiResult) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                    g_app.install_dir = pszPath;
                    CoTaskMemFree(pszPath);
                    RefreshFolderState();
                    InvalidateRect(owner, nullptr, FALSE);
                }
                psiResult->Release();
            }
        }
        pfd->Release();
    }
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
    float d = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void FillRoundedRect(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius, const Gdiplus::Color& color) {
    Gdiplus::GraphicsPath path;
    AddRoundedRect(path, rect, radius);
    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

void DrawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius, const Gdiplus::Color& color, float width = 1.0f) {
    Gdiplus::GraphicsPath path;
    AddRoundedRect(path, rect, radius);
    Gdiplus::Pen pen(color, width);
    g.DrawPath(&pen, &path);
}

void RenderUI(HDC hdc, int width, int height) {
    // Offscreen double-buffering to prevent any flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    Gdiplus::Graphics g(memDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // 1. Deep space gradient background
    Gdiplus::LinearGradientBrush bgBrush(
        Gdiplus::Point(0, 0),
        Gdiplus::Point(0, height),
        Gdiplus::Color(255, 11, 16, 28),
        Gdiplus::Color(255, 17, 25, 42)
    );
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Subtle top accent glow bar
    Gdiplus::LinearGradientBrush topBarBrush(
        Gdiplus::Point(0, 0),
        Gdiplus::Point(width, 0),
        Gdiplus::Color(255, 38, 132, 255),
        Gdiplus::Color(255, 16, 215, 162)
    );
    g.FillRectangle(&topBarBrush, 0, 0, width, 4);

    // 2. Stylized Earth Globe Icon + Orbital Ring in Header
    float globe_cx = 64.0f, globe_cy = 58.0f, globe_r = 22.0f;
    Gdiplus::SolidBrush globeGlow(Gdiplus::Color(40, 48, 165, 255));
    g.FillEllipse(&globeGlow, globe_cx - globe_r - 6.0f, globe_cy - globe_r - 6.0f, (globe_r + 6.0f) * 2.0f, (globe_r + 6.0f) * 2.0f);

    Gdiplus::LinearGradientBrush globeBrush(
        Gdiplus::PointF(globe_cx - globe_r, globe_cy - globe_r),
        Gdiplus::PointF(globe_cx + globe_r, globe_cy + globe_r),
        Gdiplus::Color(255, 36, 156, 255),
        Gdiplus::Color(255, 18, 92, 198)
    );
    g.FillEllipse(&globeBrush, globe_cx - globe_r, globe_cy - globe_r, globe_r * 2.0f, globe_r * 2.0f);

    Gdiplus::Pen gridPen(Gdiplus::Color(115, 190, 235, 255), 1.2f);
    g.DrawEllipse(&gridPen, globe_cx - globe_r * 0.55f, globe_cy - globe_r, globe_r * 1.1f, globe_r * 2.0f);
    g.DrawLine(&gridPen, globe_cx - globe_r, globe_cy, globe_cx + globe_r, globe_cy);
    Gdiplus::Pen ringPen(Gdiplus::Color(210, 52, 230, 175), 2.0f);
    g.DrawEllipse(&ringPen, globe_cx - globe_r - 4.0f, globe_cy - 7.0f, (globe_r + 4.0f) * 2.0f, 14.0f);

    // Header Typography
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font fontTitle(&fontFamily, 20.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fontSub(&fontFamily, 12.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fontLabel(&fontFamily, 11.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fontPath(&fontFamily, 13.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fontBtn(&fontFamily, 14.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fontStatus(&fontFamily, 12.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 245, 248, 255));
    Gdiplus::SolidBrush mutedBrush(Gdiplus::Color(255, 152, 168, 195));

    g.DrawString(L"Google Earth VR Multiplayer", -1, &fontTitle, Gdiplus::PointF(102.0f, 32.0f), &whiteBrush);
    g.DrawString(L"Planetary-Scale 3D Avatars  \x2022  Global Voice Chat  \x2022  In-Game Server Browser",
                 -1, &fontSub, Gdiplus::PointF(103.0f, 61.0f), &mutedBrush);

    // 3. Installation Folder Card
    Gdiplus::RectF folderCard(28.0f, 106.0f, 564.0f, 130.0f);
    FillRoundedRect(g, folderCard, 12.0f, Gdiplus::Color(255, 21, 30, 49));
    DrawRoundedRect(g, folderCard, 12.0f, Gdiplus::Color(255, 42, 58, 90), 1.2f);

    Gdiplus::SolidBrush labelBrush(Gdiplus::Color(255, 110, 178, 255));
    g.DrawString(L"GOOGLE EARTH VR INSTALLATION FOLDER", -1, &fontLabel, Gdiplus::PointF(44.0f, 122.0f), &labelBrush);

    // Path Box
    Gdiplus::RectF pathBox(44.0f, 152.0f, 414.0f, 38.0f);
    FillRoundedRect(g, pathBox, 8.0f, Gdiplus::Color(255, 12, 18, 31));
    DrawRoundedRect(g, pathBox, 8.0f,
                    g_app.earth_exe_found ? Gdiplus::Color(255, 40, 160, 115) : Gdiplus::Color(255, 65, 82, 118),
                    1.3f);

    Gdiplus::StringFormat pathFormat;
    pathFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    pathFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
    pathFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    std::wstring shown_path = g_app.install_dir.empty() ? L"Click Browse... to select your EarthVR folder" : g_app.install_dir;
    Gdiplus::RectF pathTextRect(pathBox.X + 12.0f, pathBox.Y, pathBox.Width - 24.0f, pathBox.Height);
    g.DrawString(shown_path.c_str(), -1, &fontPath, pathTextRect, &pathFormat,
                 g_app.install_dir.empty() ? &mutedBrush : &whiteBrush);

    // Browse... Button
    bool hover_browse = (g_app.hovered_btn == 1);
    FillRoundedRect(g, g_app.rect_browse, 8.0f,
                    hover_browse ? Gdiplus::Color(255, 52, 74, 116) : Gdiplus::Color(255, 36, 52, 84));
    DrawRoundedRect(g, g_app.rect_browse, 8.0f, Gdiplus::Color(255, 82, 114, 172), 1.2f);

    Gdiplus::StringFormat centerFormat;
    centerFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
    centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Browse...", -1, &fontPath, g_app.rect_browse, &centerFormat, &whiteBrush);

    // Detection Badge inside Card
    std::wstring badge_text;
    Gdiplus::Color badge_col;
    if (g_app.earth_exe_found && g_app.mod_installed) {
        badge_text = L"\x2713  Earth.exe verified  \x2022  Multiplayer Mod is currently INSTALLED";
        badge_col = Gdiplus::Color(255, 56, 228, 162);
    } else if (g_app.earth_exe_found) {
        badge_text = L"\x2713  Earth.exe verified  \x2022  Ready to install Multiplayer Mod";
        badge_col = Gdiplus::Color(255, 92, 205, 255);
    } else {
        badge_text = L"\x26A0  Earth.exe not found in this directory";
        badge_col = Gdiplus::Color(255, 255, 180, 76);
    }
    Gdiplus::SolidBrush badgeBrush(badge_col);
    g.DrawString(badge_text.c_str(), -1, &fontStatus, Gdiplus::PointF(46.0f, 202.0f), &badgeBrush);

    // 4. Primary Action Buttons
    if (!g_app.mod_installed) {
        // Single "Install Google Earth VR Multiplayer" button
        bool hov = (g_app.hovered_btn == 2) && g_app.earth_exe_found;
        Gdiplus::Color c1 = !g_app.earth_exe_found
            ? Gdiplus::Color(255, 42, 52, 72)
            : (hov ? Gdiplus::Color(255, 38, 162, 255) : Gdiplus::Color(255, 24, 132, 235));
        Gdiplus::Color c2 = !g_app.earth_exe_found
            ? Gdiplus::Color(255, 36, 44, 62)
            : (hov ? Gdiplus::Color(255, 22, 205, 148) : Gdiplus::Color(255, 16, 178, 126));

        Gdiplus::LinearGradientBrush btnBrush(
            Gdiplus::PointF(g_app.rect_primary.X, g_app.rect_primary.Y),
            Gdiplus::PointF(g_app.rect_primary.X + g_app.rect_primary.Width, g_app.rect_primary.Y),
            c1, c2
        );
        Gdiplus::GraphicsPath btnPath;
        AddRoundedRect(btnPath, g_app.rect_primary, 10.0f);
        g.FillPath(&btnBrush, &btnPath);
        DrawRoundedRect(g, g_app.rect_primary, 10.0f,
                        g_app.earth_exe_found ? Gdiplus::Color(200, 130, 235, 255) : Gdiplus::Color(120, 80, 95, 120),
                        1.4f);

        g.DrawString(L"Install Google Earth VR Multiplayer", -1, &fontBtn,
                     g_app.rect_primary, &centerFormat,
                     g_app.earth_exe_found ? &whiteBrush : &mutedBrush);
    } else {
        // Two buttons: "Reinstall Google Earth VR Multiplayer" and "Uninstall Google Earth VR Multiplayer"
        Gdiplus::Font fontDualBtn(&fontFamily, 12.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        // Button 1: Reinstall
        bool hov_re = (g_app.hovered_btn == 2);
        FillRoundedRect(g, g_app.rect_reinstall, 10.0f,
                        hov_re ? Gdiplus::Color(255, 34, 142, 240) : Gdiplus::Color(255, 24, 114, 205));
        DrawRoundedRect(g, g_app.rect_reinstall, 10.0f, Gdiplus::Color(220, 110, 195, 255), 1.4f);
        g.DrawString(L"Reinstall Google Earth VR Multiplayer", -1, &fontDualBtn,
                     g_app.rect_reinstall, &centerFormat, &whiteBrush);

        // Button 2: Uninstall
        bool hov_un = (g_app.hovered_btn == 3);
        FillRoundedRect(g, g_app.rect_uninstall, 10.0f,
                        hov_un ? Gdiplus::Color(255, 185, 44, 62) : Gdiplus::Color(255, 135, 32, 48));
        DrawRoundedRect(g, g_app.rect_uninstall, 10.0f, Gdiplus::Color(220, 245, 110, 125), 1.4f);
        g.DrawString(L"Uninstall Google Earth VR Multiplayer", -1, &fontDualBtn,
                     g_app.rect_uninstall, &centerFormat, &whiteBrush);
    }

    // 5. Bottom Status Bar
    Gdiplus::RectF statusCard(28.0f, 340.0f, 564.0f, 44.0f);
    FillRoundedRect(g, statusCard, 8.0f, Gdiplus::Color(255, 14, 20, 34));
    DrawRoundedRect(g, statusCard, 8.0f, Gdiplus::Color(255, 34, 48, 76), 1.0f);

    Gdiplus::Color stColor = g_app.status_is_error
        ? Gdiplus::Color(255, 255, 115, 115)
        : (g_app.status_is_success ? Gdiplus::Color(255, 72, 235, 168) : Gdiplus::Color(255, 175, 192, 220));
    Gdiplus::SolidBrush stBrush(stColor);

    Gdiplus::RectF statusTextRect(statusCard.X + 14.0f, statusCard.Y, statusCard.Width - 28.0f, statusCard.Height);
    g.DrawString(g_app.status_message.c_str(), -1, &fontStatus, statusTextRect, &pathFormat, &stBrush);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

int HitTestButtons(int x, int y) {
    Gdiplus::PointF pt(static_cast<float>(x), static_cast<float>(y));
    if (g_app.rect_browse.Contains(pt)) return 1;
    if (!g_app.mod_installed) {
        if (g_app.earth_exe_found && g_app.rect_primary.Contains(pt)) return 2;
    } else {
        if (g_app.rect_reinstall.Contains(pt)) return 2;
        if (g_app.rect_uninstall.Contains(pt)) return 3;
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_app.hwnd = hwnd;
            return 0;

        case WM_ERASEBKGND:
            return 1; // Handled in double-buffered WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            RenderUI(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int mx = static_cast<short>(LOWORD(lParam));
            int my = static_cast<short>(HIWORD(lParam));
            int hit = HitTestButtons(mx, my);
            SetCursor(LoadCursorW(nullptr, hit > 0 ? IDC_HAND : IDC_ARROW));
            if (hit != g_app.hovered_btn) {
                g_app.hovered_btn = hit;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int mx = static_cast<short>(LOWORD(lParam));
            int my = static_cast<short>(HIWORD(lParam));
            g_app.pressed_btn = HitTestButtons(mx, my);
            return 0;
        }

        case WM_LBUTTONUP: {
            int mx = static_cast<short>(LOWORD(lParam));
            int my = static_cast<short>(HIWORD(lParam));
            int hit = HitTestButtons(mx, my);
            if (hit > 0 && hit == g_app.pressed_btn) {
                if (hit == 1) {
                    OpenFolderBrowserDialog(hwnd);
                } else if (hit == 2) {
                    PerformInstallOrReinstall(g_app.mod_installed);
                } else if (hit == 3 && g_app.mod_installed) {
                    PerformUninstall();
                }
            }
            g_app.pressed_btn = 0;
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    // Enable Per-Monitor DPI awareness if available
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        using fn_SetProcessDpiAwarenessContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto fn = reinterpret_cast<fn_SetProcessDpiAwarenessContext>(
            GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
        if (fn) {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Automatically search for Google Earth VR at startup
    g_app.install_dir = AutoDetectEarthVRFolder();
    RefreshFolderState();

    // Headless self-test mode for automated build verification
    if (lpCmdLine && wcsstr(lpCmdLine, L"--self-test") != nullptr) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        bool payload_ok = (sizeof(EmbeddedPayload::kVersionDllBytes) > 1024 &&
                           sizeof(EmbeddedPayload::kSteamApi64DllBytes) > 1024 &&
                           !g_app.install_dir.empty());
        return payload_ok ? 0 : 2;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = { 0, 0, 620, 412 };
    AdjustWindowRect(&rc, style, FALSE);
    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;

    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (scr_w - win_w) / 2;
    int pos_y = (scr_h - win_h) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        style,
        pos_x, pos_y, win_w, win_h,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
