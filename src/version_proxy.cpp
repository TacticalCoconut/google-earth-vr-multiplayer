#include <windows.h>
#include <cstdint>
#include <string>
#include <algorithm>
#include "earth_hook.h"

// Proxy forwarding table for all standard Windows version.dll exports
namespace {

HMODULE g_real_version_dll = nullptr;
HMODULE g_real_avrt_dll = nullptr;
HMODULE g_self_module = nullptr;

FARPROC g_pAvSetMmThreadCharacteristicsW = nullptr;
FARPROC g_pAvSetMmThreadCharacteristicsA = nullptr;
FARPROC g_pAvRevertMmThreadCharacteristics = nullptr;

FARPROC g_pGetFileVersionInfoA = nullptr;
FARPROC g_pGetFileVersionInfoByHandle = nullptr;
FARPROC g_pGetFileVersionInfoExA = nullptr;
FARPROC g_pGetFileVersionInfoExW = nullptr;
FARPROC g_pGetFileVersionInfoSizeA = nullptr;
FARPROC g_pGetFileVersionInfoSizeExA = nullptr;
FARPROC g_pGetFileVersionInfoSizeExW = nullptr;
FARPROC g_pGetFileVersionInfoSizeW = nullptr;
FARPROC g_pGetFileVersionInfoW = nullptr;
FARPROC g_pVerFindFileA = nullptr;
FARPROC g_pVerFindFileW = nullptr;
FARPROC g_pVerInstallFileA = nullptr;
FARPROC g_pVerInstallFileW = nullptr;
FARPROC g_pVerLanguageNameA = nullptr;
FARPROC g_pVerLanguageNameW = nullptr;
FARPROC g_pVerQueryValueA = nullptr;
FARPROC g_pVerQueryValueW = nullptr;

void LoadRealVersionDll() {
    if (g_real_version_dll) return;
    char sys_dir[MAX_PATH] = {};
    GetSystemDirectoryA(sys_dir, MAX_PATH);
    std::string full_path = std::string(sys_dir) + "\\version.dll";
    g_real_version_dll = LoadLibraryA(full_path.c_str());
    if (!g_real_version_dll) return;

    g_pGetFileVersionInfoA       = GetProcAddress(g_real_version_dll, "GetFileVersionInfoA");
    g_pGetFileVersionInfoByHandle= GetProcAddress(g_real_version_dll, "GetFileVersionInfoByHandle");
    g_pGetFileVersionInfoExA     = GetProcAddress(g_real_version_dll, "GetFileVersionInfoExA");
    g_pGetFileVersionInfoExW     = GetProcAddress(g_real_version_dll, "GetFileVersionInfoExW");
    g_pGetFileVersionInfoSizeA   = GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeA");
    g_pGetFileVersionInfoSizeExA = GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeExA");
    g_pGetFileVersionInfoSizeExW = GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeExW");
    g_pGetFileVersionInfoSizeW   = GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeW");
    g_pGetFileVersionInfoW       = GetProcAddress(g_real_version_dll, "GetFileVersionInfoW");
    g_pVerFindFileA              = GetProcAddress(g_real_version_dll, "VerFindFileA");
    g_pVerFindFileW              = GetProcAddress(g_real_version_dll, "VerFindFileW");
    g_pVerInstallFileA           = GetProcAddress(g_real_version_dll, "VerInstallFileA");
    g_pVerInstallFileW           = GetProcAddress(g_real_version_dll, "VerInstallFileW");
    g_pVerLanguageNameA          = GetProcAddress(g_real_version_dll, "VerLanguageNameA");
    g_pVerLanguageNameW          = GetProcAddress(g_real_version_dll, "VerLanguageNameW");
    g_pVerQueryValueA            = GetProcAddress(g_real_version_dll, "VerQueryValueA");
    g_pVerQueryValueW            = GetProcAddress(g_real_version_dll, "VerQueryValueW");
}

void LoadRealAvrtDll() {
    if (g_real_avrt_dll) return;
    char sys_dir[MAX_PATH] = {};
    GetSystemDirectoryA(sys_dir, MAX_PATH);
    std::string full_path = std::string(sys_dir) + "\\avrt.dll";
    g_real_avrt_dll = LoadLibraryA(full_path.c_str());
    if (!g_real_avrt_dll) return;

    g_pAvSetMmThreadCharacteristicsW   = GetProcAddress(g_real_avrt_dll, "AvSetMmThreadCharacteristicsW");
    g_pAvSetMmThreadCharacteristicsA   = GetProcAddress(g_real_avrt_dll, "AvSetMmThreadCharacteristicsA");
    g_pAvRevertMmThreadCharacteristics = GetProcAddress(g_real_avrt_dll, "AvRevertMmThreadCharacteristics");
}

bool IsCurrentProcessEarthExe() {
    char exe_path[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exe_path, MAX_PATH)) return false;
    std::string s(exe_path);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s.size() >= 9 && s.substr(s.size() - 9) == "earth.exe";
}

std::string GetModuleDirectory(HMODULE hMod) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(hMod, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    return (pos != std::string::npos) ? s.substr(0, pos) : ".";
}

} // namespace

extern "C" {

BOOL WINAPI Proxy_GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(LPCSTR, DWORD, DWORD, LPVOID);
    return g_pGetFileVersionInfoA ? reinterpret_cast<fn>(g_pGetFileVersionInfoA)(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL WINAPI Proxy_GetFileVersionInfoByHandle(DWORD dwFlags, HANDLE hFile, LPVOID* lplpData, PDWORD pdwLen) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(DWORD, HANDLE, LPVOID*, PDWORD);
    return g_pGetFileVersionInfoByHandle ? reinterpret_cast<fn>(g_pGetFileVersionInfoByHandle)(dwFlags, hFile, lplpData, pdwLen) : FALSE;
}

BOOL WINAPI Proxy_GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    return g_pGetFileVersionInfoExA ? reinterpret_cast<fn>(g_pGetFileVersionInfoExA)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL WINAPI Proxy_GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    return g_pGetFileVersionInfoExW ? reinterpret_cast<fn>(g_pGetFileVersionInfoExW)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

DWORD WINAPI Proxy_GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(LPCSTR, LPDWORD);
    return g_pGetFileVersionInfoSizeA ? reinterpret_cast<fn>(g_pGetFileVersionInfoSizeA)(lptstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI Proxy_GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCSTR, LPDWORD);
    return g_pGetFileVersionInfoSizeExA ? reinterpret_cast<fn>(g_pGetFileVersionInfoSizeExA)(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI Proxy_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCWSTR, LPDWORD);
    return g_pGetFileVersionInfoSizeExW ? reinterpret_cast<fn>(g_pGetFileVersionInfoSizeExW)(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI Proxy_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(LPCWSTR, LPDWORD);
    return g_pGetFileVersionInfoSizeW ? reinterpret_cast<fn>(g_pGetFileVersionInfoSizeW)(lptstrFilename, lpdwHandle) : 0;
}

BOOL WINAPI Proxy_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
    return g_pGetFileVersionInfoW ? reinterpret_cast<fn>(g_pGetFileVersionInfoW)(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

DWORD WINAPI Proxy_VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir, LPSTR szCurDir, PUINT lpuCurDirLen, LPSTR szDestDir, PUINT lpuDestDirLen) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
    return g_pVerFindFileA ? reinterpret_cast<fn>(g_pVerFindFileA)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD WINAPI Proxy_VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir, LPWSTR szCurDir, PUINT lpuCurDirLen, LPWSTR szDestDir, PUINT lpuDestDirLen) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    return g_pVerFindFileW ? reinterpret_cast<fn>(g_pVerFindFileW)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD WINAPI Proxy_VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir, LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT lpuTmpFileLen) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
    return g_pVerInstallFileA ? reinterpret_cast<fn>(g_pVerInstallFileA)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD WINAPI Proxy_VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir, LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT lpuTmpFileLen) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
    return g_pVerInstallFileW ? reinterpret_cast<fn>(g_pVerInstallFileW)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD WINAPI Proxy_VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPSTR, DWORD);
    return g_pVerLanguageNameA ? reinterpret_cast<fn>(g_pVerLanguageNameA)(wLang, szLang, cchLang) : 0;
}

DWORD WINAPI Proxy_VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD cchLang) {
    LoadRealVersionDll();
    using fn = DWORD (WINAPI*)(DWORD, LPWSTR, DWORD);
    return g_pVerLanguageNameW ? reinterpret_cast<fn>(g_pVerLanguageNameW)(wLang, szLang, cchLang) : 0;
}

BOOL WINAPI Proxy_VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    return g_pVerQueryValueA ? reinterpret_cast<fn>(g_pVerQueryValueA)(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

BOOL WINAPI Proxy_VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    LoadRealVersionDll();
    using fn = BOOL (WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    return g_pVerQueryValueW ? reinterpret_cast<fn>(g_pVerQueryValueW)(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

HANDLE WINAPI Proxy_AvSetMmThreadCharacteristicsW(LPCWSTR TaskName, LPDWORD TaskIndex) {
    LoadRealAvrtDll();
    using fn = HANDLE (WINAPI*)(LPCWSTR, LPDWORD);
    return g_pAvSetMmThreadCharacteristicsW ? reinterpret_cast<fn>(g_pAvSetMmThreadCharacteristicsW)(TaskName, TaskIndex) : nullptr;
}

HANDLE WINAPI Proxy_AvSetMmThreadCharacteristicsA(LPCSTR TaskName, LPDWORD TaskIndex) {
    LoadRealAvrtDll();
    using fn = HANDLE (WINAPI*)(LPCSTR, LPDWORD);
    return g_pAvSetMmThreadCharacteristicsA ? reinterpret_cast<fn>(g_pAvSetMmThreadCharacteristicsA)(TaskName, TaskIndex) : nullptr;
}

BOOL WINAPI Proxy_AvRevertMmThreadCharacteristics(HANDLE AvrtHandle) {
    LoadRealAvrtDll();
    using fn = BOOL (WINAPI*)(HANDLE);
    return g_pAvRevertMmThreadCharacteristics ? reinterpret_cast<fn>(g_pAvRevertMmThreadCharacteristics)(AvrtHandle) : FALSE;
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_self_module = hModule;
        LoadRealVersionDll();
        LoadRealAvrtDll();
        if (IsCurrentProcessEarthExe()) {
            EarthHook::InstallHookInDllMain(GetModuleDirectory(hModule));
        }
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (IsCurrentProcessEarthExe()) {
            EarthHook::ShutdownMod(lpReserved != nullptr);
        }
    }
    return TRUE;
}
