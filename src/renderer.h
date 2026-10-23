#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include "earth_math.h"
#include "network.h"
#include "voice.h"

namespace EarthRender {

struct EyeRenderParams {
    int32_t viewport[4] = {0, 0, 0, 0}; // x0, y0, x1, y1
    float eye_from_start_row_major[16] = {};
    float start_from_eye_row_major[16] = {};
    float clip_from_eye_row_major[16] = {};
};

struct VRMenuInputState {
    // From Earth.exe's MenuManager [compositor + 0x110]
    bool native_menu_open = false;
    float native_menu_visibility = 0.0f;
    EarthMath::Vec3d native_menu_pos{0.0, 0.0, 0.0};
    EarthMath::Vec3d native_menu_normal{0.0, 0.0, 1.0};

    // Local HMD pose in Start Space
    EarthMath::Vec3d hmd_pos{0.0, 0.0, 0.0};
    EarthMath::Quatd hmd_rot{0.0, 0.0, 0.0, 1.0};

    // Local 6DoF controllers in Start Space + Trigger state
    bool ctrl_active[2] = {false, false};
    EarthMath::Vec3d ctrl_pos[2] = {};
    EarthMath::Quatd ctrl_rot[2] = {};
    bool ctrl_trigger_down[2] = {false, false};

    // Keyboard / desktop fallback trigger
    bool desktop_click_pressed = false;
};

class StereoAvatarRenderer {
public:
    StereoAvatarRenderer();
    ~StereoAvatarRenderer();

    bool EnsureInitialized();
    void Shutdown();

    // Updates interactive 3D VR Multiplayer menu state, raycasting, and button actions
    void UpdateMenuInteraction(
        const VRMenuInputState& input,
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice
    );

    // Toggle standalone multiplayer menu even when native Google Earth VR menu is closed
    void ToggleStandaloneMenu(const EarthMath::Vec3d& hmd_pos, const EarthMath::Quatd& hmd_rot);
    bool IsMenuVisible() const { return menu_visible_; }
    EarthMath::Vec3d GetPanelPointWorld(float lx, float ly) const {
        EarthMath::Vec3d r(panel_right_.x, panel_right_.y, panel_right_.z);
        EarthMath::Vec3d u(panel_up_.x, panel_up_.y, panel_up_.z);
        return panel_center_ + r * static_cast<double>(lx) + u * static_cast<double>(ly);
    }

    // Forward keyboard characters when password modal is open
    void OnCharInput(char c);
    void OnBackspaceInput();

    // Renders all remote players (3D HMD/controllers, X-ray pass, floating nametag/marker)
    // AND the 3D VR Multiplayer Options Tab & Laser Pointer for both eyes
    void RenderFrame(
        const EarthMath::Sim3d& start_from_keyhole,
        const EarthMath::Vec3d& local_hmd_start_pos,
        const EyeRenderParams eyes[2],
        const std::vector<EarthNet::RemotePlayerState>& remote_players,
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice
    );

    // Visual feature toggles (all 3 enabled by default, individually toggleable)
    bool GetShow3DAvatars() const { return show_3d_avatars_.load(); }
    void SetShow3DAvatars(bool v) { show_3d_avatars_.store(v); }
    void ToggleShow3DAvatars() { show_3d_avatars_.store(!show_3d_avatars_.load()); }

    bool GetShowXRayThroughEarth() const { return show_xray_through_earth_.load(); }
    void SetShowXRayThroughEarth(bool v) { show_xray_through_earth_.store(v); }
    void ToggleShowXRayThroughEarth() { show_xray_through_earth_.store(!show_xray_through_earth_.load()); }

    bool GetShowFloatingMarkers() const { return show_floating_markers_.load(); }
    void SetShowFloatingMarkers(bool v) { show_floating_markers_.store(v); }
    void ToggleShowFloatingMarkers() { show_floating_markers_.store(!show_floating_markers_.load()); }

    void LoadConfig(const std::string& ini_path);

private:
    struct Vertex3D {
        float px, py, pz;
        float nx, ny, nz;
        float r, g, b, a;
    };

    struct Vertex2D {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    struct UIButton {
        int id = 0;
        float cx = 0.0f;      // Local X on panel in meters
        float cy = 0.0f;      // Local Y on panel in meters
        float half_w = 0.0f;  // Half-width in meters
        float half_h = 0.0f;  // Half-height in meters
        std::string label;
        float bg_r = 0.12f, bg_g = 0.16f, bg_b = 0.24f, bg_a = 0.92f;
        float fg_r = 1.0f,  fg_g = 1.0f,  fg_b = 1.0f;
        uint64_t target_id = 0;
        bool requires_pw = false;
    };

    enum class PasswordModalMode : uint8_t {
        None = 0,
        HostWithPassword = 1,
        JoinWithPassword = 2
    };

    bool BuildShaders();
    void BuildMeshes();
    void BuildFontTexture();

    void AnchorMenuInFrontOfHMD(const EarthMath::Vec3d& hmd_pos, const EarthMath::Quatd& hmd_rot);
    void BuildMenuButtons(
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice,
        const std::vector<EarthNet::RemotePlayerState>& remote_players
    );
    void ExecuteButtonAction(
        const UIButton& btn,
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice,
        const EarthMath::Vec3d& hmd_pos,
        const EarthMath::Quatd& hmd_rot
    );

    void RenderEyePass(
        int eye_idx,
        const EyeRenderParams& eye,
        const EarthMath::Sim3d& start_from_keyhole,
        const EarthMath::Vec3d& local_hmd_start_pos,
        const std::vector<EarthNet::RemotePlayerState>& remote_players,
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice
    );

    void RenderMultiplayerMenuForEye(
        std::vector<Vertex2D>& ui_verts,
        const EarthMath::Vec3d& eye_pos_start,
        EarthNet::NetworkManager& network,
        EarthVoice::GlobalVoiceManager& voice,
        const std::vector<EarthNet::RemotePlayerState>& remote_players
    );

    void AppendBillboardQuad(
        std::vector<Vertex2D>& verts,
        const EarthMath::Vec3f& center,
        const EarthMath::Vec3f& right,
        const EarthMath::Vec3f& up,
        float half_w, float half_h,
        float u0, float v0, float u1, float v1,
        float r, float g, float b, float a
    );

    void AppendBillboardText(
        std::vector<Vertex2D>& verts,
        const std::string& text,
        const EarthMath::Vec3f& origin,
        const EarthMath::Vec3f& right,
        const EarthMath::Vec3f& up,
        float char_height,
        float r, float g, float b, float a
    );

    bool gl_ready_ = false;
    GLuint avatar_prog_ = 0;
    GLint u_avatar_mvp_ = -1;
    GLint u_avatar_model_ = -1;
    GLint u_avatar_tint_ = -1;
    GLint u_avatar_xray_ = -1;
    GLint u_avatar_voice_ = -1;

    GLuint marker_prog_ = 0;
    GLint u_marker_mvp_ = -1;
    GLint u_marker_tex_ = -1;

    GLuint hmd_vao_ = 0, hmd_vbo_ = 0;
    GLsizei hmd_vertex_count_ = 0;

    GLuint ctrl_vao_ = 0, ctrl_vbo_ = 0;
    GLsizei ctrl_vertex_count_ = 0;

    GLuint halo_vao_ = 0, halo_vbo_ = 0;
    GLsizei halo_vertex_count_ = 0;

    GLuint marker_vao_ = 0, marker_vbo_ = 0;
    GLuint font_tex_ = 0;

    std::atomic<bool> show_3d_avatars_{true};
    std::atomic<bool> show_xray_through_earth_{true};
    std::atomic<bool> show_floating_markers_{true};

    // 3D VR Multiplayer Menu Tab state
    bool menu_visible_ = false;
    bool standalone_menu_pinned_ = false;
    bool prev_native_menu_open_ = false;
    bool tab_expanded_ = true;

    EarthMath::Vec3d panel_center_{0.0, 1.4, -1.0};
    EarthMath::Vec3f panel_right_{1.0f, 0.0f, 0.0f};
    EarthMath::Vec3f panel_up_{0.0f, 1.0f, 0.0f};
    EarthMath::Vec3f panel_normal_{0.0f, 0.0f, 1.0f};

    bool has_laser_hit_ = false;
    int laser_ctrl_idx_ = -1;
    float laser_local_x_ = 0.0f;
    float laser_local_y_ = 0.0f;
    EarthMath::Vec3d laser_start_pos_{0.0, 0.0, 0.0};
    EarthMath::Vec3d laser_hit_pos_{0.0, 0.0, 0.0};
    int hovered_button_id_ = -1;
    bool prev_trigger_down_[2] = {false, false};

    PasswordModalMode pw_modal_mode_ = PasswordModalMode::None;
    bool show_ban_list_ = false;
    uint64_t pending_join_lobby_id_ = 0;
    std::string pw_input_buffer_;

    std::vector<UIButton> active_buttons_;
};

} // namespace EarthRender
