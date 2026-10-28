#include "earth_hook.h"
#include "minhook_x64.h"
#include "earth_math.h"
#include "voice.h"
#include "network.h"
#include "renderer.h"
#include "openvr_emulation.h"
#include <windows.h>
#include <gl/GL.h>
#include <share.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <mutex>
#include <atomic>

namespace EarthHook {

namespace {

// Earth.exe RVA of CompositorImpl::RenderForeground(this, device, head_pose, controller_states)
constexpr uintptr_t kRva_CompositorRenderForeground = 0xAFE540;

// Expected 16-byte prologue at Earth.exe + 0xAFE540:
//   40 53                 push rbx
//   55                    push rbp
//   56                    push rsi
//   57                    push rdi
//   41 56                 push r14
//   41 57                 push r15
//   48 81 EC F8 00 00 00  sub rsp, 0xF8
const uint8_t kExpectedPrologue[16] = {
    0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41,
    0x57, 0x48, 0x81, 0xEC, 0xF8, 0x00, 0x00, 0x00
};

using fn_RenderForeground = void (__fastcall*)(void* compositor, void* device, const float* head_pose, void* controller_states);

Hook64::TrampolineHook g_fg_hook;
fn_RenderForeground g_orig_render_foreground = nullptr;

EarthVoice::GlobalVoiceManager    g_voice;
EarthNet::NetworkManager          g_network;
EarthRender::StereoAvatarRenderer g_renderer;

std::string g_dll_dir;
std::atomic<bool> g_subsystems_initialized{false};
FILE* g_log_file = nullptr;
std::mutex g_log_mutex;

bool g_key_down[256] = {};

void LogMsg(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_file && !g_dll_dir.empty()) {
        std::string path = g_dll_dir + "\\earthvr_mp.log";
        g_log_file = _fsopen(path.c_str(), "a", _SH_DENYNO);
    }
    if (!g_log_file) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    std::fprintf(g_log_file, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_log_file, fmt, args);
    va_end(args);

    std::fprintf(g_log_file, "\n");
    std::fflush(g_log_file);
}

void EnsureSubsystemsInitializedOnFirstFrame() {
    if (g_subsystems_initialized.exchange(true)) return;

    LogMsg("=== Google Earth VR Multiplayer Mod First-Frame Init ===");
    g_renderer.LoadConfig(g_dll_dir + "\\earthvr_mp.ini");
    g_network.Initialize(&g_voice, g_dll_dir);

    LogMsg("Subsystems active (LocalPlayer='%s', Steam=%s, PeerID=0x%llX)",
           g_network.GetLocalPlayerName().c_str(),
           g_network.IsSteamConnected() ? "Connected" : "UDP-Fallback",
           static_cast<unsigned long long>(g_network.GetLocalPeerId()));
    LogMsg("Controls: Open in-game Google Earth VR Menu (or press TAB/F4) for the MULTIPLAYER Options Tab.");
}

bool WasKeyPressed(int vk) {
    bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool pressed = down && !g_key_down[vk];
    g_key_down[vk] = down;
    return pressed;
}

void PollHotkeysAndKeyboard(
    const EarthMath::Vec3d& hmd_pos,
    const EarthMath::Quatd& hmd_rot,
    bool& out_desktop_click) {

    out_desktop_click = false;

    if (WasKeyPressed(VK_TAB) || WasKeyPressed(VK_F4)) {
        g_renderer.ToggleStandaloneMenu(hmd_pos, hmd_rot);
        LogMsg("Toggled Standalone Multiplayer Menu Tab (TAB/F4): %s",
               g_renderer.IsMenuVisible() ? "OPEN" : "CLOSED");
    }
    if (WasKeyPressed(VK_F5)) {
        g_renderer.ToggleShow3DAvatars();
        LogMsg("Toggled 3D VR Avatars (F5): %s", g_renderer.GetShow3DAvatars() ? "ON" : "OFF");
    }
    if (WasKeyPressed(VK_F6)) {
        g_renderer.ToggleShowXRayThroughEarth();
        LogMsg("Toggled X-Ray Through Earth (F6): %s", g_renderer.GetShowXRayThroughEarth() ? "ON" : "OFF");
    }
    if (WasKeyPressed(VK_F7)) {
        g_renderer.ToggleShowFloatingMarkers();
        LogMsg("Toggled Floating Markers/Nametags (F7): %s", g_renderer.GetShowFloatingMarkers() ? "ON" : "OFF");
    }
    if (WasKeyPressed(VK_F8)) {
        g_network.CycleTestBotMode();
        LogMsg("Cycled Test Friend Bot Mode (F8): %d", g_network.GetTestBotMode());
    }
    if (WasKeyPressed(VK_F9)) {
        g_voice.ToggleMicMuted();
        LogMsg("Toggled Microphone Mute (F9): %s", g_voice.IsMicMuted() ? "MUTED" : "LIVE");
    }

    if (g_renderer.IsMenuVisible()) {
        if (WasKeyPressed(VK_RETURN) || WasKeyPressed(VK_SPACE) || WasKeyPressed(VK_LBUTTON)) {
            out_desktop_click = true;
        }
        if (WasKeyPressed(VK_BACK)) {
            g_renderer.OnBackspaceInput();
        }
        for (int k = '0'; k <= '9'; ++k) {
            if (WasKeyPressed(k)) g_renderer.OnCharInput(static_cast<char>(k));
        }
        for (int k = 'A'; k <= 'Z'; ++k) {
            if (WasKeyPressed(k)) g_renderer.OnCharInput(static_cast<char>(k));
        }
    }
}

bool ExtractEngineFrameState(
    void* compositor,
    EarthMath::Sim3d& out_sim,
    EarthMath::Vec3d& out_hmd_pos,
    EarthMath::Quatd& out_hmd_rot,
    EarthMath::Vec3d out_ctrl_pos[2],
    EarthMath::Quatd out_ctrl_rot[2],
    bool out_ctrl_active[2],
    EarthRender::EyeRenderParams out_eyes[2],
    EarthRender::VRMenuInputState& out_menu_input) {

    __try {
        if (!compositor) return false;
        uintptr_t comp_addr = reinterpret_cast<uintptr_t>(compositor);

        // 1. Read GeoWidgetImpl* at [compositor + 0x10] and TransformManager* at [geo_widget + 0x60]
        uintptr_t geo_widget = *reinterpret_cast<uintptr_t*>(comp_addr + 0x10);
        if (!geo_widget) return false;

        uintptr_t transform_mgr = *reinterpret_cast<uintptr_t*>(geo_widget + 0x60);
        if (!transform_mgr) return false;

        // Read Sim3d start_space_from_keyhole_space_ at [transform_mgr + 0x98]
        out_sim = *reinterpret_cast<const EarthMath::Sim3d*>(transform_mgr + 0x98);
        if (!out_sim.IsValid()) return false;

        // 2. Read Foreground Camera/SceneNode at [compositor + 0xf8]
        uintptr_t fg_cam = *reinterpret_cast<uintptr_t*>(comp_addr + 0xf8);
        if (!fg_cam) return false;

        // Read start_space_from_head (row-major float[16]) at [fg_cam + 0x17c]
        const float* start_from_head = reinterpret_cast<const float*>(fg_cam + 0x17c);
        out_hmd_pos = {
            static_cast<double>(start_from_head[3]),
            static_cast<double>(start_from_head[7]),
            static_cast<double>(start_from_head[11])
        };
        out_hmd_rot = EarthMath::Quatd::FromRowMajorMat4f(start_from_head);
        out_menu_input.hmd_pos = out_hmd_pos;
        out_menu_input.hmd_rot = out_hmd_rot;

        // Read stereo eye viewports from TransformManager's eye_viewports_ vector at [transform_mgr + 0x18]
        uintptr_t vp_vec_begin = *reinterpret_cast<uintptr_t*>(transform_mgr + 0x18);
        uintptr_t vp_vec_end   = *reinterpret_cast<uintptr_t*>(transform_mgr + 0x20);
        bool has_tm_viewports = (vp_vec_begin != 0 && vp_vec_end >= vp_vec_begin + 32);

        const int32_t* target_dims = reinterpret_cast<const int32_t*>(fg_cam + 0x5d8);

        for (int eye = 0; eye < 2; ++eye) {
            if (has_tm_viewports) {
                const int32_t* vp = reinterpret_cast<const int32_t*>(vp_vec_begin + eye * 16);
                std::memcpy(out_eyes[eye].viewport, vp, 4 * sizeof(int32_t));
            } else if (target_dims[0] > 0 && target_dims[1] > 0) {
                int32_t half_w = target_dims[0] / 2;
                out_eyes[eye].viewport[0] = (eye == 0) ? 0 : half_w;
                out_eyes[eye].viewport[1] = 0;
                out_eyes[eye].viewport[2] = (eye == 0) ? half_w : target_dims[0];
                out_eyes[eye].viewport[3] = target_dims[1];
            }

            const float* eye_from_start = reinterpret_cast<const float*>(fg_cam + 0x2bc + eye * 64);
            std::memcpy(out_eyes[eye].eye_from_start_row_major, eye_from_start, 16 * sizeof(float));

            const float* start_from_eye = reinterpret_cast<const float*>(fg_cam + 0x33c + eye * 64);
            std::memcpy(out_eyes[eye].start_from_eye_row_major, start_from_eye, 16 * sizeof(float));

            const float* clip_from_eye = reinterpret_cast<const float*>(fg_cam + 0x0fc + eye * 64);
            std::memcpy(out_eyes[eye].clip_from_eye_row_major, clip_from_eye, 16 * sizeof(float));
        }

        // 3. Read Left & Right 6DoF controller poses & trigger button states from [compositor + 0x28]
        out_ctrl_active[0] = false;
        out_ctrl_active[1] = false;
        out_menu_input.ctrl_active[0] = false;
        out_menu_input.ctrl_active[1] = false;
        out_menu_input.ctrl_trigger_down[0] = false;
        out_menu_input.ctrl_trigger_down[1] = false;

        uintptr_t input_state = *reinterpret_cast<uintptr_t*>(comp_addr + 0x28);
        if (input_state) {
            for (int c = 0; c < 2; ++c) {
                uintptr_t ctrl_base = input_state + (c == 0 ? 0x58 : 0xa8);
                const float* q = reinterpret_cast<const float*>(ctrl_base + 0x10);
                const float* p = reinterpret_cast<const float*>(ctrl_base + 0x20);
                double n2 = static_cast<double>(q[0]) * q[0] +
                            static_cast<double>(q[1]) * q[1] +
                            static_cast<double>(q[2]) * q[2] +
                            static_cast<double>(q[3]) * q[3];
                if (std::isfinite(n2) && n2 > 0.5 && n2 < 1.5 &&
                    std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2])) {
                    out_ctrl_active[c] = true;
                    out_ctrl_rot[c] = EarthMath::Quatd(q[0], q[1], q[2], q[3]).Normalized();
                    out_ctrl_pos[c] = EarthMath::Vec3d(p[0], p[1], p[2]);

                    out_menu_input.ctrl_active[c] = true;
                    out_menu_input.ctrl_rot[c] = out_ctrl_rot[c];
                    out_menu_input.ctrl_pos[c] = out_ctrl_pos[c];
                }

                // Read button/axis vector at [ctrl_base + 0x38 .. ctrl_base + 0x40] (stride 0x1c = 28 bytes)
                uintptr_t btn_begin = *reinterpret_cast<uintptr_t*>(ctrl_base + 0x38);
                uintptr_t btn_end   = *reinterpret_cast<uintptr_t*>(ctrl_base + 0x40);
                if (btn_begin != 0 && btn_end > btn_begin && (btn_end - btn_begin) <= 28 * 32) {
                    for (uintptr_t cur = btn_begin; cur + 28 <= btn_end; cur += 28) {
                        int32_t btn_id  = *reinterpret_cast<const int32_t*>(cur + 0x00);
                        uint8_t pressed = *reinterpret_cast<const uint8_t*>(cur + 0x04);
                        float axis_val  = *reinterpret_cast<const float*>(cur + 0x08);
                        if ((btn_id == 1 && (pressed != 0 || axis_val > 0.55f)) ||
                            (btn_id == 8 && axis_val > 0.55f)) {
                            out_menu_input.ctrl_trigger_down[c] = true;
                        }
                    }
                }
                if (EarthOpenVREmu::g_emulation_active) {
                    if ((EarthOpenVREmu::g_emu_state.ctrl_buttons_pressed[c] & (1ULL << 33)) != 0 ||
                        EarthOpenVREmu::g_emu_state.ctrl_axes[c][1].x > 0.55f) {
                        out_menu_input.ctrl_trigger_down[c] = true;
                    }
                }
            }
        }

        // 4. Read in-game Google Earth VR Menu state from MenuManager* at [compositor + 0x110]
        out_menu_input.native_menu_open = false;
        out_menu_input.native_menu_visibility = 0.0f;
        uintptr_t menu_mgr = *reinterpret_cast<uintptr_t*>(comp_addr + 0x110);
        if (menu_mgr) {
            float target_vis = *reinterpret_cast<const float*>(menu_mgr + 0x1c);
            float anim_vis   = *reinterpret_cast<const float*>(menu_mgr + 0x20);
            if (std::isfinite(target_vis) && std::isfinite(anim_vis)) {
                out_menu_input.native_menu_open = (target_vis > 0.5f || anim_vis > 0.15f);
                out_menu_input.native_menu_visibility = anim_vis;
            }
            if (out_menu_input.native_menu_open) {
                const float* mp = reinterpret_cast<const float*>(menu_mgr + 0x38);
                const float* mn = reinterpret_cast<const float*>(menu_mgr + 0x50);
                if (std::isfinite(mp[0]) && std::isfinite(mp[1]) && std::isfinite(mp[2])) {
                    out_menu_input.native_menu_pos = EarthMath::Vec3d(mp[0], mp[1], mp[2]);
                }
                if (std::isfinite(mn[0]) && std::isfinite(mn[1]) && std::isfinite(mn[2])) {
                    out_menu_input.native_menu_normal = EarthMath::Vec3d(mn[0], mn[1], mn[2]);
                }
            }
        }

        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall Detour_RenderForeground(void* compositor, void* device, const float* head_pose, void* controller_states) {
    // 1. Run Earth.exe's original RenderForeground so local controllers, UI, and camera matrices are updated
    g_orig_render_foreground(compositor, device, head_pose, controller_states);

    // 2. Lazy-initialize networking & audio on first rendered frame (outside loader lock)
    EnsureSubsystemsInitializedOnFirstFrame();

    // 3. Extract current Earth Sim3d transform, local HMD/controller poses, stereo eye matrices, and menu state
    EarthMath::Sim3d start_from_keyhole;
    EarthMath::Vec3d local_hmd_pos;
    EarthMath::Quatd local_hmd_rot;
    EarthMath::Vec3d local_ctrl_pos[2];
    EarthMath::Quatd local_ctrl_rot[2];
    bool local_ctrl_active[2] = {false, false};
    EarthRender::EyeRenderParams eyes[2];
    EarthRender::VRMenuInputState menu_input;

    if (!ExtractEngineFrameState(compositor, start_from_keyhole, local_hmd_pos, local_hmd_rot,
                                 local_ctrl_pos, local_ctrl_rot, local_ctrl_active, eyes, menu_input)) {
        static int fail_log = 0;
        if (fail_log < 5) {
            LogMsg("ExtractEngineFrameState returned false (call %d)", ++fail_log);
        }
        return;
    }

    static int ok_log = 0;
    if (EarthOpenVREmu::g_emulation_active && start_from_keyhole.scale > 0.001) {
        // Pulse Right Controller Trigger (button 33) every 45 frames to dismiss SplashAppMode ("Press any button to start")
        int phase = ok_log % 45;
        if (ok_log >= 45 && phase < 6) {
            EarthOpenVREmu::g_emu_state.ctrl_buttons_pressed[1] |= (1ULL << 33);
            EarthOpenVREmu::g_emu_state.ctrl_axes[1][1].x = 1.0f;
        } else if (ok_log >= 45 && phase == 6) {
            EarthOpenVREmu::g_emu_state.ctrl_buttons_pressed[1] &= ~(1ULL << 33);
            EarthOpenVREmu::g_emu_state.ctrl_axes[1][1].x = 0.0f;
        }
    }
    if (ok_log % 30 == 0 && ok_log <= 900) {
        GLint draw_fbo = 0, samples = 0;
        glGetIntegerv(0x8CA6 /*GL_DRAW_FRAMEBUFFER_BINDING*/, &draw_fbo);
        glGetIntegerv(0x80A9 /*GL_SAMPLES*/, &samples);
        LogMsg("Live Earth.exe Frame #%d: Sim3d.scale=%.6g, HMD=(%.2f,%.2f,%.2f), Ctrl0=%d(%.2f,%.2f,%.2f), Ctrl1=%d(%.2f,%.2f,%.2f), VP0=(%d,%d,%d,%d), NativeMenu=%d(%.2f), FBO=%d(samples=%d), SubTex=(%u,%u)",
               ok_log, start_from_keyhole.scale,
               local_hmd_pos.x, local_hmd_pos.y, local_hmd_pos.z,
               local_ctrl_active[0] ? 1 : 0, local_ctrl_pos[0].x, local_ctrl_pos[0].y, local_ctrl_pos[0].z,
               local_ctrl_active[1] ? 1 : 0, local_ctrl_pos[1].x, local_ctrl_pos[1].y, local_ctrl_pos[1].z,
               eyes[0].viewport[0], eyes[0].viewport[1], eyes[0].viewport[2], eyes[0].viewport[3],
               menu_input.native_menu_open ? 1 : 0, menu_input.native_menu_visibility,
               draw_fbo, samples,
               EarthOpenVREmu::g_emu_state.last_submitted_gl_tex[0],
               EarthOpenVREmu::g_emu_state.last_submitted_gl_tex[1]);
    }
    ++ok_log;

    // 4. Poll hotkeys & keyboard input (TAB/F4 menu toggle, F5..F9, password typing)
    PollHotkeysAndKeyboard(local_hmd_pos, local_hmd_rot, menu_input.desktop_click_pressed);

    // 5. Update 3D VR Multiplayer Options Tab raycast & button clicks
    g_renderer.UpdateMenuInteraction(menu_input, g_network, g_voice);

    // 6. Broadcast local Keyhole-Space pose & poll incoming authenticated Steam P2P / UDP packets
    g_network.UpdateLocalPoseAndPoll(
        start_from_keyhole,
        local_hmd_pos,
        local_hmd_rot,
        local_ctrl_pos,
        local_ctrl_rot,
        local_ctrl_active
    );

    // 7. Render all active remote players AND the 3D VR Multiplayer Options Tab into Left & Right Eye FBOs
    auto remote_players = g_network.GetActiveRemotePlayers();
    if (!remote_players.empty() || g_renderer.IsMenuVisible()) {
        g_renderer.RenderFrame(start_from_keyhole, local_hmd_pos, eyes, remote_players, g_network, g_voice);
    }

}

} // namespace

bool InstallHookInDllMain(const std::string& dll_dir) {
    static std::atomic<bool> s_hook_installed{false};
    if (s_hook_installed.exchange(true)) return true;

    g_dll_dir = dll_dir;

    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) return false;

    EarthOpenVREmu::InstallOpenVRIATHooks(base, &LogMsg);

    uint8_t* target = reinterpret_cast<uint8_t*>(base + kRva_CompositorRenderForeground);

    // Verify exact prologue before patching
    DWORD old_prot = 0;
    if (!VirtualProtect(target, 16, PAGE_EXECUTE_READ, &old_prot)) {
        return false;
    }
    bool match = (std::memcmp(target, kExpectedPrologue, sizeof(kExpectedPrologue)) == 0);
    VirtualProtect(target, 16, old_prot, &old_prot);
    if (!match) return false;

    return g_fg_hook.Install(target, reinterpret_cast<void*>(&Detour_RenderForeground), 16,
                             reinterpret_cast<void**>(&g_orig_render_foreground));
}

void ShutdownMod(bool is_process_terminating) {
    if (!is_process_terminating) {
        g_fg_hook.Remove();
        g_network.Shutdown();
        g_voice.Shutdown();
        g_renderer.Shutdown();
    }
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file) {
        std::fprintf(g_log_file, "=== Google Earth VR Multiplayer Mod Unloaded ===\n");
        std::fflush(g_log_file);
        std::fclose(g_log_file);
        g_log_file = nullptr;
    }
}

} // namespace EarthHook
