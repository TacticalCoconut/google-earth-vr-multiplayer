#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <atomic>

namespace EarthOpenVREmu {

#pragma pack(push, 8)
struct HmdMatrix34_t {
    float m[3][4];
};

struct HmdVector3_t {
    float v[3];
};

struct HmdVector2_t {
    float v[2];
};

struct DistortionCoordinates_t {
    float rfRed[2];
    float rfGreen[2];
    float rfBlue[2];
};

struct HiddenAreaMesh_t {
    const HmdVector2_t* pVertexData;
    uint32_t unTriangleCount;
};

struct TrackedDevicePose_t {
    HmdMatrix34_t mDeviceToAbsoluteTracking;
    HmdVector3_t vVelocity;
    HmdVector3_t vAngularVelocity;
    int32_t eTrackingResult;
    bool bPoseIsValid;
    bool bDeviceIsConnected;
};

struct VRControllerAxis_t {
    float x;
    float y;
};

struct VRControllerState001_t {
    uint32_t unPacketNum;
    uint64_t ulButtonPressed;
    uint64_t ulButtonTouched;
    VRControllerAxis_t rAxis[5];
};

struct Texture_t {
    void* handle;
    int32_t eType;
    int32_t eColorSpace;
};

struct VRTextureBounds_t {
    float uMin, vMin, uMax, vMax;
};
#pragma pack(pop)

using LogCallbackFn = void(*)(const char* fmt, ...);

struct EmulatorControlState {
    HmdMatrix34_t hmd_pose;
    HmdMatrix34_t ctrl_pose[2];
    uint64_t ctrl_buttons_pressed[2];
    uint64_t prev_ctrl_buttons_pressed[2];
    uint64_t ctrl_buttons_touched[2];
    VRControllerAxis_t ctrl_axes[2][5];
    uint32_t packet_num;
    uint32_t last_submitted_gl_tex[2];
    uint32_t frame_counter;
    bool custom_hmd_pose_set;
};

inline EmulatorControlState g_emu_state = {};
inline LogCallbackFn g_log_fn = nullptr;
inline bool g_emulation_active = false;

inline void SetIdentityPose(HmdMatrix34_t& m, float x, float y, float z) {
    std::memset(&m, 0, sizeof(m));
    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f;
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
}

inline void SetQuatPose(HmdMatrix34_t& m, float qw, float qx, float qy, float qz, float x, float y, float z) {
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    m.m[0][0] = 1.0f - 2.0f * (yy + zz);
    m.m[0][1] = 2.0f * (xy - wz);
    m.m[0][2] = 2.0f * (xz + wy);
    m.m[0][3] = x;
    m.m[1][0] = 2.0f * (xy + wz);
    m.m[1][1] = 1.0f - 2.0f * (xx + zz);
    m.m[1][2] = 2.0f * (yz - wx);
    m.m[1][3] = y;
    m.m[2][0] = 2.0f * (xz - wy);
    m.m[2][1] = 2.0f * (yz + wx);
    m.m[2][2] = 1.0f - 2.0f * (xx + yy);
    m.m[2][3] = z;
}

inline void InitDefaultEmulatedPoses() {
    SetIdentityPose(g_emu_state.hmd_pose, 0.0f, 1.65f, 0.0f);
    SetIdentityPose(g_emu_state.ctrl_pose[0], -0.22f, 1.38f, -0.45f);
    SetIdentityPose(g_emu_state.ctrl_pose[1],  0.22f, 1.38f, -0.45f);
    g_emu_state.packet_num = 1;
}

// ============================================================================
// IVRSystem_017 implementation
// ============================================================================
inline void __fastcall Sys_GetRecommendedRenderTargetSize(void*, uint32_t* pnWidth, uint32_t* pnHeight) {
    if (pnWidth)  *pnWidth  = 1280;
    if (pnHeight) *pnHeight = 720;
}

inline void __fastcall Sys_GetProjectionRaw(void*, int eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom) {
    (void)eEye;
    if (pfLeft)   *pfLeft   = -1.0f;
    if (pfRight)  *pfRight  =  1.0f;
    if (pfTop)    *pfTop    = -0.5625f;
    if (pfBottom) *pfBottom =  0.5625f;
}

inline bool __fastcall Sys_ComputeDistortion(void*, int eEye, float fU, float fV, DistortionCoordinates_t* pOut) {
    (void)eEye;
    if (pOut) {
        pOut->rfRed[0]   = fU; pOut->rfRed[1]   = fV;
        pOut->rfGreen[0] = fU; pOut->rfGreen[1] = fV;
        pOut->rfBlue[0]  = fU; pOut->rfBlue[1]  = fV;
    }
    return true;
}

inline HmdMatrix34_t* __fastcall Sys_GetEyeToHeadTransform(void*, HmdMatrix34_t* out_mat, int eEye) {
    if (out_mat) {
        SetIdentityPose(*out_mat, (eEye == 0) ? -0.032f : 0.032f, 0.0f, 0.0f);
    }
    return out_mat;
}

inline int __fastcall Sys_GetTrackedDeviceClass(void*, uint32_t unDeviceIndex) {
    if (unDeviceIndex == 0) return 1; // TrackedDeviceClass_HMD
    if (unDeviceIndex == 1 || unDeviceIndex == 2) return 2; // TrackedDeviceClass_Controller
    return 0; // TrackedDeviceClass_Invalid
}

inline bool __fastcall Sys_IsTrackedDeviceConnected(void*, uint32_t unDeviceIndex) {
    return unDeviceIndex <= 2;
}

inline float __fastcall Sys_GetFloatTrackedDeviceProperty(void*, uint32_t unDeviceIndex, int prop, int* pError) {
    (void)unDeviceIndex;
    if (pError) *pError = 0; // TrackedProp_Success
    if (prop == 0x7D2) return 90.0f;  // Prop_DisplayFrequency_Float
    if (prop == 0x7D1) return 0.011f; // Prop_SecondsFromVsyncToPhotons_Float
    if (prop == 0x7D3) return 0.064f; // Prop_UserIpdMeters_Float
    return 0.0f;
}

inline int32_t __fastcall Sys_GetInt32TrackedDeviceProperty(void*, uint32_t unDeviceIndex, int prop, int* pError) {
    if (pError) *pError = 0; // TrackedProp_Success
    if (prop == 3002) return 1; // Prop_Axis0Type_Int32 -> k_eControllerAxis_TrackPad
    if (prop == 3003) return 2; // Prop_Axis1Type_Int32 -> k_eControllerAxis_Trigger
    if (prop == 3004 || prop == 3005 || prop == 3006) return 0; // k_eControllerAxis_None
    if (prop == 3007) return (unDeviceIndex == 1) ? 1 : 2; // Prop_ControllerRoleHint_Int32
    return 0;
}

inline uint32_t __fastcall Sys_GetStringTrackedDeviceProperty(
    void*, uint32_t unDeviceIndex, int prop, char* pchValue, uint32_t unBufferSize, int* pError) {
    if (pError) *pError = 0; // TrackedProp_Success
    const char* val = "HTC";
    if (prop == 0x3ED) { // Prop_ManufacturerName_String (1005)
        val = "HTC";
    } else if (prop == 0x3E9) { // Prop_ModelNumber_String (1001)
        val = (unDeviceIndex == 0) ? "Vive MV" : "Vive Controller MV";
    } else if (prop == 0x3EB) { // Prop_RenderModelName_String (1003)
        val = (unDeviceIndex == 0) ? "generic_hmd" : "vr_controller_vive_1_5";
    } else if (prop == 0x3EA) { // Prop_SerialNumber_String (1002)
        val = (unDeviceIndex == 0) ? "LHR-00000000" : (unDeviceIndex == 1 ? "LHR-00000001" : "LHR-00000002");
    }
    uint32_t len = static_cast<uint32_t>(std::strlen(val)) + 1;
    if (pchValue && unBufferSize >= len) {
        std::memcpy(pchValue, val, len);
    } else if (pchValue && unBufferSize > 0) {
        std::memcpy(pchValue, val, unBufferSize - 1);
        pchValue[unBufferSize - 1] = '\0';
    }
    return len;
}

inline const char* __fastcall Sys_GetPropErrorNameFromEnum(void*, int error) {
    (void)error;
    return "TrackedProp_Success";
}

inline bool __fastcall Sys_PollNextEventWithPose(void*, int eOrigin, void* pEvent, uint32_t uncbVREvent, TrackedDevicePose_t* pPose) {
    (void)eOrigin;
    if (!pEvent || uncbVREvent < 0x18) return false;

    const uint32_t kButtonsToCheck[4] = { 33, 1, 2, 32 }; // Trigger, Menu, Grip, Touchpad
    for (int c = 0; c < 2; ++c) {
        uint64_t diff = g_emu_state.ctrl_buttons_pressed[c] ^ g_emu_state.prev_ctrl_buttons_pressed[c];
        if (diff == 0) continue;
        for (int b = 0; b < 4; ++b) {
            uint32_t btn_id = kButtonsToCheck[b];
            uint64_t mask = (1ULL << btn_id);
            if (diff & mask) {
                bool is_pressed = (g_emu_state.ctrl_buttons_pressed[c] & mask) != 0;
                if (is_pressed) {
                    g_emu_state.prev_ctrl_buttons_pressed[c] |= mask;
                } else {
                    g_emu_state.prev_ctrl_buttons_pressed[c] &= ~mask;
                }

                std::memset(pEvent, 0, uncbVREvent);
                uint32_t* ev32 = reinterpret_cast<uint32_t*>(pEvent);
                ev32[0] = is_pressed ? 200u : 201u; // VREvent_ButtonPress (200) / VREvent_ButtonUnpress (201)
                ev32[1] = static_cast<uint32_t>(c + 1); // trackedDeviceIndex: 1=Left, 2=Right
                *reinterpret_cast<float*>(&ev32[2]) = 0.0f; // eventAgeSeconds
                ev32[3] = 0;
                ev32[4] = btn_id; // VREvent_Controller_t::button

                if (pPose) {
                    std::memset(pPose, 0, sizeof(TrackedDevicePose_t));
                    pPose->mDeviceToAbsoluteTracking = g_emu_state.ctrl_pose[c];
                    pPose->eTrackingResult = 200; // TrackingResult_Running_OK
                    pPose->bPoseIsValid = true;
                    pPose->bDeviceIsConnected = true;
                }
                return true;
            }
        }
        g_emu_state.prev_ctrl_buttons_pressed[c] = g_emu_state.ctrl_buttons_pressed[c];
    }
    return false;
}

inline HiddenAreaMesh_t* __fastcall Sys_GetHiddenAreaMesh(void*, HiddenAreaMesh_t* out_mesh, int eEye, int type) {
    (void)eEye; (void)type;
    if (out_mesh) {
        out_mesh->pVertexData = nullptr;
        out_mesh->unTriangleCount = 0;
    }
    return out_mesh;
}

inline bool __fastcall Sys_GetControllerState(void*, uint32_t unControllerDeviceIndex, VRControllerState001_t* pState, uint32_t unSize) {
    if (!pState || unSize < sizeof(VRControllerState001_t)) return false;
    std::memset(pState, 0, sizeof(VRControllerState001_t));
    pState->unPacketNum = ++g_emu_state.packet_num;
    if (unControllerDeviceIndex == 1 || unControllerDeviceIndex == 2) {
        int c = (unControllerDeviceIndex == 1) ? 0 : 1;
        pState->ulButtonPressed = g_emu_state.ctrl_buttons_pressed[c];
        pState->ulButtonTouched = g_emu_state.ctrl_buttons_touched[c];
        for (int a = 0; a < 5; ++a) {
            pState->rAxis[a] = g_emu_state.ctrl_axes[c][a];
        }
    }
    return true;
}

inline void __fastcall Sys_TriggerHapticPulse(void*, uint32_t, uint32_t, unsigned short) {}

// ============================================================================
// IVRCompositor_021 implementation
// ============================================================================
inline void __fastcall Comp_SetTrackingSpace(void*, int) {}

inline int __fastcall Comp_WaitGetPoses(
    void*,
    TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount,
    TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) {

    // On frame >= 1, ensure HMD pose differs slightly from frame 0's exact identity pose
    // so Earth.exe's InitAppMode::Update (0x140862130) detects active HMD tracking immediately.
    if (g_emu_state.frame_counter == 1 && !g_emu_state.custom_hmd_pose_set) {
        SetQuatPose(g_emu_state.hmd_pose, 0.99985f, -0.015f, 0.008f, 0.0f, 0.0f, 1.652f, -0.01f);
    }

    auto fill_poses = [](TrackedDevicePose_t* arr, uint32_t count) {
        if (!arr) return;
        for (uint32_t i = 0; i < count; ++i) {
            std::memset(&arr[i], 0, sizeof(TrackedDevicePose_t));
            if (i == 0) {
                arr[i].mDeviceToAbsoluteTracking = g_emu_state.hmd_pose;
                arr[i].eTrackingResult = 200; // TrackingResult_Running_OK
                arr[i].bPoseIsValid = true;
                arr[i].bDeviceIsConnected = true;
            } else if (i == 1 || i == 2) {
                arr[i].mDeviceToAbsoluteTracking = g_emu_state.ctrl_pose[i - 1];
                arr[i].eTrackingResult = 200;
                arr[i].bPoseIsValid = true;
                arr[i].bDeviceIsConnected = true;
            } else {
                arr[i].eTrackingResult = 1; // TrackingResult_Uninitialized
                arr[i].bPoseIsValid = false;
                arr[i].bDeviceIsConnected = false;
            }
        }
    };

    fill_poses(pRenderPoseArray, unRenderPoseArrayCount);
    fill_poses(pGamePoseArray, unGamePoseArrayCount);
    g_emu_state.frame_counter++;
    Sleep(11); // Emulate 90Hz VSync pacing so Earth.exe background RockTree streaming threads run smoothly
    return 0; // VRCompositorError_None
}

inline int __fastcall Comp_Submit(void*, int eEye, const Texture_t* pTexture, const VRTextureBounds_t* pBounds, int nSubmitFlags) {
    (void)pBounds; (void)nSubmitFlags;
    if (pTexture && (eEye == 0 || eEye == 1)) {
        g_emu_state.last_submitted_gl_tex[eEye] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pTexture->handle));
    }
    return 0; // VRCompositorError_None
}

inline void __fastcall Comp_PostPresentHandoff(void*) {}

inline bool __fastcall Comp_GetFrameTiming(void*, void* pTiming, uint32_t unFramesAgo) {
    (void)unFramesAgo;
    if (pTiming) {
        uint32_t sz = *reinterpret_cast<uint32_t*>(pTiming);
        if (sz > 0 && sz <= 512) {
            std::memset(pTiming, 0, sz);
            *reinterpret_cast<uint32_t*>(pTiming) = sz;
        }
    }
    return true;
}

inline float __fastcall Comp_GetFrameTimeRemaining(void*) {
    return 0.008f;
}

inline void __fastcall Comp_GetCumulativeStats(void*, void* pStats, uint32_t nStatsSizeInBytes) {
    if (pStats && nStatsSizeInBytes > 0 && nStatsSizeInBytes <= 512) {
        std::memset(pStats, 0, nStatsSizeInBytes);
    }
}

inline void __fastcall Comp_ForceInterleavedReprojectionOn(void*, bool) {}
inline void __fastcall Comp_SuspendRendering(void*, bool) {}

// ============================================================================
// IVROverlay_016 implementation
// ============================================================================
inline int __fastcall Ovl_ShowKeyboard(void*, int, int, const char*, uint32_t, const char*, bool, uint64_t) {
    return 0;
}
inline void __fastcall Ovl_HideKeyboard(void*) {}
inline uint32_t __fastcall Ovl_GetKeyboardText(void*, char* pchText, uint32_t cchText) {
    if (pchText && cchText > 0) pchText[0] = '\0';
    return 0;
}

// Safe fallback for any other vtable slot
inline uint64_t __fastcall DummyVtableMethod(void*, void*, void*, void*) {
    return 0;
}

struct EmulatedInterfaceObj {
    void** vtable;
};

inline void* g_sys_vtable[64] = {};
inline void* g_comp_vtable[64] = {};
inline void* g_ovl_vtable[96] = {};

inline EmulatedInterfaceObj g_sys_obj  = { g_sys_vtable };
inline EmulatedInterfaceObj g_comp_obj = { g_comp_vtable };
inline EmulatedInterfaceObj g_ovl_obj  = { g_ovl_vtable };

inline void InitVtables() {
    for (int i = 0; i < 64; ++i) g_sys_vtable[i]  = reinterpret_cast<void*>(&DummyVtableMethod);
    for (int i = 0; i < 64; ++i) g_comp_vtable[i] = reinterpret_cast<void*>(&DummyVtableMethod);
    for (int i = 0; i < 96; ++i) g_ovl_vtable[i]  = reinterpret_cast<void*>(&DummyVtableMethod);

    g_sys_vtable[0]  = reinterpret_cast<void*>(&Sys_GetRecommendedRenderTargetSize);
    g_sys_vtable[2]  = reinterpret_cast<void*>(&Sys_GetProjectionRaw);
    g_sys_vtable[3]  = reinterpret_cast<void*>(&Sys_ComputeDistortion);
    g_sys_vtable[4]  = reinterpret_cast<void*>(&Sys_GetEyeToHeadTransform);
    g_sys_vtable[20] = reinterpret_cast<void*>(&Sys_GetTrackedDeviceClass);
    g_sys_vtable[21] = reinterpret_cast<void*>(&Sys_IsTrackedDeviceConnected);
    g_sys_vtable[23] = reinterpret_cast<void*>(&Sys_GetFloatTrackedDeviceProperty);
    g_sys_vtable[24] = reinterpret_cast<void*>(&Sys_GetInt32TrackedDeviceProperty);
    g_sys_vtable[27] = reinterpret_cast<void*>(&Sys_GetStringTrackedDeviceProperty);
    g_sys_vtable[28] = reinterpret_cast<void*>(&Sys_GetPropErrorNameFromEnum);
    g_sys_vtable[30] = reinterpret_cast<void*>(&Sys_PollNextEventWithPose);
    g_sys_vtable[32] = reinterpret_cast<void*>(&Sys_GetHiddenAreaMesh);
    g_sys_vtable[33] = reinterpret_cast<void*>(&Sys_GetControllerState);
    g_sys_vtable[35] = reinterpret_cast<void*>(&Sys_TriggerHapticPulse);

    g_comp_vtable[0]  = reinterpret_cast<void*>(&Comp_SetTrackingSpace);
    g_comp_vtable[2]  = reinterpret_cast<void*>(&Comp_WaitGetPoses);
    g_comp_vtable[5]  = reinterpret_cast<void*>(&Comp_Submit);
    g_comp_vtable[7]  = reinterpret_cast<void*>(&Comp_PostPresentHandoff);
    g_comp_vtable[8]  = reinterpret_cast<void*>(&Comp_GetFrameTiming);
    g_comp_vtable[10] = reinterpret_cast<void*>(&Comp_GetFrameTimeRemaining);
    g_comp_vtable[11] = reinterpret_cast<void*>(&Comp_GetCumulativeStats);
    g_comp_vtable[25] = reinterpret_cast<void*>(&Comp_ForceInterleavedReprojectionOn);
    g_comp_vtable[27] = reinterpret_cast<void*>(&Comp_SuspendRendering);

    g_ovl_vtable[71] = reinterpret_cast<void*>(&Ovl_ShowKeyboard);
    g_ovl_vtable[74] = reinterpret_cast<void*>(&Ovl_HideKeyboard);
    g_ovl_vtable[75] = reinterpret_cast<void*>(&Ovl_GetKeyboardText);
}

// OpenVR C API function pointer types
using fn_VR_GetInitToken            = uint32_t (__fastcall*)();
using fn_VR_IsInterfaceVersionValid = bool (__fastcall*)(const char* pchInterfaceVersion);
using fn_VR_InitInternal2           = uint32_t (__fastcall*)(int* peError, int eApplicationType, const char* pStartupInfo);
using fn_VR_ShutdownInternal        = void (__fastcall*)();
using fn_VR_GetGenericInterface     = void* (__fastcall*)(const char* pchInterfaceVersion, int* peError);
using fn_VR_GetVRInitErrorAsSymbol  = const char* (__fastcall*)(int error);

inline fn_VR_GetInitToken            g_orig_VR_GetInitToken = nullptr;
inline fn_VR_IsInterfaceVersionValid g_orig_VR_IsInterfaceVersionValid = nullptr;
inline fn_VR_InitInternal2           g_orig_VR_InitInternal2 = nullptr;
inline fn_VR_ShutdownInternal        g_orig_VR_ShutdownInternal = nullptr;
inline fn_VR_GetGenericInterface     g_orig_VR_GetGenericInterface = nullptr;
inline fn_VR_GetVRInitErrorAsSymbol  g_orig_VR_GetVRInitErrorAsSymbol = nullptr;

inline uint32_t __fastcall Hook_VR_GetInitToken() {
    if (g_emulation_active) return 1;
    return g_orig_VR_GetInitToken ? g_orig_VR_GetInitToken() : 1;
}

inline bool __fastcall Hook_VR_IsInterfaceVersionValid(const char* pchInterfaceVersion) {
    if (g_emulation_active) return true;
    return g_orig_VR_IsInterfaceVersionValid ? g_orig_VR_IsInterfaceVersionValid(pchInterfaceVersion) : true;
}

inline bool IsSteamVRServerRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"vrserver.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

inline uint32_t __fastcall Hook_VR_InitInternal2(int* peError, int eApplicationType, const char* pStartupInfo) {
    char env_buf[32] = {};
    bool force_emu = (GetEnvironmentVariableA("EARTHVR_EMULATE_VR", env_buf, sizeof(env_buf)) > 0 && env_buf[0] == '1');

    if (!force_emu && IsSteamVRServerRunning() && g_orig_VR_InitInternal2) {
        int err = 0;
        uint32_t token = g_orig_VR_InitInternal2(&err, eApplicationType, pStartupInfo);
        if (err == 0) {
            if (peError) *peError = 0;
            g_emulation_active = false;
            return token;
        }
    }

    // Physical HMD / SteamVR not connected (or EARTHVR_EMULATE_VR=1): activate built-in OpenVR Driver Emulator immediately
    InitVtables();
    InitDefaultEmulatedPoses();
    g_emulation_active = true;

    // Set FLAGS_config ("labels0" = Top of Half Dome, CA) so InitAppMode enters InteractiveAppMode (3D Earth) directly
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (base) {
        uintptr_t flags_config_ptr = *reinterpret_cast<uintptr_t*>(base + 0x7EFF0C8);
        if (flags_config_ptr) {
            uint64_t cap = *reinterpret_cast<uint64_t*>(flags_config_ptr + 24);
            if (cap == 15) {
                std::memcpy(reinterpret_cast<void*>(flags_config_ptr), "labels0", 8);
                *reinterpret_cast<uint64_t*>(flags_config_ptr + 16) = 7;
            }
        }
    }

    if (peError) *peError = 0; // VRInitError_None
    if (g_log_fn) {
        g_log_fn("[OpenVREmu] Activated built-in OpenVR HMD + Dual Vive Controller Driver inside Earth.exe");
    }
    return 1;
}

inline void __fastcall Hook_VR_ShutdownInternal() {
    if (!g_emulation_active && g_orig_VR_ShutdownInternal) {
        g_orig_VR_ShutdownInternal();
    }
}

inline void* __fastcall Hook_VR_GetGenericInterface(const char* pchInterfaceVersion, int* peError) {
    if (!g_emulation_active && g_orig_VR_GetGenericInterface) {
        return g_orig_VR_GetGenericInterface(pchInterfaceVersion, peError);
    }
    if (peError) *peError = 0;
    if (!pchInterfaceVersion) return nullptr;

    if (std::strstr(pchInterfaceVersion, "IVRSystem")) {
        return &g_sys_obj;
    }
    if (std::strstr(pchInterfaceVersion, "IVRCompositor")) {
        return &g_comp_obj;
    }
    if (std::strstr(pchInterfaceVersion, "IVROverlay")) {
        return &g_ovl_obj;
    }
    if (std::strstr(pchInterfaceVersion, "IVRExtendedDisplay")) {
        if (peError) *peError = 108;
        return nullptr;
    }
    return nullptr;
}

inline const char* __fastcall Hook_VR_GetVRInitErrorAsSymbol(int error) {
    if (!g_emulation_active && g_orig_VR_GetVRInitErrorAsSymbol) {
        return g_orig_VR_GetVRInitErrorAsSymbol(error);
    }
    return "VRInitError_None";
}

inline void PatchIATSlot(uintptr_t iat_addr, void* new_fn, void** out_orig) {
    DWORD old_prot = 0;
    if (VirtualProtect(reinterpret_cast<void*>(iat_addr), sizeof(void*), PAGE_READWRITE, &old_prot)) {
        if (out_orig) *out_orig = *reinterpret_cast<void**>(iat_addr);
        *reinterpret_cast<void**>(iat_addr) = new_fn;
        VirtualProtect(reinterpret_cast<void*>(iat_addr), sizeof(void*), old_prot, &old_prot);
    }
}

inline void InstallOpenVRIATHooks(uintptr_t exe_base, LogCallbackFn log_fn) {
    g_log_fn = log_fn;
    // Earth.exe OpenVR IAT RVAs:
    //   0xF1D478: VR_GetInitToken
    //   0xF1D480: VR_IsInterfaceVersionValid
    //   0xF1D488: VR_InitInternal2
    //   0xF1D490: VR_ShutdownInternal
    //   0xF1D498: VR_GetGenericInterface
    //   0xF1D4A0: VR_GetVRInitErrorAsSymbol
    PatchIATSlot(exe_base + 0xF1D478, reinterpret_cast<void*>(&Hook_VR_GetInitToken),            reinterpret_cast<void**>(&g_orig_VR_GetInitToken));
    PatchIATSlot(exe_base + 0xF1D480, reinterpret_cast<void*>(&Hook_VR_IsInterfaceVersionValid), reinterpret_cast<void**>(&g_orig_VR_IsInterfaceVersionValid));
    PatchIATSlot(exe_base + 0xF1D488, reinterpret_cast<void*>(&Hook_VR_InitInternal2),           reinterpret_cast<void**>(&g_orig_VR_InitInternal2));
    PatchIATSlot(exe_base + 0xF1D490, reinterpret_cast<void*>(&Hook_VR_ShutdownInternal),        reinterpret_cast<void**>(&g_orig_VR_ShutdownInternal));
    PatchIATSlot(exe_base + 0xF1D498, reinterpret_cast<void*>(&Hook_VR_GetGenericInterface),     reinterpret_cast<void**>(&g_orig_VR_GetGenericInterface));
    PatchIATSlot(exe_base + 0xF1D4A0, reinterpret_cast<void*>(&Hook_VR_GetVRInitErrorAsSymbol),  reinterpret_cast<void**>(&g_orig_VR_GetVRInitErrorAsSymbol));
}

} // namespace EarthOpenVREmu
