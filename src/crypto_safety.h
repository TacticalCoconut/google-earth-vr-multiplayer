#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <unordered_map>

#pragma comment(lib, "bcrypt.lib")

namespace EarthSecurity {

// Generates cryptographically secure random bytes using Windows CNG (BCryptGenRandom)
inline bool GenerateRandomBytes(void* out_buf, size_t len) {
    if (!out_buf || len == 0) return false;
    NTSTATUS status = BCryptGenRandom(
        nullptr,
        reinterpret_cast<PUCHAR>(out_buf),
        static_cast<ULONG>(len),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    return status >= 0;
}

inline uint64_t GenerateRandomToken64() {
    uint64_t token = 0;
    if (!GenerateRandomBytes(&token, sizeof(token)) || token == 0) {
        LARGE_INTEGER qpc = {};
        QueryPerformanceCounter(&qpc);
        token = (static_cast<uint64_t>(GetTickCount64()) << 32) ^ static_cast<uint64_t>(qpc.QuadPart) ^ 0x9E3779B97F4A7C15ULL;
    }
    return token;
}

inline std::string GenerateRandomSaltHex(size_t num_bytes = 16) {
    uint8_t buf[32] = {};
    if (num_bytes > sizeof(buf)) num_bytes = sizeof(buf);
    GenerateRandomBytes(buf, num_bytes);
    char hex[65] = {};
    for (size_t i = 0; i < num_bytes; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", buf[i]);
    }
    return std::string(hex);
}

// Computes SHA-256 digest via Windows CNG (BCrypt)
inline bool ComputeSha256(const void* data, size_t len, uint8_t out_hash[32]) {
    std::memset(out_hash, 0, 32);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return false;
    }
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0) {
        if (BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(len), 0) >= 0) {
            if (BCryptFinishHash(hHash, out_hash, 32, 0) >= 0) {
                ok = true;
            }
        }
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// Computes salted, peer-bound SHA-256 join proof:
// SHA256("EVRMP_AUTH_V2|" + salt_hex + "|" + password + "|" + client_steam_id + "|" + lobby_id)
inline void ComputeJoinAuthHash(
    const std::string& salt_hex,
    const std::string& password,
    uint64_t client_steam_id,
    uint64_t lobby_id,
    uint8_t out_hash[32]) {

    char buf[512] = {};
    int n = std::snprintf(
        buf, sizeof(buf),
        "EVRMP_AUTH_V2|%s|%s|%llu|%llu",
        salt_hex.c_str(),
        password.c_str(),
        static_cast<unsigned long long>(client_steam_id),
        static_cast<unsigned long long>(lobby_id)
    );
    if (n < 0) n = 0;
    if (static_cast<size_t>(n) >= sizeof(buf)) n = sizeof(buf) - 1;
    ComputeSha256(buf, static_cast<size_t>(n), out_hash);
}

// Constant-time memory comparison to prevent timing side-channel attacks on password hashes
inline bool ConstantTimeEquals(const void* a, const void* b, size_t len) {
    const volatile uint8_t* pa = reinterpret_cast<const volatile uint8_t*>(a);
    const volatile uint8_t* pb = reinterpret_cast<const volatile uint8_t*>(b);
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= (pa[i] ^ pb[i]);
    }
    return diff == 0;
}

// Strictly sanitizes untrusted network strings to printable ASCII [32..126]
inline std::string SanitizeDisplayString(const char* raw_buf, size_t max_len, const char* fallback = "Player") {
    if (!raw_buf || max_len == 0) return fallback;
    std::string out;
    out.reserve(max_len);
    for (size_t i = 0; i < max_len && raw_buf[i] != '\0'; ++i) {
        unsigned char c = static_cast<unsigned char>(raw_buf[i]);
        if (c >= 32 && c <= 126) {
            out.push_back(static_cast<char>(c));
        }
    }
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.empty()) return fallback;
    return out;
}

// Per-peer sliding window rate limiter to protect against packet flooding / DoS
class PeerRateLimiter {
public:
    bool AllowPacket(uint64_t peer_id, uint64_t now_ms, uint32_t max_per_sec) {
        auto& entry = counters_[peer_id];
        if (now_ms - entry.window_start_ms >= 1000) {
            entry.window_start_ms = now_ms;
            entry.count = 1;
            return true;
        }
        if (entry.count >= max_per_sec) {
            return false;
        }
        ++entry.count;
        return true;
    }

    void RemovePeer(uint64_t peer_id) {
        counters_.erase(peer_id);
    }

    void Clear() {
        counters_.clear();
    }

private:
    struct Counter {
        uint64_t window_start_ms = 0;
        uint32_t count = 0;
    };
    std::unordered_map<uint64_t, Counter> counters_;
};

} // namespace EarthSecurity
