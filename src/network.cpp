#include "network.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

namespace EarthNet {

namespace {

using fn_SteamAPI_InitSafe = bool (*)();
using fn_SteamAPI_Shutdown = void (*)();
using fn_SteamAPI_GetHSteamPipe = int32_t (*)();
using fn_SteamAPI_ManualDispatch_Init = void (*)();
using fn_SteamAPI_ManualDispatch_RunFrame = void (*)(int32_t);
using fn_SteamAPI_ManualDispatch_GetNextCallback = bool (*)(int32_t, void*);
using fn_SteamAPI_ManualDispatch_FreeLastCallback = void (*)(int32_t);
using fn_SteamAPI_ManualDispatch_GetAPICallResult = bool (*)(int32_t, uint64_t, void*, int, int, bool*);

using fn_SteamAPI_SteamUser_v023 = void* (*)();
using fn_SteamAPI_ISteamUser_GetSteamID = uint64_t (*)(void*);

using fn_SteamAPI_SteamFriends_v017 = void* (*)();
using fn_SteamAPI_ISteamFriends_GetPersonaName = const char* (*)(void*);

using fn_SteamAPI_SteamMatchmaking_v009 = void* (*)();
using fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter = void (*)(void*, const char*, const char*, int);
using fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter = void (*)(void*, int);
using fn_SteamAPI_ISteamMatchmaking_RequestLobbyList = uint64_t (*)(void*);
using fn_SteamAPI_ISteamMatchmaking_GetLobbyByIndex = uint64_t (*)(void*, int);
using fn_SteamAPI_ISteamMatchmaking_CreateLobby = uint64_t (*)(void*, int, int);
using fn_SteamAPI_ISteamMatchmaking_JoinLobby = uint64_t (*)(void*, uint64_t);
using fn_SteamAPI_ISteamMatchmaking_LeaveLobby = void (*)(void*, uint64_t);
using fn_SteamAPI_ISteamMatchmaking_SetLobbyData = bool (*)(void*, uint64_t, const char*, const char*);
using fn_SteamAPI_ISteamMatchmaking_GetLobbyData = const char* (*)(void*, uint64_t, const char*);
using fn_SteamAPI_ISteamMatchmaking_SetLobbyJoinable = bool (*)(void*, uint64_t, bool);
using fn_SteamAPI_ISteamMatchmaking_GetLobbyOwner = uint64_t (*)(void*, uint64_t);
using fn_SteamAPI_ISteamMatchmaking_GetNumLobbyMembers = int (*)(void*, uint64_t);
using fn_SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit = int (*)(void*, uint64_t);

using fn_SteamAPI_SteamNetworking_v006 = void* (*)();
using fn_SteamAPI_ISteamNetworking_AllowP2PPacketRelay = bool (*)(void*, bool);
using fn_SteamAPI_ISteamNetworking_SendP2PPacket = bool (*)(void*, uint64_t, const void*, uint32_t, int, int);
using fn_SteamAPI_ISteamNetworking_IsP2PPacketAvailable = bool (*)(void*, uint32_t*, int);
using fn_SteamAPI_ISteamNetworking_ReadP2PPacket = bool (*)(void*, void*, uint32_t, uint32_t*, uint64_t*, int);
using fn_SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser = bool (*)(void*, uint64_t);
using fn_SteamAPI_ISteamNetworking_CloseP2PSessionWithUser = bool (*)(void*, uint64_t);

struct SteamProcs {
    fn_SteamAPI_InitSafe InitSafe = nullptr;
    fn_SteamAPI_Shutdown Shutdown = nullptr;
    fn_SteamAPI_GetHSteamPipe GetHSteamPipe = nullptr;
    fn_SteamAPI_ManualDispatch_Init ManualDispatch_Init = nullptr;
    fn_SteamAPI_ManualDispatch_RunFrame ManualDispatch_RunFrame = nullptr;
    fn_SteamAPI_ManualDispatch_GetNextCallback ManualDispatch_GetNextCallback = nullptr;
    fn_SteamAPI_ManualDispatch_FreeLastCallback ManualDispatch_FreeLastCallback = nullptr;
    fn_SteamAPI_ManualDispatch_GetAPICallResult ManualDispatch_GetAPICallResult = nullptr;

    fn_SteamAPI_SteamUser_v023 SteamUser = nullptr;
    fn_SteamAPI_ISteamUser_GetSteamID GetSteamID = nullptr;

    fn_SteamAPI_SteamFriends_v017 SteamFriends = nullptr;
    fn_SteamAPI_ISteamFriends_GetPersonaName GetPersonaName = nullptr;

    fn_SteamAPI_SteamMatchmaking_v009 SteamMatchmaking = nullptr;
    fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter AddRequestLobbyListStringFilter = nullptr;
    fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter AddRequestLobbyListDistanceFilter = nullptr;
    fn_SteamAPI_ISteamMatchmaking_RequestLobbyList RequestLobbyList = nullptr;
    fn_SteamAPI_ISteamMatchmaking_GetLobbyByIndex GetLobbyByIndex = nullptr;
    fn_SteamAPI_ISteamMatchmaking_CreateLobby CreateLobby = nullptr;
    fn_SteamAPI_ISteamMatchmaking_JoinLobby JoinLobby = nullptr;
    fn_SteamAPI_ISteamMatchmaking_LeaveLobby LeaveLobby = nullptr;
    fn_SteamAPI_ISteamMatchmaking_SetLobbyData SetLobbyData = nullptr;
    fn_SteamAPI_ISteamMatchmaking_GetLobbyData GetLobbyData = nullptr;
    fn_SteamAPI_ISteamMatchmaking_SetLobbyJoinable SetLobbyJoinable = nullptr;
    fn_SteamAPI_ISteamMatchmaking_GetLobbyOwner GetLobbyOwner = nullptr;
    fn_SteamAPI_ISteamMatchmaking_GetNumLobbyMembers GetNumLobbyMembers = nullptr;
    fn_SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit GetLobbyMemberLimit = nullptr;

    fn_SteamAPI_SteamNetworking_v006 SteamNetworking = nullptr;
    fn_SteamAPI_ISteamNetworking_AllowP2PPacketRelay AllowP2PPacketRelay = nullptr;
    fn_SteamAPI_ISteamNetworking_SendP2PPacket SendP2PPacket = nullptr;
    fn_SteamAPI_ISteamNetworking_IsP2PPacketAvailable IsP2PPacketAvailable = nullptr;
    fn_SteamAPI_ISteamNetworking_ReadP2PPacket ReadP2PPacket = nullptr;
    fn_SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser AcceptP2PSessionWithUser = nullptr;
    fn_SteamAPI_ISteamNetworking_CloseP2PSessionWithUser CloseP2PSessionWithUser = nullptr;
} g_steam;

#pragma pack(push, 8)
struct SteamCallbackMsg_t {
    int32_t m_hSteamUser;
    int32_t m_iCallback;
    uint8_t* m_pubParam;
    int32_t m_cubParam;
};

struct SteamAPICallCompleted_t {
    uint64_t m_hAsyncCall;
    int32_t m_iCallback;
    uint32_t m_cubParam;
};

struct LobbyMatchList_t {
    uint32_t m_nLobbiesMatching;
};

struct LobbyCreated_t {
    int32_t m_eResult;
    uint64_t m_ulSteamIDLobby;
};

struct LobbyEnter_t {
    uint64_t m_ulSteamIDLobby;
    uint32_t m_rgfChatPermissions;
    bool m_bLocked;
    uint32_t m_EChatRoomEnterResponse;
};

struct P2PSessionRequest_t {
    uint64_t m_steamIDRemote;
};
#pragma pack(pop)

constexpr int kCallback_LobbyEnter = 504;
constexpr int kCallback_LobbyMatchList = 510;
constexpr int kCallback_LobbyCreated = 513;
constexpr int kCallback_SteamAPICallCompleted = 703;
constexpr int kCallback_P2PSessionRequest = 1202;

void HsvToRgb(float h, float s, float v, float out_rgb[3]) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    int sector = static_cast<int>(h * 6.0f) % 6;
    switch (sector) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    out_rgb[0] = r + m;
    out_rgb[1] = g + m;
    out_rgb[2] = b + m;
}

} // namespace

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    Shutdown();
}

void NetworkManager::SetStatusMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_message_ = msg;
}

std::string NetworkManager::GetStatusMessage() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_message_;
}

std::string NetworkManager::GetCurrentPasswordSalt() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return password_salt_;
}

void NetworkManager::LoadConfig(const std::string& ini_path) {
    char buf[256] = {};
    GetPrivateProfileStringA("Multiplayer", "PlayerName", "", buf, sizeof(buf), ini_path.c_str());
    if (buf[0] != '\0') {
        local_player_name_ = EarthSecurity::SanitizeDisplayString(buf, sizeof(buf), "Explorer");
    } else {
        DWORD sz = sizeof(buf);
        if (GetUserNameA(buf, &sz) && buf[0] != '\0') {
            local_player_name_ = EarthSecurity::SanitizeDisplayString(buf, sizeof(buf), "Explorer");
        }
    }

    int bot_mode = GetPrivateProfileIntA("Multiplayer", "TestBotMode", 0, ini_path.c_str());
    test_bot_mode_.store(std::clamp(bot_mode, 0, 3));

    int lan_udp = GetPrivateProfileIntA("Multiplayer", "EnableLanUdp", 0, ini_path.c_str());
    if (lan_udp != 0) {
        udp_enabled_ = true;
    }
}

bool NetworkManager::Initialize(EarthVoice::GlobalVoiceManager* voice_mgr, const std::string& dll_dir, bool enable_local_udp) {
    if (initialized_.load()) return true;
    voice_mgr_ = voice_mgr;
    udp_enabled_ = enable_local_udp;

    local_peer_id_ = EarthSecurity::GenerateRandomToken64();
    LoadConfig(dll_dir + "\\earthvr_mp.ini");

    InitSteamworks(dll_dir);
    if (udp_enabled_ || !steam_initialized_.load()) {
        InitUdpSocket();
    }

    float hue = std::fmod(static_cast<float>(local_peer_id_ & 0xFFFF) * 0.6180339887f, 1.0f);
    HsvToRgb(hue, 0.75f, 1.0f, local_color_rgb_);

    if (voice_mgr_) {
        voice_mgr_->Initialize([this](const uint8_t* adpcm_data, size_t len) {
            BroadcastVoicePacket(adpcm_data, len);
        });
    }

    // Start in Offline state (user explicitly hosts or joins via in-game Multiplayer Menu)
    session_state_.store(SessionState::Offline);
    SetStatusMessage("OFFLINE - USE MULTIPLAYER TAB TO HOST OR JOIN");

    initialized_.store(true);
    return true;
}

void NetworkManager::Shutdown() {
    if (!initialized_.exchange(false)) return;
    CloseServerOrLeave();
    if (udp_sock_ != INVALID_SOCKET) {
        closesocket(udp_sock_);
        udp_sock_ = INVALID_SOCKET;
    }
    WSACleanup();
}

bool NetworkManager::InitUdpSocket() {
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    udp_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock_ == INVALID_SOCKET) return false;

    u_long non_blocking = 1;
    ioctlsocket(udp_sock_, FIONBIO, &non_blocking);

    // Bind exclusively to loopback (127.0.0.1) unless LAN UDP is explicitly requested,
    // protecting user's external IP/interface from unsolicited external packets.
    for (uint16_t p = kDefaultUdpPort; p < kDefaultUdpPort + 8; ++p) {
        sockaddr_in bind_addr = {};
        bind_addr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
        bind_addr.sin_port = htons(p);
        if (bind(udp_sock_, reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr)) == 0) {
            bound_udp_port_ = p;
            break;
        }
    }

    for (uint16_t p = kDefaultUdpPort; p < kDefaultUdpPort + 4; ++p) {
        if (p == bound_udp_port_) continue;
        sockaddr_in local_target = {};
        local_target.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &local_target.sin_addr);
        local_target.sin_port = htons(p);
        udp_broadcast_targets_.push_back(local_target);
    }
    return true;
}

bool NetworkManager::InitSteamworks(const std::string& dll_dir) {
    SetEnvironmentVariableA("SteamAppId", "348250");
    SetEnvironmentVariableA("SteamGameId", "348250");

    steam_dll_ = GetModuleHandleA("steam_api64.dll");
    if (!steam_dll_) {
        std::string full_path = dll_dir + "\\steam_api64.dll";
        steam_dll_ = LoadLibraryA(full_path.c_str());
    }
    if (!steam_dll_) return false;

    #define LOAD_STEAM_PROC(field, sym) \
        g_steam.field = reinterpret_cast<decltype(g_steam.field)>(GetProcAddress(steam_dll_, sym))

    LOAD_STEAM_PROC(InitSafe, "SteamAPI_InitSafe");
    LOAD_STEAM_PROC(Shutdown, "SteamAPI_Shutdown");
    LOAD_STEAM_PROC(GetHSteamPipe, "SteamAPI_GetHSteamPipe");
    LOAD_STEAM_PROC(ManualDispatch_Init, "SteamAPI_ManualDispatch_Init");
    LOAD_STEAM_PROC(ManualDispatch_RunFrame, "SteamAPI_ManualDispatch_RunFrame");
    LOAD_STEAM_PROC(ManualDispatch_GetNextCallback, "SteamAPI_ManualDispatch_GetNextCallback");
    LOAD_STEAM_PROC(ManualDispatch_FreeLastCallback, "SteamAPI_ManualDispatch_FreeLastCallback");
    LOAD_STEAM_PROC(ManualDispatch_GetAPICallResult, "SteamAPI_ManualDispatch_GetAPICallResult");

    LOAD_STEAM_PROC(SteamUser, "SteamAPI_SteamUser_v023");
    LOAD_STEAM_PROC(GetSteamID, "SteamAPI_ISteamUser_GetSteamID");

    LOAD_STEAM_PROC(SteamFriends, "SteamAPI_SteamFriends_v017");
    LOAD_STEAM_PROC(GetPersonaName, "SteamAPI_ISteamFriends_GetPersonaName");

    LOAD_STEAM_PROC(SteamMatchmaking, "SteamAPI_SteamMatchmaking_v009");
    LOAD_STEAM_PROC(AddRequestLobbyListStringFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    LOAD_STEAM_PROC(AddRequestLobbyListDistanceFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    LOAD_STEAM_PROC(RequestLobbyList, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    LOAD_STEAM_PROC(GetLobbyByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    LOAD_STEAM_PROC(CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
    LOAD_STEAM_PROC(JoinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby");
    LOAD_STEAM_PROC(LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    LOAD_STEAM_PROC(SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    LOAD_STEAM_PROC(GetLobbyData, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    LOAD_STEAM_PROC(SetLobbyJoinable, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    LOAD_STEAM_PROC(GetLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    LOAD_STEAM_PROC(GetNumLobbyMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    LOAD_STEAM_PROC(GetLobbyMemberLimit, "SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit");

    LOAD_STEAM_PROC(SteamNetworking, "SteamAPI_SteamNetworking_v006");
    LOAD_STEAM_PROC(AllowP2PPacketRelay, "SteamAPI_ISteamNetworking_AllowP2PPacketRelay");
    LOAD_STEAM_PROC(SendP2PPacket, "SteamAPI_ISteamNetworking_SendP2PPacket");
    LOAD_STEAM_PROC(IsP2PPacketAvailable, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable");
    LOAD_STEAM_PROC(ReadP2PPacket, "SteamAPI_ISteamNetworking_ReadP2PPacket");
    LOAD_STEAM_PROC(AcceptP2PSessionWithUser, "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser");
    LOAD_STEAM_PROC(CloseP2PSessionWithUser, "SteamAPI_ISteamNetworking_CloseP2PSessionWithUser");

    #undef LOAD_STEAM_PROC

    if (!g_steam.InitSafe || !g_steam.InitSafe()) {
        return false;
    }

    if (g_steam.ManualDispatch_Init && g_steam.GetHSteamPipe) {
        g_steam.ManualDispatch_Init();
        steam_pipe_ = g_steam.GetHSteamPipe();
    }

    if (g_steam.SteamUser) steam_user_ = g_steam.SteamUser();
    if (g_steam.SteamFriends) steam_friends_ = g_steam.SteamFriends();
    if (g_steam.SteamMatchmaking) steam_matchmaking_ = g_steam.SteamMatchmaking();
    if (g_steam.SteamNetworking) steam_networking_ = g_steam.SteamNetworking();

    if (steam_user_ && g_steam.GetSteamID) {
        uint64_t sid = g_steam.GetSteamID(steam_user_);
        if (sid != 0) local_peer_id_ = sid;
    }
    if (steam_friends_ && g_steam.GetPersonaName) {
        const char* persona = g_steam.GetPersonaName(steam_friends_);
        if (persona && persona[0] != '\0') {
            local_player_name_ = EarthSecurity::SanitizeDisplayString(persona, 32, "Explorer");
        }
    }
    // Always enforce Valve SDR Relay so users' real IP addresses are never exposed
    if (steam_networking_ && g_steam.AllowP2PPacketRelay) {
        g_steam.AllowP2PPacketRelay(steam_networking_, true);
    }

    steam_initialized_.store(true);
    return true;
}

bool NetworkManager::StartServer(const std::string& password) {
    CloseServerOrLeave();

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        server_password_ = password;
        password_salt_ = EarthSecurity::GenerateRandomSaltHex(16);
        session_token_.store(EarthSecurity::GenerateRandomToken64());
        session_host_id_.store(local_peer_id_);
        session_locked_.store(false);
        has_password_.store(!password.empty());
        authorized_peers_.clear();
        pose_rate_limiter_.Clear();
        voice_rate_limiter_.Clear();
        ctrl_rate_limiter_.Clear();
    }

    uint64_t local_lobby = (local_peer_id_ & 0xFFFFFFFF00000000ULL) | 0x0000000045564D50ULL;
    current_lobby_id_.store(local_lobby);
    session_state_.store(SessionState::Hosting);

    if (steam_initialized_.load() && steam_matchmaking_ && g_steam.CreateLobby) {
        // 2 = k_ELobbyTypePublic (discoverable in public session browser)
        g_steam.CreateLobby(steam_matchmaking_, 2, static_cast<int>(kMaxSessionPeers));
        SetStatusMessage(password.empty() ? "HOSTING PUBLIC SERVER (OPEN)" : "HOSTING SERVER (PASSWORD PROTECTED)");
    } else {
        SetStatusMessage(password.empty() ? "HOSTING PUBLIC SERVER (OPEN)" : "HOSTING SERVER (PASSWORD PROTECTED)");
    }
    return true;
}

void NetworkManager::CloseServerOrLeave() {
    SessionState prev_state = session_state_.exchange(SessionState::Offline);
    uint64_t token = session_token_.exchange(0);
    uint64_t lobby = current_lobby_id_.exchange(0);
    session_host_id_.store(0);
    session_locked_.store(false);
    has_password_.store(false);

    if (prev_state == SessionState::Hosting && token != 0) {
        PacketHeader close_pkt = {};
        close_pkt.type = kPacketSessionClosed;
        close_pkt.sender_id = local_peer_id_;
        close_pkt.session_token = token;

        std::lock_guard<std::mutex> lock(session_mutex_);
        for (uint64_t peer : authorized_peers_) {
            SendDirectPacket(peer, &close_pkt, sizeof(close_pkt), true, 0);
            if (steam_initialized_.load() && steam_networking_ && g_steam.CloseP2PSessionWithUser) {
                g_steam.CloseP2PSessionWithUser(steam_networking_, peer);
            }
        }
        authorized_peers_.clear();
    } else if ((prev_state == SessionState::InSession || prev_state == SessionState::Joining) &&
               steam_initialized_.load() && steam_networking_ && g_steam.CloseP2PSessionWithUser) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (uint64_t peer : authorized_peers_) {
            g_steam.CloseP2PSessionWithUser(steam_networking_, peer);
        }
        authorized_peers_.clear();
    }

    if (lobby != 0 && steam_initialized_.load() && steam_matchmaking_ && g_steam.LeaveLobby) {
        g_steam.LeaveLobby(steam_matchmaking_, lobby);
    }

    {
        std::lock_guard<std::mutex> lock(players_mutex_);
        remote_players_.clear();
    }

    SetStatusMessage("OFFLINE - SERVER CLOSED");
}

void NetworkManager::SetSessionLocked(bool locked) {
    if (session_state_.load() != SessionState::Hosting) return;
    session_locked_.store(locked);

    uint64_t lobby = current_lobby_id_.load();
    if (lobby != 0 && steam_initialized_.load() && steam_matchmaking_) {
        if (g_steam.SetLobbyJoinable) {
            g_steam.SetLobbyJoinable(steam_matchmaking_, lobby, !locked);
        }
        if (g_steam.SetLobbyData) {
            g_steam.SetLobbyData(steam_matchmaking_, lobby, "locked", locked ? "1" : "0");
        }
    }
    SetStatusMessage(locked ? "HOSTING (SESSION LOCKED)" : "HOSTING (SESSION UNLOCKED)");
}

void NetworkManager::KickPlayer(uint64_t target_peer_id) {
    if (session_state_.load() != SessionState::Hosting || target_peer_id == 0) return;

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        authorized_peers_.erase(target_peer_id);
    }

    KickPacket kick_pkt = {};
    kick_pkt.hdr.type = kPacketKick;
    kick_pkt.hdr.sender_id = local_peer_id_;
    kick_pkt.hdr.session_token = session_token_.load();
    kick_pkt.kicked_peer_id = target_peer_id;
    kick_pkt.is_ban = 0;
    SendDirectPacket(target_peer_id, &kick_pkt, sizeof(kick_pkt), true, 0);

    if (steam_initialized_.load() && steam_networking_ && g_steam.CloseP2PSessionWithUser) {
        g_steam.CloseP2PSessionWithUser(steam_networking_, target_peer_id);
    }

    std::string kicked_name = "PLAYER";
    {
        std::lock_guard<std::mutex> lock(players_mutex_);
        auto it = remote_players_.find(target_peer_id);
        if (it != remote_players_.end()) {
            kicked_name = it->second.name;
            remote_players_.erase(it);
        }
    }
    last_peer_list_ms_ = 0; // Broadcast updated peer list immediately
    SetStatusMessage("KICKED " + kicked_name);
}

void NetworkManager::BanPlayer(uint64_t target_peer_id, const std::string& player_name) {
    if (target_peer_id == 0 || target_peer_id == local_peer_id_) return;

    std::string resolved_name = EarthSecurity::SanitizeDisplayString(player_name.c_str(), 32, "");
    {
        std::lock_guard<std::mutex> lock(players_mutex_);
        auto it = remote_players_.find(target_peer_id);
        if (it != remote_players_.end()) {
            if (resolved_name.empty()) resolved_name = it->second.name;
            remote_players_.erase(it);
        }
    }
    if (resolved_name.empty()) {
        char fallback[32];
        std::snprintf(fallback, sizeof(fallback), "PEER_%04llX", static_cast<unsigned long long>(target_peer_id & 0xFFFFULL));
        resolved_name = fallback;
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        banned_peers_[target_peer_id] = resolved_name;
        authorized_peers_.erase(target_peer_id);
    }

    if (session_state_.load() == SessionState::Hosting) {
        KickPacket kick_pkt = {};
        kick_pkt.hdr.type = kPacketKick;
        kick_pkt.hdr.sender_id = local_peer_id_;
        kick_pkt.hdr.session_token = session_token_.load();
        kick_pkt.kicked_peer_id = target_peer_id;
        kick_pkt.is_ban = 1;
        SendDirectPacket(target_peer_id, &kick_pkt, sizeof(kick_pkt), true, 0);
        last_peer_list_ms_ = 0;
    }

    if (steam_initialized_.load() && steam_networking_ && g_steam.CloseP2PSessionWithUser) {
        g_steam.CloseP2PSessionWithUser(steam_networking_, target_peer_id);
    }

    SetStatusMessage("BANNED " + resolved_name);
}

void NetworkManager::UnbanPlayer(uint64_t target_peer_id) {
    if (target_peer_id == 0) return;
    std::string unbanned_name = "PLAYER";
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        auto it = banned_peers_.find(target_peer_id);
        if (it != banned_peers_.end()) {
            unbanned_name = it->second;
            banned_peers_.erase(it);
            removed = true;
        }
    }
    if (removed) {
        SetStatusMessage("UNBANNED " + unbanned_name);
    }
}

bool NetworkManager::IsPlayerBanned(uint64_t target_peer_id) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return banned_peers_.find(target_peer_id) != banned_peers_.end();
}

std::vector<BannedPlayerInfo> NetworkManager::GetBannedPlayers() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    std::vector<BannedPlayerInfo> out;
    out.reserve(banned_peers_.size());
    for (const auto& kv : banned_peers_) {
        out.push_back({kv.first, kv.second});
    }
    std::sort(out.begin(), out.end(), [](const BannedPlayerInfo& a, const BannedPlayerInfo& b) {
        return a.peer_id < b.peer_id;
    });
    return out;
}

void NetworkManager::RefreshPublicSessions() {
    size_t current_count = 0;
    {
        std::lock_guard<std::mutex> lock(browser_mutex_);
        current_count = public_sessions_.size();
    }
    if (current_count > 0) {
        SetStatusMessage("FOUND " + std::to_string(current_count) + " PUBLIC SESSION(S)");
    } else {
        SetStatusMessage("SEARCHING FOR PUBLIC SESSIONS...");
    }
    if (steam_initialized_.load() && steam_matchmaking_ &&
        g_steam.AddRequestLobbyListStringFilter && g_steam.RequestLobbyList) {
        if (g_steam.AddRequestLobbyListDistanceFilter) {
            // 3 = k_ELobbyDistanceFilterWorldwide
            g_steam.AddRequestLobbyListDistanceFilter(steam_matchmaking_, 3);
        }
        // Only match open (unlocked) EarthVR Multiplayer v2 lobbies
        g_steam.AddRequestLobbyListStringFilter(steam_matchmaking_, "earthvr_mp_app", "evrmp_v2", 0);
        g_steam.AddRequestLobbyListStringFilter(steam_matchmaking_, "locked", "0", 0);
        g_steam.RequestLobbyList(steam_matchmaking_);
    } else {
        SetStatusMessage("FOUND " + std::to_string(current_count) + " PUBLIC SESSION(S)");
    }
}

std::vector<PublicSessionInfo> NetworkManager::GetPublicSessions() {
    std::lock_guard<std::mutex> lock(browser_mutex_);
    return public_sessions_;
}

bool NetworkManager::JoinPublicSession(uint64_t lobby_id, const std::string& password) {
    if (lobby_id == 0) return false;

    PublicSessionInfo target_info;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(browser_mutex_);
        for (const auto& s : public_sessions_) {
            if (s.lobby_id == lobby_id) {
                target_info = s;
                found = true;
                break;
            }
        }
    }

    CloseServerOrLeave();

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        pending_join_password_ = password;
        password_salt_ = target_info.pw_salt;
        authorized_peers_.clear();
    }

    current_lobby_id_.store(lobby_id);
    session_state_.store(SessionState::Joining);
    join_start_ms_ = GetTickCount64();
    SetStatusMessage("JOINING SESSION...");

    if (found && target_info.is_lan_session && target_info.host_id != 0) {
        session_host_id_.store(target_info.host_id);
        {
            std::lock_guard<std::mutex> lock(udp_mutex_);
            udp_peer_addrs_[target_info.host_id] = target_info.lan_addr;
        }
        JoinRequestPacket req = {};
        req.hdr.type = kPacketJoinRequest;
        req.hdr.sender_id = local_peer_id_;
        req.lobby_id = lobby_id;
        std::snprintf(req.player_name, sizeof(req.player_name), "%s", local_player_name_.c_str());
        EarthSecurity::ComputeJoinAuthHash(target_info.pw_salt, password, local_peer_id_, lobby_id, req.auth_hash);
        SendDirectPacket(target_info.host_id, &req, sizeof(req), true, 0);
        return true;
    }

    if (steam_initialized_.load() && steam_matchmaking_ && g_steam.JoinLobby) {
        g_steam.JoinLobby(steam_matchmaking_, lobby_id);
        return true;
    }
    return false;
}

void NetworkManager::SendDirectPacket(uint64_t target_peer_id, const void* data, size_t len, bool reliable, int channel) {
    if (target_peer_id == 0 || target_peer_id == local_peer_id_) return;

    if (steam_initialized_.load() && steam_networking_ && g_steam.SendP2PPacket) {
        // 2 = k_EP2PSendReliable, 1 = k_EP2PSendUnreliableNoDelay
        int send_type = reliable ? 2 : 1;
        g_steam.SendP2PPacket(steam_networking_, target_peer_id, data, static_cast<uint32_t>(len), send_type, channel);
    }

    if (udp_sock_ != INVALID_SOCKET) {
        std::lock_guard<std::mutex> lock(udp_mutex_);
        auto it = udp_peer_addrs_.find(target_peer_id);
        if (it != udp_peer_addrs_.end()) {
            sendto(udp_sock_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0,
                   reinterpret_cast<const sockaddr*>(&it->second), sizeof(it->second));
        } else {
            for (const auto& t : udp_broadcast_targets_) {
                sendto(udp_sock_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0,
                       reinterpret_cast<const sockaddr*>(&t), sizeof(t));
            }
        }
    }
}

void NetworkManager::BroadcastPoseAndPeerList(const PosePacket& pose_pkt) {
    SessionState state = session_state_.load();
    if (state != SessionState::Hosting && state != SessionState::InSession) return;

    uint64_t now = GetTickCount64();
    std::vector<uint64_t> targets;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        targets.assign(authorized_peers_.begin(), authorized_peers_.end());
    }

    for (uint64_t peer_id : targets) {
        SendDirectPacket(peer_id, &pose_pkt, sizeof(pose_pkt), false, 0);
    }

    // If Hosting, periodically broadcast the authenticated peer list (and local LAN beacon if UDP enabled)
    if (state == SessionState::Hosting && now - last_peer_list_ms_ >= 1000) {
        last_peer_list_ms_ = now;

        PeerListPacket plist = {};
        plist.hdr.type = kPacketPeerList;
        plist.hdr.sender_id = local_peer_id_;
        plist.hdr.session_token = session_token_.load();
        plist.locked = session_locked_.load() ? 1 : 0;
        plist.peer_ids[plist.peer_count++] = local_peer_id_;
        for (uint64_t peer_id : targets) {
            if (plist.peer_count < kMaxSessionPeers) {
                plist.peer_ids[plist.peer_count++] = peer_id;
            }
        }
        for (uint64_t peer_id : targets) {
            SendDirectPacket(peer_id, &plist, sizeof(plist), true, 0);
        }

        if (udp_sock_ != INVALID_SOCKET && !session_locked_.load()) {
            LanBeaconPacket beacon = {};
            beacon.hdr.type = kPacketLanBeacon;
            beacon.hdr.sender_id = local_peer_id_;
            beacon.lobby_id = current_lobby_id_.load();
            std::snprintf(beacon.host_name, sizeof(beacon.host_name), "%s", local_player_name_.c_str());
            beacon.member_count = plist.peer_count;
            beacon.max_members = static_cast<uint8_t>(kMaxSessionPeers);
            beacon.has_password = has_password_.load() ? 1 : 0;
            beacon.is_locked = session_locked_.load() ? 1 : 0;
            {
                std::lock_guard<std::mutex> lock(session_mutex_);
                std::snprintf(beacon.pw_salt, sizeof(beacon.pw_salt), "%s", password_salt_.c_str());
            }
            std::lock_guard<std::mutex> lock(udp_mutex_);
            for (const auto& t : udp_broadcast_targets_) {
                sendto(udp_sock_, reinterpret_cast<const char*>(&beacon), sizeof(beacon), 0,
                       reinterpret_cast<const sockaddr*>(&t), sizeof(t));
            }
        }
    }
}

void NetworkManager::BroadcastVoicePacket(const uint8_t* adpcm_data, size_t len) {
    SessionState state = session_state_.load();
    if (state != SessionState::Hosting && state != SessionState::InSession) return;
    if (len > sizeof(VoicePacket::payload)) return;

    VoicePacket pkt = {};
    pkt.hdr.type = kPacketVoice;
    pkt.hdr.sender_id = local_peer_id_;
    pkt.hdr.session_token = session_token_.load();
    pkt.hdr.seq = ++voice_seq_;
    pkt.codec = 0;
    pkt.payload_len = static_cast<uint16_t>(len);
    std::memcpy(pkt.payload, adpcm_data, len);
    size_t total_bytes = sizeof(PacketHeader) + 3 + len;

    std::vector<uint64_t> targets;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        targets.assign(authorized_peers_.begin(), authorized_peers_.end());
    }
    for (uint64_t peer_id : targets) {
        SendDirectPacket(peer_id, &pkt, total_bytes, false, 1);
    }
}

void NetworkManager::PollSteamCallbacksAndPackets() {
    if (!steam_initialized_.load()) return;

    if (steam_pipe_ != 0 && g_steam.ManualDispatch_RunFrame && g_steam.ManualDispatch_GetNextCallback) {
        g_steam.ManualDispatch_RunFrame(steam_pipe_);
        SteamCallbackMsg_t msg = {};
        while (g_steam.ManualDispatch_GetNextCallback(steam_pipe_, &msg)) {
            if (msg.m_iCallback == kCallback_P2PSessionRequest && msg.m_pubParam) {
                auto* req = reinterpret_cast<const P2PSessionRequest_t*>(msg.m_pubParam);
                uint64_t remote_sid = req->m_steamIDRemote;
                SessionState st = session_state_.load();

                // Security Gate: Only accept P2P handshakes when Hosting (unlocked or existing peer) or in an active session
                bool allow_handshake = false;
                {
                    std::lock_guard<std::mutex> lock(session_mutex_);
                    if (banned_peers_.find(remote_sid) == banned_peers_.end()) {
                        if (st == SessionState::Hosting) {
                            allow_handshake = (!session_locked_.load() || authorized_peers_.count(remote_sid) > 0);
                        } else if (st == SessionState::Joining || st == SessionState::InSession) {
                            allow_handshake = (remote_sid == session_host_id_.load() || authorized_peers_.count(remote_sid) > 0);
                        }
                    }
                }
                if (allow_handshake && steam_networking_ && g_steam.AcceptP2PSessionWithUser) {
                    g_steam.AcceptP2PSessionWithUser(steam_networking_, remote_sid);
                } else if (steam_networking_ && g_steam.CloseP2PSessionWithUser) {
                    g_steam.CloseP2PSessionWithUser(steam_networking_, remote_sid);
                }
            } else if (msg.m_iCallback == kCallback_SteamAPICallCompleted && msg.m_pubParam && g_steam.ManualDispatch_GetAPICallResult) {
                auto* completed = reinterpret_cast<const SteamAPICallCompleted_t*>(msg.m_pubParam);
                bool failed = false;

                if (completed->m_iCallback == kCallback_LobbyCreated) {
                    LobbyCreated_t created = {};
                    if (g_steam.ManualDispatch_GetAPICallResult(steam_pipe_, completed->m_hAsyncCall, &created, sizeof(created), kCallback_LobbyCreated, &failed) && !failed) {
                        if (created.m_eResult == 1 && created.m_ulSteamIDLobby != 0 && session_state_.load() == SessionState::Hosting) {
                            uint64_t lid = created.m_ulSteamIDLobby;
                            current_lobby_id_.store(lid);
                            if (g_steam.SetLobbyData) {
                                g_steam.SetLobbyData(steam_matchmaking_, lid, "earthvr_mp_app", "evrmp_v2");
                                g_steam.SetLobbyData(steam_matchmaking_, lid, "host_name", local_player_name_.c_str());
                                g_steam.SetLobbyData(steam_matchmaking_, lid, "pw_required", has_password_.load() ? "1" : "0");
                                g_steam.SetLobbyData(steam_matchmaking_, lid, "locked", session_locked_.load() ? "1" : "0");
                                std::string salt = GetCurrentPasswordSalt();
                                g_steam.SetLobbyData(steam_matchmaking_, lid, "pw_salt", salt.c_str());
                            }
                            SetStatusMessage(has_password_.load() ? "HOSTING SERVER (PASSWORD PROTECTED)" : "HOSTING PUBLIC SERVER (OPEN)");
                        } else {
                            session_state_.store(SessionState::Offline);
                            SetStatusMessage("FAILED TO CREATE STEAM LOBBY");
                        }
                    }
                } else if (completed->m_iCallback == kCallback_LobbyMatchList) {
                    LobbyMatchList_t list = {};
                    if (g_steam.ManualDispatch_GetAPICallResult(steam_pipe_, completed->m_hAsyncCall, &list, sizeof(list), kCallback_LobbyMatchList, &failed) && !failed) {
                        std::vector<PublicSessionInfo> found_list;
                        uint32_t count = std::min(list.m_nLobbiesMatching, 32u);
                        for (uint32_t i = 0; i < count; ++i) {
                            uint64_t lid = g_steam.GetLobbyByIndex ? g_steam.GetLobbyByIndex(steam_matchmaking_, static_cast<int>(i)) : 0;
                            if (lid == 0 || lid == current_lobby_id_.load()) continue;

                            const char* locked_str = g_steam.GetLobbyData ? g_steam.GetLobbyData(steam_matchmaking_, lid, "locked") : "0";
                            if (locked_str && locked_str[0] == '1') continue; // Skip locked sessions

                            PublicSessionInfo info;
                            info.lobby_id = lid;
                            info.host_id = g_steam.GetLobbyOwner ? g_steam.GetLobbyOwner(steam_matchmaking_, lid) : 0;
                            const char* hname = g_steam.GetLobbyData ? g_steam.GetLobbyData(steam_matchmaking_, lid, "host_name") : "";
                            info.host_name = EarthSecurity::SanitizeDisplayString(hname, 32, "Steam Host");
                            const char* pw_req = g_steam.GetLobbyData ? g_steam.GetLobbyData(steam_matchmaking_, lid, "pw_required") : "0";
                            info.has_password = (pw_req && pw_req[0] == '1');
                            const char* salt = g_steam.GetLobbyData ? g_steam.GetLobbyData(steam_matchmaking_, lid, "pw_salt") : "";
                            info.pw_salt = EarthSecurity::SanitizeDisplayString(salt, 40, "");
                            info.member_count = g_steam.GetNumLobbyMembers ? g_steam.GetNumLobbyMembers(steam_matchmaking_, lid) : 1;
                            info.max_members = g_steam.GetLobbyMemberLimit ? g_steam.GetLobbyMemberLimit(steam_matchmaking_, lid) : 16;
                            if (info.max_members <= 0) info.max_members = 16;
                            info.last_seen_ms = GetTickCount64();
                            found_list.push_back(info);
                        }
                        {
                            std::lock_guard<std::mutex> lock(browser_mutex_);
                            // Preserve any recent LAN sessions while updating Steam sessions
                            for (const auto& existing : public_sessions_) {
                                if (existing.is_lan_session && GetTickCount64() - existing.last_seen_ms < 5000) {
                                    found_list.push_back(existing);
                                }
                            }
                            public_sessions_ = std::move(found_list);
                        }
                        SetStatusMessage("FOUND " + std::to_string(public_sessions_.size()) + " OPEN SESSION(S)");
                    }
                } else if (completed->m_iCallback == kCallback_LobbyEnter) {
                    LobbyEnter_t entered = {};
                    if (g_steam.ManualDispatch_GetAPICallResult(steam_pipe_, completed->m_hAsyncCall, &entered, sizeof(entered), kCallback_LobbyEnter, &failed) && !failed) {
                        uint64_t lid = entered.m_ulSteamIDLobby;
                        if (lid != 0 && session_state_.load() == SessionState::Joining && g_steam.GetLobbyOwner) {
                            uint64_t owner_id = g_steam.GetLobbyOwner(steam_matchmaking_, lid);
                            if (owner_id != 0 && owner_id != local_peer_id_) {
                                session_host_id_.store(owner_id);
                                const char* salt = g_steam.GetLobbyData ? g_steam.GetLobbyData(steam_matchmaking_, lid, "pw_salt") : "";
                                std::string salt_str = EarthSecurity::SanitizeDisplayString(salt, 40, "");
                                std::string pwd;
                                {
                                    std::lock_guard<std::mutex> lock(session_mutex_);
                                    if (!salt_str.empty()) password_salt_ = salt_str;
                                    else salt_str = password_salt_;
                                    pwd = pending_join_password_;
                                }
                                if (steam_networking_ && g_steam.AcceptP2PSessionWithUser) {
                                    g_steam.AcceptP2PSessionWithUser(steam_networking_, owner_id);
                                }
                                JoinRequestPacket req = {};
                                req.hdr.type = kPacketJoinRequest;
                                req.hdr.sender_id = local_peer_id_;
                                req.lobby_id = lid;
                                std::snprintf(req.player_name, sizeof(req.player_name), "%s", local_player_name_.c_str());
                                EarthSecurity::ComputeJoinAuthHash(salt_str, pwd, local_peer_id_, lid, req.auth_hash);
                                SendDirectPacket(owner_id, &req, sizeof(req), true, 0);
                                SetStatusMessage("AUTHENTICATING WITH HOST...");
                            }
                        }
                    }
                }
            }
            g_steam.ManualDispatch_FreeLastCallback(steam_pipe_);
        }
    }

    // Read incoming Steam P2P packets and authenticate sender_id against Steam's remote_sid
    if (steam_networking_ && g_steam.IsP2PPacketAvailable && g_steam.ReadP2PPacket) {
        uint8_t buf[1024];
        for (int channel = 0; channel <= 1; ++channel) {
            uint32_t msg_size = 0;
            int max_reads = 64;
            while (max_reads-- > 0 && g_steam.IsP2PPacketAvailable(steam_networking_, &msg_size, channel)) {
                if (msg_size > 0 && msg_size <= sizeof(buf)) {
                    uint32_t bytes_read = 0;
                    uint64_t remote_sid = 0;
                    if (g_steam.ReadP2PPacket(steam_networking_, buf, sizeof(buf), &bytes_read, &remote_sid, channel)) {
                        if (remote_sid != 0 && remote_sid != local_peer_id_) {
                            HandleAuthenticatedPacket(remote_sid, buf, bytes_read, nullptr);
                        }
                    }
                } else {
                    // Drain oversized packet safely
                    uint8_t dummy[16];
                    uint32_t bytes_read = 0;
                    uint64_t remote_sid = 0;
                    g_steam.ReadP2PPacket(steam_networking_, dummy, sizeof(dummy), &bytes_read, &remote_sid, channel);
                    break;
                }
            }
        }
    }
}

void NetworkManager::PollUdpPackets() {
    if (udp_sock_ == INVALID_SOCKET) return;
    uint8_t buf[1024];
    sockaddr_in from = {};
    int from_len = sizeof(from);
    int max_reads = 64;
    while (max_reads-- > 0) {
        int received = recvfrom(udp_sock_, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                                reinterpret_cast<sockaddr*>(&from), &from_len);
        if (received < static_cast<int>(sizeof(PacketHeader))) break;
        const auto* hdr = reinterpret_cast<const PacketHeader*>(buf);
        if (hdr->magic == kPacketMagic && hdr->sender_id != 0 && hdr->sender_id != local_peer_id_) {
            HandleAuthenticatedPacket(hdr->sender_id, buf, static_cast<size_t>(received), &from);
        }
    }
}

void NetworkManager::InjectTestPacket(uint64_t authenticated_sender_id, const void* data, size_t len) {
    HandleAuthenticatedPacket(authenticated_sender_id, reinterpret_cast<const uint8_t*>(data), len, nullptr);
    if (data && len >= sizeof(PosePacket)) {
        const auto* hdr = reinterpret_cast<const PacketHeader*>(data);
        if (hdr->type == kPacketPose) {
            std::lock_guard<std::mutex> lock(players_mutex_);
            auto it = remote_players_.find(authenticated_sender_id);
            if (it != remote_players_.end()) {
                it->second.prev_snap = it->second.target_snap;
                it->second.interp_snap = it->second.target_snap;
            }
        }
    }
}

void NetworkManager::HandleAuthenticatedPacket(
    uint64_t authenticated_sender_id,
    const uint8_t* data,
    size_t len,
    const sockaddr_in* udp_from) {

    if (!data || len < sizeof(PacketHeader)) return;
    const auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic != kPacketMagic || hdr->version != kProtocolVersion) return;

    // Anti-Spoofing Check: Packet's claimed sender_id MUST match the transport's authenticated sender ID
    if (hdr->sender_id == 0 || hdr->sender_id == local_peer_id_ || hdr->sender_id != authenticated_sender_id) {
        return;
    }

    uint64_t now = GetTickCount64();

    // Handle LAN Discovery Beacon (allowed even when Offline so Server Browser works locally)
    if (hdr->type == kPacketLanBeacon && len >= sizeof(LanBeaconPacket)) {
        const auto* b = reinterpret_cast<const LanBeaconPacket*>(data);
        if (b->is_locked != 0) return;
        std::lock_guard<std::mutex> lock(browser_mutex_);
        PublicSessionInfo* target = nullptr;
        for (auto& s : public_sessions_) {
            if (s.lobby_id == b->lobby_id) {
                target = &s;
                break;
            }
        }
        if (!target && public_sessions_.size() < 32) {
            public_sessions_.push_back({});
            target = &public_sessions_.back();
        }
        if (target) {
            target->lobby_id = b->lobby_id;
            target->host_id = authenticated_sender_id;
            target->host_name = EarthSecurity::SanitizeDisplayString(b->host_name, sizeof(b->host_name), "LAN Host");
            target->member_count = std::clamp(static_cast<int>(b->member_count), 1, 16);
            target->max_members = std::clamp(static_cast<int>(b->max_members), 1, 16);
            target->has_password = (b->has_password != 0);
            target->is_locked = (b->is_locked != 0);
            target->pw_salt = EarthSecurity::SanitizeDisplayString(b->pw_salt, sizeof(b->pw_salt), "");
            target->is_lan_session = true;
            if (udp_from) target->lan_addr = *udp_from;
            target->last_seen_ms = now;
        }
        return;
    }

    SessionState state = session_state_.load();
    if (state == SessionState::Offline) {
        return; // Drop all unsolicited traffic when Offline
    }

    // Check Persistent Ban list and Per-Peer Rate Limits
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (banned_peers_.find(authenticated_sender_id) != banned_peers_.end()) {
            return;
        }
        if (hdr->type == kPacketPose) {
            if (!pose_rate_limiter_.AllowPacket(authenticated_sender_id, now, 120)) return;
        } else if (hdr->type == kPacketVoice) {
            if (!voice_rate_limiter_.AllowPacket(authenticated_sender_id, now, 100)) return;
        } else {
            if (!ctrl_rate_limiter_.AllowPacket(authenticated_sender_id, now, 20)) return;
        }
    }

    if (udp_from) {
        std::lock_guard<std::mutex> lock(udp_mutex_);
        udp_peer_addrs_[authenticated_sender_id] = *udp_from;
    }

    // === 1. Host Handling Join Request ===
    if (hdr->type == kPacketJoinRequest && len >= sizeof(JoinRequestPacket)) {
        if (state != SessionState::Hosting) return;
        const auto* req = reinterpret_cast<const JoinRequestPacket*>(data);

        uint8_t reject_reason = kRejectNone;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            if (banned_peers_.find(authenticated_sender_id) != banned_peers_.end()) {
                reject_reason = kRejectBanned;
            } else if (session_locked_.load() && authorized_peers_.count(authenticated_sender_id) == 0) {
                reject_reason = kRejectSessionLocked;
            } else if (authorized_peers_.size() >= kMaxSessionPeers - 1 && authorized_peers_.count(authenticated_sender_id) == 0) {
                reject_reason = kRejectSessionFull;
            } else if (has_password_.load()) {
                uint8_t expected_hash[32] = {};
                EarthSecurity::ComputeJoinAuthHash(password_salt_, server_password_, authenticated_sender_id, current_lobby_id_.load(), expected_hash);
                if (!EarthSecurity::ConstantTimeEquals(expected_hash, req->auth_hash, 32)) {
                    reject_reason = kRejectWrongPassword;
                }
            }

            if (reject_reason == kRejectNone) {
                authorized_peers_.insert(authenticated_sender_id);
            }
        }

        if (reject_reason != kRejectNone) {
            JoinRejectPacket rej = {};
            rej.hdr.type = kPacketJoinReject;
            rej.hdr.sender_id = local_peer_id_;
            rej.reason = reject_reason;
            SendDirectPacket(authenticated_sender_id, &rej, sizeof(rej), true, 0);
            return;
        }

        JoinAcceptPacket acc = {};
        acc.hdr.type = kPacketJoinAccept;
        acc.hdr.sender_id = local_peer_id_;
        acc.hdr.session_token = session_token_.load();
        acc.assigned_session_token = session_token_.load();
        std::snprintf(acc.host_name, sizeof(acc.host_name), "%s", local_player_name_.c_str());
        SendDirectPacket(authenticated_sender_id, &acc, sizeof(acc), true, 0);

        std::string joiner_name = EarthSecurity::SanitizeDisplayString(req->player_name, sizeof(req->player_name), "Player");
        SetStatusMessage(joiner_name + " JOINED THE SESSION");
        last_peer_list_ms_ = 0; // Trigger immediate peer list broadcast
        return;
    }

    // === 2. Client Handling Join Accept / Reject from Host ===
    if (hdr->type == kPacketJoinAccept && len >= sizeof(JoinAcceptPacket)) {
        if (state != SessionState::Joining || authenticated_sender_id != session_host_id_.load()) return;
        const auto* acc = reinterpret_cast<const JoinAcceptPacket*>(data);
        if (acc->assigned_session_token == 0) return;

        session_token_.store(acc->assigned_session_token);
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            authorized_peers_.insert(authenticated_sender_id);
        }
        session_state_.store(SessionState::InSession);
        std::string hname = EarthSecurity::SanitizeDisplayString(acc->host_name, sizeof(acc->host_name), "Host");
        SetStatusMessage("CONNECTED TO " + hname);
        return;
    }

    if (hdr->type == kPacketJoinReject && len >= sizeof(JoinRejectPacket)) {
        if (state != SessionState::Joining || authenticated_sender_id != session_host_id_.load()) return;
        const auto* rej = reinterpret_cast<const JoinRejectPacket*>(data);
        CloseServerOrLeave();
        switch (rej->reason) {
            case kRejectWrongPassword: SetStatusMessage("JOIN FAILED: WRONG PASSWORD"); break;
            case kRejectSessionLocked: SetStatusMessage("JOIN FAILED: SESSION IS LOCKED"); break;
            case kRejectBanned:        SetStatusMessage("JOIN FAILED: BANNED BY HOST"); break;
            case kRejectSessionFull:   SetStatusMessage("JOIN FAILED: SESSION IS FULL"); break;
            default:                   SetStatusMessage("JOIN REJECTED BY HOST"); break;
        }
        return;
    }

    // === All Subsequent Packets Require Valid Session Token ===
    uint64_t active_token = session_token_.load();
    if (active_token == 0 || hdr->session_token != active_token) {
        return;
    }

    // === 3. Host Control Packets (Kick / Ban / SessionClosed / PeerList) ===
    if (hdr->type == kPacketKick && len >= sizeof(KickPacket)) {
        if (authenticated_sender_id != session_host_id_.load()) return;
        const auto* k = reinterpret_cast<const KickPacket*>(data);
        if (k->kicked_peer_id == local_peer_id_) {
            CloseServerOrLeave();
            SetStatusMessage(k->is_ban ? "YOU WERE BANNED BY THE HOST" : "YOU WERE KICKED BY THE HOST");
        }
        return;
    }

    if (hdr->type == kPacketSessionClosed) {
        if (authenticated_sender_id != session_host_id_.load()) return;
        CloseServerOrLeave();
        SetStatusMessage("HOST CLOSED THE SESSION");
        return;
    }

    if (hdr->type == kPacketPeerList && len >= sizeof(PeerListPacket)) {
        if (authenticated_sender_id != session_host_id_.load()) return;
        const auto* pl = reinterpret_cast<const PeerListPacket*>(data);
        uint8_t count = std::min(pl->peer_count, static_cast<uint8_t>(kMaxSessionPeers));

        std::unordered_set<uint64_t> new_peers;
        for (uint8_t i = 0; i < count; ++i) {
            uint64_t pid = pl->peer_ids[i];
            if (pid != 0 && pid != local_peer_id_) {
                new_peers.insert(pid);
                if (steam_initialized_.load() && steam_networking_ && g_steam.AcceptP2PSessionWithUser) {
                    g_steam.AcceptP2PSessionWithUser(steam_networking_, pid);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            authorized_peers_ = new_peers;
        }
        {
            std::lock_guard<std::mutex> lock(players_mutex_);
            for (auto it = remote_players_.begin(); it != remote_players_.end(); ) {
                if (new_peers.count(it->first) == 0) {
                    it = remote_players_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        return;
    }

    // Verify sender is in authorized_peers_ before accepting Pose or Voice
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (authorized_peers_.count(authenticated_sender_id) == 0) {
            return;
        }
    }

    // === 4. Authenticated Pose & Voice Packets ===
    if (hdr->type == kPacketPose && len >= sizeof(PosePacket)) {
        const auto* pkt = reinterpret_cast<const PosePacket*>(data);
        if (!std::isfinite(pkt->viewer_scale) || pkt->viewer_scale <= 0.0 ||
            !std::isfinite(pkt->hmd_pos_kh.x) || !std::isfinite(pkt->hmd_pos_kh.y) || !std::isfinite(pkt->hmd_pos_kh.z) ||
            !std::isfinite(pkt->hmd_rot_kh.x) || !std::isfinite(pkt->hmd_rot_kh.y) || !std::isfinite(pkt->hmd_rot_kh.z) || !std::isfinite(pkt->hmd_rot_kh.w) ||
            !std::isfinite(pkt->voice_level)) {
            return;
        }

        std::lock_guard<std::mutex> lock(players_mutex_);
        if (remote_players_.find(authenticated_sender_id) == remote_players_.end() &&
            remote_players_.size() >= kMaxSessionPeers) {
            return;
        }
        auto& player = remote_players_[authenticated_sender_id];
        bool is_new = (player.peer_id == 0);
        player.peer_id = authenticated_sender_id;
        player.last_seq = hdr->seq;
        player.last_seen_ms = now;
        player.name = EarthSecurity::SanitizeDisplayString(pkt->player_name, sizeof(pkt->player_name), "Friend");
        player.color_rgb[0] = std::isfinite(pkt->color_rgb[0]) ? std::clamp(pkt->color_rgb[0], 0.0f, 1.0f) : 0.2f;
        player.color_rgb[1] = std::isfinite(pkt->color_rgb[1]) ? std::clamp(pkt->color_rgb[1], 0.0f, 1.0f) : 0.85f;
        player.color_rgb[2] = std::isfinite(pkt->color_rgb[2]) ? std::clamp(pkt->color_rgb[2], 0.0f, 1.0f) : 1.0f;
        player.voice_level = std::max(std::clamp(pkt->voice_level, 0.0f, 1.0f),
                                      voice_mgr_ ? voice_mgr_->GetPeerVoiceLevel(authenticated_sender_id) : 0.0f);

        PlayerPoseSnapshot snap;
        snap.viewer_scale = std::clamp(pkt->viewer_scale, 1e-3, 1e9);
        snap.hmd_pos_kh = pkt->hmd_pos_kh;
        snap.hmd_rot_kh = pkt->hmd_rot_kh.Normalized();
        for (int c = 0; c < 2; ++c) {
            bool valid_ctrl = (pkt->ctrl_active[c] != 0) &&
                std::isfinite(pkt->ctrl_pos_kh[c].x) && std::isfinite(pkt->ctrl_pos_kh[c].y) && std::isfinite(pkt->ctrl_pos_kh[c].z) &&
                std::isfinite(pkt->ctrl_rot_kh[c].x) && std::isfinite(pkt->ctrl_rot_kh[c].y) && std::isfinite(pkt->ctrl_rot_kh[c].z) && std::isfinite(pkt->ctrl_rot_kh[c].w);
            snap.ctrl_active[c] = valid_ctrl;
            snap.ctrl_pos_kh[c] = valid_ctrl ? pkt->ctrl_pos_kh[c] : pkt->hmd_pos_kh;
            snap.ctrl_rot_kh[c] = valid_ctrl ? pkt->ctrl_rot_kh[c].Normalized() : snap.hmd_rot_kh;
        }
        snap.timestamp_ms = now;

        if (is_new) {
            player.prev_snap = snap;
            player.target_snap = snap;
            player.interp_snap = snap;
        } else {
            player.prev_snap = player.interp_snap;
            player.target_snap = snap;
        }
    } else if (hdr->type == kPacketVoice && len >= sizeof(PacketHeader) + 3) {
        const auto* pkt = reinterpret_cast<const VoicePacket*>(data);
        if (pkt->codec == 0 && voice_mgr_ &&
            pkt->payload_len == EarthVoice::kAdpcmPayloadBytes &&
            len >= sizeof(PacketHeader) + 3 + pkt->payload_len) {
            voice_mgr_->OnRemoteAdpcmFrame(authenticated_sender_id, pkt->payload, pkt->payload_len);
        }
    }
}

void NetworkManager::UpdateSimulatedBot(
    const EarthMath::Vec3d& local_hmd_kh_pos,
    const EarthMath::Quatd& local_hmd_kh_rot,
    double local_viewer_scale) {

    int mode = test_bot_mode_.load();
    if (mode <= 0) return;

    uint64_t now = GetTickCount64();
    double t = static_cast<double>(now) * 0.001;

    simulated_bot_.peer_id = 0xB070000000000001ULL;
    simulated_bot_.is_simulated_bot = true;
    simulated_bot_.last_seen_ms = now;
    simulated_bot_.color_rgb[0] = 1.0f;
    simulated_bot_.color_rgb[1] = 0.45f;
    simulated_bot_.color_rgb[2] = 0.15f;
    float sim_voice = static_cast<float>(std::sin(t * 3.0) * std::sin(t * 0.7));
    simulated_bot_.voice_level = (sim_voice > 0.35f) ? (sim_voice - 0.35f) * 1.5f : 0.0f;

    PlayerPoseSnapshot& snap = simulated_bot_.interp_snap;
    snap.timestamp_ms = now;
    snap.ctrl_active[0] = true;
    snap.ctrl_active[1] = true;

    EarthMath::Vec3d up = local_hmd_kh_pos.Normalized();
    if (mode == 1) {
        simulated_bot_.name = "Atlas [Opposite Side Giant]";
        EarthMath::Vec3d anti_up = up * -1.0;
        snap.viewer_scale = 2500000.0;
        snap.hmd_pos_kh = anti_up * (EarthMath::kEarthRadiusMeters + 1800000.0);

        EarthMath::Quatd nod{std::sin(t * 1.5) * 0.12, std::cos(t * 1.1) * 0.15, 0.0, 1.0};
        snap.hmd_rot_kh = (local_hmd_kh_rot * nod).Normalized();

        EarthMath::Vec3d right = snap.hmd_rot_kh.Rotate({1.0, 0.0, 0.0});
        EarthMath::Vec3d fwd   = snap.hmd_rot_kh.Rotate({0.0, 0.0, -1.0});
        EarthMath::Vec3d down  = snap.hmd_rot_kh.Rotate({0.0, -1.0, 0.0});
        double wave = std::sin(t * 2.8) * 0.25;
        snap.ctrl_pos_kh[0] = snap.hmd_pos_kh + (right * -0.35 + fwd * 0.45 + down * (0.25 + wave)) * snap.viewer_scale;
        snap.ctrl_pos_kh[1] = snap.hmd_pos_kh + (right *  0.35 + fwd * 0.45 + down * (0.25 - wave)) * snap.viewer_scale;
        snap.ctrl_rot_kh[0] = snap.hmd_rot_kh;
        snap.ctrl_rot_kh[1] = snap.hmd_rot_kh;
    } else if (mode == 2) {
        simulated_bot_.name = "Echo [Same-Scale Friend]";
        snap.viewer_scale = local_viewer_scale;
        EarthMath::Vec3d fwd = local_hmd_kh_rot.Rotate({0.0, 0.0, -1.0});
        EarthMath::Vec3d right = local_hmd_kh_rot.Rotate({1.0, 0.0, 0.0});
        EarthMath::Vec3d head_up = local_hmd_kh_rot.Rotate({0.0, 1.0, 0.0});

        snap.hmd_pos_kh = local_hmd_kh_pos + fwd * (2.2 * local_viewer_scale);
        EarthMath::Quatd turn180{0.0, 1.0, 0.0, 0.0};
        EarthMath::Quatd tilt{std::sin(t * 2.0) * 0.08, std::cos(t * 1.4) * 0.12, 0.0, 1.0};
        snap.hmd_rot_kh = (local_hmd_kh_rot * turn180 * tilt.Normalized()).Normalized();

        double wave_l = std::sin(t * 3.2) * 0.18;
        double wave_r = std::cos(t * 2.7) * 0.18;
        snap.ctrl_pos_kh[0] = snap.hmd_pos_kh + (right *  0.32 - fwd * 0.35 - head_up * (0.22 - wave_l)) * snap.viewer_scale;
        snap.ctrl_pos_kh[1] = snap.hmd_pos_kh + (right * -0.32 - fwd * 0.35 - head_up * (0.22 - wave_r)) * snap.viewer_scale;
        snap.ctrl_rot_kh[0] = snap.hmd_rot_kh;
        snap.ctrl_rot_kh[1] = snap.hmd_rot_kh;
    } else if (mode == 3) {
        simulated_bot_.name = "Nova [Human Scale in Paris]";
        snap.viewer_scale = 1.0;
        snap.hmd_pos_kh = EarthMath::LatLonAltToKeyhole(48.8584, 2.2945, 320.0);
        snap.hmd_rot_kh = local_hmd_kh_rot;
        EarthMath::Vec3d right = snap.hmd_rot_kh.Rotate({1.0, 0.0, 0.0});
        EarthMath::Vec3d fwd   = snap.hmd_rot_kh.Rotate({0.0, 0.0, -1.0});
        EarthMath::Vec3d down  = snap.hmd_rot_kh.Rotate({0.0, -1.0, 0.0});
        snap.ctrl_pos_kh[0] = snap.hmd_pos_kh + (right * -0.30 + fwd * 0.40 + down * 0.25);
        snap.ctrl_pos_kh[1] = snap.hmd_pos_kh + (right *  0.30 + fwd * 0.40 + down * 0.25);
        snap.ctrl_rot_kh[0] = snap.hmd_rot_kh;
        snap.ctrl_rot_kh[1] = snap.hmd_rot_kh;
    }
}

void NetworkManager::UpdateLocalPoseAndPoll(
    const EarthMath::Sim3d& start_from_keyhole,
    const EarthMath::Vec3d& local_hmd_start_pos,
    const EarthMath::Quatd& local_hmd_start_rot,
    const EarthMath::Vec3d local_ctrl_start_pos[2],
    const EarthMath::Quatd local_ctrl_start_rot[2],
    const bool local_ctrl_active[2]) {

    if (!initialized_.load()) return;

    PollSteamCallbacksAndPackets();
    PollUdpPackets();

    uint64_t now = GetTickCount64();
    if (session_state_.load() == SessionState::Joining && now - join_start_ms_ > 8000) {
        CloseServerOrLeave();
        SetStatusMessage("JOIN TIMED OUT");
    }

    if (!start_from_keyhole.IsValid()) return;

    double local_viewer_scale = start_from_keyhole.GetViewerScale();
    EarthMath::Vec3d hmd_kh_pos = start_from_keyhole.TransformPointFromStartToKeyhole(local_hmd_start_pos);
    EarthMath::Quatd hmd_kh_rot = start_from_keyhole.TransformRotFromStartToKeyhole(local_hmd_start_rot);

    UpdateSimulatedBot(hmd_kh_pos, hmd_kh_rot, local_viewer_scale);

    SessionState st = session_state_.load();
    if ((st == SessionState::Hosting || st == SessionState::InSession) && now - last_pose_send_ms_ >= 20) {
        last_pose_send_ms_ = now;

        PosePacket pkt = {};
        pkt.hdr.type = kPacketPose;
        pkt.hdr.sender_id = local_peer_id_;
        pkt.hdr.session_token = session_token_.load();
        pkt.hdr.seq = ++pose_seq_;
        std::snprintf(pkt.player_name, sizeof(pkt.player_name), "%s", local_player_name_.c_str());
        pkt.viewer_scale = local_viewer_scale;
        pkt.hmd_pos_kh = hmd_kh_pos;
        pkt.hmd_rot_kh = hmd_kh_rot;

        for (int c = 0; c < 2; ++c) {
            pkt.ctrl_active[c] = local_ctrl_active[c] ? 1 : 0;
            pkt.ctrl_pos_kh[c] = start_from_keyhole.TransformPointFromStartToKeyhole(local_ctrl_start_pos[c]);
            pkt.ctrl_rot_kh[c] = start_from_keyhole.TransformRotFromStartToKeyhole(local_ctrl_start_rot[c]);
        }

        pkt.voice_level = voice_mgr_ ? voice_mgr_->GetLocalVoiceLevel() : 0.0f;
        pkt.color_rgb[0] = local_color_rgb_[0];
        pkt.color_rgb[1] = local_color_rgb_[1];
        pkt.color_rgb[2] = local_color_rgb_[2];

        BroadcastPoseAndPeerList(pkt);
    }
}

std::vector<RemotePlayerState> NetworkManager::GetActiveRemotePlayers() {
    std::vector<RemotePlayerState> out;
    uint64_t now = GetTickCount64();

    {
        std::lock_guard<std::mutex> lock(players_mutex_);
        for (auto it = remote_players_.begin(); it != remote_players_.end(); ) {
            if (now - it->second.last_seen_ms > 10000) {
                it = remote_players_.erase(it);
                continue;
            }
            auto& p = it->second;
            double dt_ms = static_cast<double>(now - p.target_snap.timestamp_ms);
            double alpha = std::clamp(dt_ms / 25.0, 0.0, 1.0);

            p.interp_snap.hmd_pos_kh = p.prev_snap.hmd_pos_kh + (p.target_snap.hmd_pos_kh - p.prev_snap.hmd_pos_kh) * alpha;
            p.interp_snap.hmd_rot_kh = EarthMath::Quatd::Slerp(p.prev_snap.hmd_rot_kh, p.target_snap.hmd_rot_kh, alpha);

            double log_s0 = std::log(std::max(p.prev_snap.viewer_scale, 1e-4));
            double log_s1 = std::log(std::max(p.target_snap.viewer_scale, 1e-4));
            p.interp_snap.viewer_scale = std::exp(log_s0 + (log_s1 - log_s0) * alpha);

            for (int c = 0; c < 2; ++c) {
                p.interp_snap.ctrl_active[c] = p.target_snap.ctrl_active[c];
                p.interp_snap.ctrl_pos_kh[c] = p.prev_snap.ctrl_pos_kh[c] + (p.target_snap.ctrl_pos_kh[c] - p.prev_snap.ctrl_pos_kh[c]) * alpha;
                p.interp_snap.ctrl_rot_kh[c] = EarthMath::Quatd::Slerp(p.prev_snap.ctrl_rot_kh[c], p.target_snap.ctrl_rot_kh[c], alpha);
            }

            if (voice_mgr_) {
                float v = voice_mgr_->GetPeerVoiceLevel(p.peer_id);
                if (v > p.voice_level) p.voice_level = v;
                else p.voice_level *= 0.9f;
            }

            out.push_back(p);
            ++it;
        }
    }

    std::sort(out.begin(), out.end(), [](const RemotePlayerState& a, const RemotePlayerState& b) {
        return a.peer_id < b.peer_id;
    });

    if (test_bot_mode_.load() > 0 && simulated_bot_.peer_id != 0) {
        out.push_back(simulated_bot_);
    }

    return out;
}

size_t NetworkManager::GetConnectedPeerCount() {
    std::lock_guard<std::mutex> lock(players_mutex_);
    return remote_players_.size();
}

} // namespace EarthNet
