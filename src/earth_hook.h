#pragma once
#include <string>

namespace EarthHook {

// Called synchronously in DllMain(DLL_PROCESS_ATTACH) before Earth.exe starts any threads.
// Only installs the trampoline in memory (loader-lock safe).
bool InstallHookInDllMain(const std::string& dll_dir);

// Called on DLL_PROCESS_DETACH.
void ShutdownMod(bool is_process_terminating);

} // namespace EarthHook
