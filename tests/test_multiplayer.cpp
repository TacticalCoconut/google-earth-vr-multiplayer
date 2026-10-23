#include <cstdio>
#include <cmath>
#include <cassert>
#include <cstring>
#include "earth_math.h"
#include "voice.h"
#include "network.h"
#include "crypto_safety.h"

int main() {
    std::printf("=== Google Earth VR Multiplayer & Security Verification Suite ===\n");

    // 1. Test Keyhole <-> LatLonAlt conversion (matching Earth.exe 0x2ad5d0)
    {
        double lat_in = 48.8584, lon_in = 2.2945, alt_in = 320.0; // Eiffel Tower, Paris
        EarthMath::Vec3d kh = EarthMath::LatLonAltToKeyhole(lat_in, lon_in, alt_in);
        double lat_out = 0, lon_out = 0, alt_out = 0;
        EarthMath::KeyholeToLatLonAlt(kh, lat_out, lon_out, alt_out);

        double err_lat = std::fabs(lat_in - lat_out);
        double err_lon = std::fabs(lon_in - lon_out);
        double err_alt = std::fabs(alt_in - alt_out);
        std::printf("[Test 1] Paris Lat/Lon/Alt -> Keyhole -> Lat/Lon/Alt: err=(%.2e deg, %.2e deg, %.2e m)\n",
                    err_lat, err_lon, err_alt);
        assert(err_lat < 1e-7 && err_lon < 1e-7 && err_alt < 1e-3);
    }

    // 2. Test Cross-Player Scale & Antipodal Transformation (Human in New York vs Giant in Australia)
    {
        EarthMath::Vec3d ny_kh = EarthMath::LatLonAltToKeyhole(40.7128, -74.0060, 2.0);
        EarthMath::Sim3d sim_a;
        sim_a.scale = 1.0;
        sim_a.rotation = EarthMath::Quatd(0.1, 0.2, 0.3, 0.9).Normalized();
        EarthMath::Vec3d local_hmd_start_a{0.0, 1.7, 0.0};
        sim_a.translation = local_hmd_start_a - sim_a.rotation.Rotate(ny_kh) * sim_a.scale;

        EarthMath::Vec3d ny_kh_recovered = sim_a.TransformPointFromStartToKeyhole(local_hmd_start_a);
        double rt_err = (ny_kh_recovered - ny_kh).Length();
        std::printf("[Test 2a] Player A Start->Keyhole roundtrip error at 6,371 km radius: %.6e meters\n", rt_err);
        assert(rt_err < 1e-6);

        EarthMath::Vec3d antipode_kh = ny_kh * -1.0;
        double viewer_scale_b = 1000000.0;

        EarthMath::Vec3d b_in_a_start = sim_a.TransformPointFromKeyholeToStart(antipode_kh);
        double dist_in_a_start = (b_in_a_start - local_hmd_start_a).Length();
        double rel_scale_b_in_a = sim_a.scale * viewer_scale_b;
        bool occluded = EarthMath::IsOccludedByEarthSphere(ny_kh, antipode_kh);

        std::printf("[Test 2b] Antipodal Giant in Player A Start Space: dist=%.1f km, relative_scale=%.1fx, occluded_by_earth=%s\n",
                    dist_in_a_start * 0.001, rel_scale_b_in_a, occluded ? "true" : "false");
        assert(std::fabs(dist_in_a_start - 2.0 * (EarthMath::kEarthRadiusMeters + 2.0)) < 1.0);
        assert(std::fabs(rel_scale_b_in_a - 1000000.0) < 1e-3);
        assert(occluded == true);
    }

    // 3. Test IMA-ADPCM 16kHz Global Voice Codec roundtrip
    {
        int16_t pcm_in[EarthVoice::kFrameSamples];
        for (size_t i = 0; i < EarthVoice::kFrameSamples; ++i) {
            double t = static_cast<double>(i) / EarthVoice::kSampleRate;
            pcm_in[i] = static_cast<int16_t>(std::sin(2.0 * 3.14159265 * 440.0 * t) * 12000.0);
        }
        uint8_t encoded[EarthVoice::kAdpcmPayloadBytes] = {};
        int16_t pcm_out[EarthVoice::kFrameSamples] = {};

        EarthVoice::GlobalVoiceManager::EncodeAdpcm(pcm_in, EarthVoice::kFrameSamples, encoded);
        EarthVoice::GlobalVoiceManager::DecodeAdpcm(encoded, EarthVoice::kAdpcmPayloadBytes, pcm_out, EarthVoice::kFrameSamples);

        double signal_power = 0.0, noise_power = 0.0;
        for (size_t i = 16; i < EarthVoice::kFrameSamples; ++i) {
            double s = pcm_in[i];
            double n = pcm_in[i] - pcm_out[i];
            signal_power += s * s;
            noise_power += n * n;
        }
        double snr_db = 10.0 * std::log10(signal_power / (noise_power + 1.0));
        std::printf("[Test 3] IMA-ADPCM 16kHz Voice Codec (320 samples -> %u bytes): SNR = %.2f dB\n",
                    EarthVoice::kAdpcmPayloadBytes, snr_db);
        assert(snr_db > 18.0);
    }

    // 4. Test Authenticated Session Management, Salted SHA-256 Password Auth, Lock, Kick/Ban & Anti-Spoofing
    {
        EarthNet::NetworkManager net;
        net.Initialize(nullptr, ".");

        // Verify mod starts Offline (no automatic background lobby join)
        assert(net.GetSessionState() == EarthNet::SessionState::Offline);

        EarthMath::Sim3d dummy_sim;
        dummy_sim.scale = 1.0;
        dummy_sim.rotation = {0, 0, 0, 1};
        dummy_sim.translation = {0, 0, 0};
        EarthMath::Vec3d ctrl_p[2] = {};
        EarthMath::Quatd ctrl_q[2] = {};
        bool ctrl_a[2] = {true, true};

        constexpr uint64_t kPeerFriend   = 0x1111222233334444ULL;
        constexpr uint64_t kPeerAttacker = 0x9999888877776666ULL;
        constexpr uint64_t kPeerLate     = 0x5555666677778888ULL;

        // 4a. Start a password-protected server
        bool started = net.StartServer("GALAXY99");
        assert(started && net.GetSessionState() == EarthNet::SessionState::Hosting);
        assert(net.IsHost() && net.HasServerPassword() && !net.IsSessionLocked());

        // 4b. Verify unauthenticated PosePacket or Wrong-Password JoinRequest is rejected
        EarthNet::JoinRequestPacket bad_req = {};
        bad_req.hdr.type = EarthNet::kPacketJoinRequest;
        bad_req.hdr.sender_id = kPeerAttacker;
        bad_req.lobby_id = net.GetCurrentLobbyId();
        std::snprintf(bad_req.player_name, sizeof(bad_req.player_name), "Attacker");
        EarthSecurity::ComputeJoinAuthHash(net.GetCurrentPasswordSalt(), "WRONG_PASS",
                                           kPeerAttacker, net.GetCurrentLobbyId(), bad_req.auth_hash);
        net.InjectTestPacket(kPeerAttacker, &bad_req, sizeof(bad_req));

        EarthNet::PosePacket atk_pose = {};
        atk_pose.hdr.type = EarthNet::kPacketPose;
        atk_pose.hdr.sender_id = kPeerAttacker;
        atk_pose.hdr.session_token = net.GetCurrentSessionToken();
        atk_pose.hdr.seq = 1;
        std::snprintf(atk_pose.player_name, sizeof(atk_pose.player_name), "Attacker");
        atk_pose.viewer_scale = 1.0;
        atk_pose.hmd_pos_kh = EarthMath::LatLonAltToKeyhole(0.0, 0.0, 10.0);
        atk_pose.hmd_rot_kh = {0, 0, 0, 1};
        net.InjectTestPacket(kPeerAttacker, &atk_pose, sizeof(atk_pose));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().empty());
        std::printf("[Test 4a] Wrong-password & unauthenticated peer packets rejected: OK\n");

        // 4c. Authenticate legitimate friend with salted SHA-256 password hash
        EarthNet::JoinRequestPacket good_req = {};
        good_req.hdr.type = EarthNet::kPacketJoinRequest;
        good_req.hdr.sender_id = kPeerFriend;
        good_req.lobby_id = net.GetCurrentLobbyId();
        std::snprintf(good_req.player_name, sizeof(good_req.player_name), "TestFriendVR");
        EarthSecurity::ComputeJoinAuthHash(net.GetCurrentPasswordSalt(), "GALAXY99",
                                           kPeerFriend, net.GetCurrentLobbyId(), good_req.auth_hash);
        net.InjectTestPacket(kPeerFriend, &good_req, sizeof(good_req));

        // Verify anti-spoofing: attacker pretending to be kPeerFriend is dropped
        EarthNet::PosePacket spoof_pkt = atk_pose;
        spoof_pkt.hdr.sender_id = kPeerFriend; // Claiming to be kPeerFriend
        net.InjectTestPacket(kPeerAttacker, &spoof_pkt, sizeof(spoof_pkt)); // Transport says kPeerAttacker
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().empty());
        std::printf("[Test 4b] Transport sender_id anti-spoofing check verified: OK\n");

        // Send legitimate authenticated PosePacket from kPeerFriend
        EarthNet::PosePacket pkt = {};
        pkt.hdr.type = EarthNet::kPacketPose;
        pkt.hdr.sender_id = kPeerFriend;
        pkt.hdr.session_token = net.GetCurrentSessionToken();
        pkt.hdr.seq = 1;
        std::snprintf(pkt.player_name, sizeof(pkt.player_name), "TestFriendVR");
        pkt.viewer_scale = 50000.0;
        pkt.hmd_pos_kh = EarthMath::LatLonAltToKeyhole(-33.8568, 151.2153, 500.0);
        pkt.hmd_rot_kh = EarthMath::Quatd(0.0, 0.70710678, 0.0, 0.70710678).Normalized();
        pkt.ctrl_active[0] = 1;
        pkt.ctrl_active[1] = 1;
        pkt.ctrl_pos_kh[0] = pkt.hmd_pos_kh + EarthMath::Vec3d(-15000.0, -10000.0, -20000.0);
        pkt.ctrl_pos_kh[1] = pkt.hmd_pos_kh + EarthMath::Vec3d( 15000.0, -10000.0, -20000.0);
        pkt.ctrl_rot_kh[0] = pkt.hmd_rot_kh;
        pkt.ctrl_rot_kh[1] = pkt.hmd_rot_kh;
        pkt.voice_level = 0.75f;

        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);

        auto remotes = net.GetActiveRemotePlayers();
        assert(remotes.size() == 1);
        const auto& friend_state = remotes[0];
        double pos_err = (friend_state.interp_snap.hmd_pos_kh - pkt.hmd_pos_kh).Length();
        std::printf("[Test 4c] Authenticated Pose Sync: Peer='%s', Scale=%.1fx, PosErr=%.2e m, Voice=%.2f\n",
                    friend_state.name.c_str(), friend_state.interp_snap.viewer_scale, pos_err, friend_state.voice_level);
        assert(friend_state.name == "TestFriendVR");
        assert(std::fabs(friend_state.interp_snap.viewer_scale - 50000.0) < 1.0);
        assert(pos_err < 1e-6);

        // 4d. Test Session Locking (prevents any new player from joining even with correct password)
        net.SetSessionLocked(true);
        assert(net.IsSessionLocked());

        EarthNet::JoinRequestPacket late_req = {};
        late_req.hdr.type = EarthNet::kPacketJoinRequest;
        late_req.hdr.sender_id = kPeerLate;
        late_req.lobby_id = net.GetCurrentLobbyId();
        std::snprintf(late_req.player_name, sizeof(late_req.player_name), "LateJoiner");
        EarthSecurity::ComputeJoinAuthHash(net.GetCurrentPasswordSalt(), "GALAXY99",
                                           kPeerLate, net.GetCurrentLobbyId(), late_req.auth_hash);
        net.InjectTestPacket(kPeerLate, &late_req, sizeof(late_req));

        EarthNet::PosePacket late_pose = pkt;
        late_pose.hdr.sender_id = kPeerLate;
        net.InjectTestPacket(kPeerLate, &late_pose, sizeof(late_pose));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().size() == 1);
        std::printf("[Test 4d] Session Lock enforcement (new joins blocked while locked): OK\n");

        // 4e. Test Kicking a Player (removes from current session, but does NOT ban from rejoining or future sessions)
        net.SetSessionLocked(false);
        net.KickPlayer(kPeerFriend);
        assert(net.GetActiveRemotePlayers().empty());
        assert(!net.IsPlayerBanned(kPeerFriend));

        // Verify kicked player's old in-flight packets are dropped until they re-join
        pkt.hdr.seq = 2;
        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().empty());

        // Verify kicked player CAN rejoin because Kick != Ban
        net.InjectTestPacket(kPeerFriend, &good_req, sizeof(good_req));
        pkt.hdr.seq = 3;
        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().size() == 1);
        std::printf("[Test 4e] Kick enforcement (disconnects immediately, allows rejoining / future sessions): OK\n");

        // 4f. Test Separate Ban & Unban Functions (persists across future sessions until unbanned)
        net.BanPlayer(kPeerFriend, "TestFriendVR");
        assert(net.IsPlayerBanned(kPeerFriend));
        assert(net.GetBannedPlayers().size() == 1);
        assert(net.GetActiveRemotePlayers().empty());

        // Verify banned player cannot rejoin current session
        net.InjectTestPacket(kPeerFriend, &good_req, sizeof(good_req));
        pkt.hdr.seq = 4;
        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().empty());

        // Start a brand-new future session and verify ban still blocks kPeerFriend
        net.StartServer("GALAXY99");
        EarthNet::JoinRequestPacket future_req = {};
        future_req.hdr.type = EarthNet::kPacketJoinRequest;
        future_req.hdr.sender_id = kPeerFriend;
        future_req.lobby_id = net.GetCurrentLobbyId();
        std::snprintf(future_req.player_name, sizeof(future_req.player_name), "TestFriendVR");
        EarthSecurity::ComputeJoinAuthHash(net.GetCurrentPasswordSalt(), "GALAXY99",
                                           kPeerFriend, net.GetCurrentLobbyId(), future_req.auth_hash);
        net.InjectTestPacket(kPeerFriend, &future_req, sizeof(future_req));
        pkt.hdr.session_token = net.GetCurrentSessionToken();
        pkt.hdr.seq = 5;
        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().empty());

        // Now Unban kPeerFriend and verify they can join again
        net.UnbanPlayer(kPeerFriend);
        assert(!net.IsPlayerBanned(kPeerFriend));
        assert(net.GetBannedPlayers().empty());
        net.InjectTestPacket(kPeerFriend, &future_req, sizeof(future_req));
        pkt.hdr.seq = 6;
        net.InjectTestPacket(kPeerFriend, &pkt, sizeof(pkt));
        net.UpdateLocalPoseAndPoll(dummy_sim, {0, 1.7, 0}, {0, 0, 0, 1}, ctrl_p, ctrl_q, ctrl_a);
        assert(net.GetActiveRemotePlayers().size() == 1);
        std::printf("[Test 4f] Separate Ban & Unban functions verified across sessions: OK\n");

        net.CloseServerOrLeave();
        assert(net.GetSessionState() == EarthNet::SessionState::Offline);
        net.Shutdown();
    }

    // 5. Test version.dll proxy forwarding on Earth.exe
    {
        HMODULE hVer = LoadLibraryA("build\\version.dll");
        assert(hVer != nullptr);
        using fn_GetSizeW = DWORD (WINAPI*)(LPCWSTR, LPDWORD);
        using fn_GetInfoW = BOOL (WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
        using fn_VerQueryW = BOOL (WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

        auto pGetSizeW = reinterpret_cast<fn_GetSizeW>(GetProcAddress(hVer, "GetFileVersionInfoSizeW"));
        auto pGetInfoW = reinterpret_cast<fn_GetInfoW>(GetProcAddress(hVer, "GetFileVersionInfoW"));
        auto pVerQueryW = reinterpret_cast<fn_VerQueryW>(GetProcAddress(hVer, "VerQueryValueW"));
        assert(pGetSizeW && pGetInfoW && pVerQueryW);

        DWORD handle = 0;
        LPCWSTR earth_exe = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\EarthVR\\Earth.exe";
        if (GetFileAttributesW(earth_exe) == INVALID_FILE_ATTRIBUTES) {
            earth_exe = L"C:\\Windows\\System32\\kernel32.dll";
        }
        DWORD sz = pGetSizeW(earth_exe, &handle);
        assert(sz > 0);
        std::vector<uint8_t> ver_buf(sz);
        BOOL ok = pGetInfoW(earth_exe, 0, sz, ver_buf.data());
        assert(ok == TRUE);
        VS_FIXEDFILEINFO* ffi = nullptr;
        UINT ffi_len = 0;
        ok = pVerQueryW(ver_buf.data(), L"\\", reinterpret_cast<LPVOID*>(&ffi), &ffi_len);
        assert(ok == TRUE && ffi != nullptr && ffi->dwSignature == 0xFEEF04BD);
        std::printf("[Test 5] Proxy version.dll forwarding on Earth.exe: Version=%u.%u.%u.%u (%lu bytes)\n",
                    HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                    HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS), sz);
        FreeLibrary(hVer);
    }

    std::printf("=== ALL TESTS PASSED SUCCESSFULLY ===\n");
    return 0;
}
