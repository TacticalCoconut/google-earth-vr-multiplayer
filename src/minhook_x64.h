#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace Hook64 {

struct TrampolineHook {
    void* target_func = nullptr;
    void* detour_func = nullptr;
    void* trampoline = nullptr;
    uint8_t original_bytes[32] = {};
    size_t stolen_len = 0;
    bool installed = false;

    // Writes a 14-byte absolute jump: FF 25 00 00 00 00 [64-bit address]
    static void WriteAbsJump(void* dst, const void* target) {
        uint8_t* p = reinterpret_cast<uint8_t*>(dst);
        p[0] = 0xFF;
        p[1] = 0x25;
        p[2] = 0x00;
        p[3] = 0x00;
        p[4] = 0x00;
        p[5] = 0x00;
        uint64_t addr = reinterpret_cast<uint64_t>(target);
        std::memcpy(p + 6, &addr, sizeof(uint64_t));
    }

    bool Install(void* target, void* detour, size_t prologue_bytes, void** out_original) {
        if (installed || !target || !detour || prologue_bytes < 14 || prologue_bytes > 32) {
            return false;
        }
        target_func = target;
        detour_func = detour;
        stolen_len = prologue_bytes;

        trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!trampoline) return false;

        std::memcpy(original_bytes, target_func, stolen_len);
        std::memcpy(trampoline, original_bytes, stolen_len);
        WriteAbsJump(reinterpret_cast<uint8_t*>(trampoline) + stolen_len,
                     reinterpret_cast<uint8_t*>(target_func) + stolen_len);
        FlushInstructionCache(GetCurrentProcess(), trampoline, 64);

        DWORD old_protect = 0;
        if (!VirtualProtect(target_func, stolen_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            trampoline = nullptr;
            return false;
        }

        WriteAbsJump(target_func, detour_func);
        for (size_t i = 14; i < stolen_len; ++i) {
            reinterpret_cast<uint8_t*>(target_func)[i] = 0x90; // NOP padding
        }

        VirtualProtect(target_func, stolen_len, old_protect, &old_protect);
        FlushInstructionCache(GetCurrentProcess(), target_func, stolen_len);

        if (out_original) {
            *out_original = trampoline;
        }
        installed = true;
        return true;
    }

    void Remove() {
        if (!installed || !target_func) return;
        DWORD old_protect = 0;
        if (VirtualProtect(target_func, stolen_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
            std::memcpy(target_func, original_bytes, stolen_len);
            VirtualProtect(target_func, stolen_len, old_protect, &old_protect);
            FlushInstructionCache(GetCurrentProcess(), target_func, stolen_len);
        }
        if (trampoline) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            trampoline = nullptr;
        }
        installed = false;
    }
};

} // namespace Hook64
