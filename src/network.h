#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include "earth_math.h"
#include "voice.h"
#include "crypto_safety.h"

namespace EarthNet {

constexpr uint32_t kPacketMagic = 0x32504D45; // 'EMP2' (Authenticated Protocol v2)
constexpr uint16_t kProtocolVersion = 2;
constexpr uint16_t kDefaultUdpPort = 43825;
constexpr uint32_t kEarthVrAppId = 348250;
constexpr size_t   kMaxSessionPeers = 16;

enum PacketType : uint8_t {
    kPacketPose          = 1,
    kPacketVoice         = 2,
    kPacketJoinRequest   = 3,
    kPacketJoinAccept    = 4,
    kPacketJoinReject    = 5,
    kPacketKick          = 6,
    kPacketSessionClosed = 7,
    kPacketPeerList      = 8,
    kPacketLanBeacon     = 9,
};

enum RejectReason : uint8_t {
    kRejectNone          = 0,
    kRejectWrongPassword = 1,
    kRejectSessionLocked = 2,
    kRejectBanned        = 3,
    kRejectSessionFull   = 4,
};

enum class SessionState : int {
    Offline   = 0,
    Hosting   = 1,
    Joining   = 2,
    InSession = 3,
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic = kPacketMagic;
    uint16_t version = kProtocolVersion;
    uint8_t  type = 0;
    uint8_t  flags = 0;
    uint64_t sender_id = 0;
    uint64_t session_token = 0;
    uint32_t seq = 0;
};

struct PosePacket {
    PacketHeader hdr;
    char player_name[32] = {};
    double viewer_scale = 1.0;
    EarthMath::Vec3d hmd_pos_kh;
    EarthMath::Quatd hmd_rot_kh;
    EarthMath::Vec3d ctrl_pos_kh[2];
    EarthMath::Quatd ctrl_rot_kh[2];
    uint8_t ctrl_active[2] = {0, 0};
    float voice_level = 0.0f;
    float color_rgb[3] = {0.2f, 0.85f, 1.0f};
};

struct VoicePacket {
    PacketHeader hdr;
    uint8_t codec = 0; // 0 = IMA-ADPCM 16kHz (164 bytes)
    uint16_t payload_len = 0;
    uint8_t payload[256] = {};
};

struct JoinRequestPacket {
    PacketHeader hdr;
    uint64_t lobby_id = 0;
    char player_name[32] = {};
    uint8_t auth_hash[32] = {}; // Salted SHA-256 proof
};

struct JoinAcceptPacket {
    PacketHeader hdr;
    uint64_t assigned_session_token = 0;
    char host_name[32] = {};
};

struct JoinRejectPacket {
    PacketHeader hdr;
    uint8_t reason = 0;
};

struct KickPacket {
    PacketHeader hdr;
    uint64_t kicked_peer_id = 0;
    uint8_t is_ban = 0; // 0 = Kicked from current session, 1 = Banned by host
};

struct PeerListPacket {
    PacketHeader hdr;
    uint8_t locked = 0;
    uint8_t peer_count = 0;
    uint64_t peer_ids[kMaxSessionPeers] = {};
};

struct LanBeaconPacket {
    PacketHeader hdr;
    uint64_t lobby_id = 0;
    char host_name[32] = {};
    uint8_t member_count = 1;
    uint8_t max_members = 16;
    uint8_t has_password = 0;
    uint8_t is_locked = 0;
    char pw_salt[36] = {};
};
#pragma pack(pop)

struct PublicSessionInfo {
    uint64_t lobby_id = 0;
    uint64_t host_id = 0;
    std::string host_name;
    int member_count = 1;
    int max_members = 16;
    bool has_password = false;
    bool is_locked = false;
    std::string pw_salt;
    bool is_lan_session = false;
    sockaddr_in lan_addr = {};
    uint64_t last_seen_ms = 0;
};

struct PlayerPoseSnapshot {
    double viewer_scale = 1.0;
    EarthMath::Vec3d hmd_pos_kh;
    EarthMath::Quatd hmd_rot_kh;
    EarthMath::Vec3d ctrl_pos_kh[2];
    EarthMath::Quatd ctrl_rot_kh[2];
    bool ctrl_active[2] = {false, false};
    uint64_t timestamp_ms = 0;
};

struct RemotePlayerState {
    uint64_t peer_id = 0;
    std::string name;
    float color_rgb[3] = {0.2f, 0.85f, 1.0f};
    float voice_level = 0.0f;
    uint32_t last_seq = 0;
    uint64_t last_seen_ms = 0;
    bool is_simulated_bot = false;

    PlayerPoseSnapshot prev_snap;
    PlayerPoseSnapshot target_snap;
    PlayerPoseSnapshot interp_snap;
};

struct BannedPlayerInfo {
    uint64_t peer_id = 0;
    std::string name;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool Initialize(EarthVoice::GlobalVoiceManager* voice_mgr, const std::string& dll_dir, bool enable_local_udp = false);
    void Shutdown();

    // === Session & Server Controls (Triggered by In-Game Multiplayer Menu) ===
    bool StartServer(const std::string& password = "");
    void CloseServerOrLeave();
    void SetSessionLocked(bool locked);
    void ToggleSessionLocked() { SetSessionLocked(!session_locked_.load()); }
    bool IsSessionLocked() const { return session_locked_.load(); }
    bool HasServerPassword() const { return has_password_.load(); }

    // Kick removes the player from the current session only (does NOT ban them from rejoining or future sessions)
    void KickPlayer(uint64_t target_peer_id);

    // Ban & Unban are separate from Kick and persist across sessions
    void BanPlayer(uint64_t target_peer_id, const std::string& player_name = "");
    void UnbanPlayer(uint64_t target_peer_id);
    bool IsPlayerBanned(uint64_t target_peer_id);
    std::vector<BannedPlayerInfo> GetBannedPlayers();

    void RefreshPublicSessions();
    std::vector<PublicSessionInfo> GetPublicSessions();
    bool JoinPublicSession(uint64_t lobby_id, const std::string& password = "");

    SessionState GetSessionState() const { return session_state_.load(); }
    bool IsHost() const { return session_state_.load() == SessionState::Hosting; }
    std::string GetStatusMessage();

    // Called every frame from render hook with local player's Start-Space pose
    void UpdateLocalPoseAndPoll(
        const EarthMath::Sim3d& start_from_keyhole,
        const EarthMath::Vec3d& local_hmd_start_pos,
        const EarthMath::Quatd& local_hmd_start_rot,
        const EarthMath::Vec3d local_ctrl_start_pos[2],
        const EarthMath::Quatd local_ctrl_start_rot[2],
        const bool local_ctrl_active[2]
    );

    std::vector<RemotePlayerState> GetActiveRemotePlayers();

    // Simulated friend / test bot mode (0 = Off, 1 = Opposite-Side Giant, 2 = Nearby Friend, 3 = Human in Paris)
    int GetTestBotMode() const { return test_bot_mode_.load(); }
    void SetTestBotMode(int mode) { test_bot_mode_.store(mode % 4); }
    void CycleTestBotMode() { test_bot_mode_.store((test_bot_mode_.load() + 1) % 4); }

    bool IsSteamConnected() const { return steam_initialized_.load(); }
    uint64_t GetLocalPeerId() const { return local_peer_id_; }
    const std::string& GetLocalPlayerName() const { return local_player_name_; }
    size_t GetConnectedPeerCount();

    // Direct packet injection helper for unit testing
    void InjectTestPacket(uint64_t authenticated_sender_id, const void* data, size_t len);
    uint64_t GetCurrentSessionToken() const { return session_token_.load(); }
    uint64_t GetCurrentLobbyId() const { return current_lobby_id_.load(); }
    std::string GetCurrentPasswordSalt();

private:
    void LoadConfig(const std::string& ini_path);
    bool InitSteamworks(const std::string& dll_dir);
    bool InitUdpSocket();

    void PollSteamCallbacksAndPackets();
    void PollUdpPackets();
    void BroadcastPoseAndPeerList(const PosePacket& pose_pkt);
    void BroadcastVoicePacket(const uint8_t* adpcm_data, size_t len);
    void SendDirectPacket(uint64_t target_peer_id, const void* data, size_t len, bool reliable, int channel = 0);

    void HandleAuthenticatedPacket(
        uint64_t authenticated_sender_id,
        const uint8_t* data,
        size_t len,
        const sockaddr_in* udp_from = nullptr
    );

    void SetStatusMessage(const std::string& msg);
    void UpdateSimulatedBot(
        const EarthMath::Vec3d& local_hmd_kh_pos,
        const EarthMath::Quatd& local_hmd_kh_rot,
        double local_viewer_scale
    );

    EarthVoice::GlobalVoiceManager* voice_mgr_ = nullptr;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> steam_initialized_{false};
    std::atomic<SessionState> session_state_{SessionState::Offline};
    std::atomic<bool> session_locked_{false};
    std::atomic<bool> has_password_{false};
    std::atomic<int> test_bot_mode_{0};

    uint64_t local_peer_id_ = 0;
    std::string local_player_name_ = "Explorer";
    float local_color_rgb_[3] = {0.15f, 0.85f, 1.0f};
    uint32_t pose_seq_ = 0;
    uint32_t voice_seq_ = 0;

    uint64_t last_pose_send_ms_ = 0;
    uint64_t last_peer_list_ms_ = 0;
    uint64_t join_start_ms_ = 0;

    // Session security state
    std::atomic<uint64_t> current_lobby_id_{0};
    std::atomic<uint64_t> session_host_id_{0};
    std::atomic<uint64_t> session_token_{0};
    std::string server_password_;
    std::string password_salt_;
    std::string pending_join_password_;
    std::string status_message_ = "OFFLINE - OPEN MULTIPLAYER MENU TO HOST OR JOIN";
    std::mutex status_mutex_;

    std::unordered_set<uint64_t> authorized_peers_;
    std::unordered_map<uint64_t, std::string> banned_peers_; // Persistent ban list (peer_id -> display name)
    EarthSecurity::PeerRateLimiter pose_rate_limiter_;
    EarthSecurity::PeerRateLimiter voice_rate_limiter_;
    EarthSecurity::PeerRateLimiter ctrl_rate_limiter_;
    std::mutex session_mutex_;

    // Discovered public sessions
    std::vector<PublicSessionInfo> public_sessions_;
    std::mutex browser_mutex_;

    // Optional local/LAN UDP fallback (only enabled if configured or testing)
    bool udp_enabled_ = false;
    SOCKET udp_sock_ = INVALID_SOCKET;
    uint16_t bound_udp_port_ = 0;
    std::unordered_map<uint64_t, sockaddr_in> udp_peer_addrs_;
    std::vector<sockaddr_in> udp_broadcast_targets_;
    std::mutex udp_mutex_;

    // Steamworks dynamic function pointers
    HMODULE steam_dll_ = nullptr;
    void* steam_user_ = nullptr;
    void* steam_friends_ = nullptr;
    void* steam_matchmaking_ = nullptr;
    void* steam_networking_ = nullptr;
    int32_t steam_pipe_ = 0;

    // Active remote players
    std::mutex players_mutex_;
    std::unordered_map<uint64_t, RemotePlayerState> remote_players_;
    RemotePlayerState simulated_bot_;
};

} // namespace EarthNet
