#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace EarthVoice {

constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kFrameSamples = 320; // 20 ms per packet at 16 kHz
constexpr uint32_t kAdpcmPayloadBytes = 4 + (kFrameSamples / 2); // 164 bytes

class GlobalVoiceManager {
public:
    using SendVoiceCallback = std::function<void(const uint8_t* data, size_t len)>;

    GlobalVoiceManager();
    ~GlobalVoiceManager();

    bool Initialize(SendVoiceCallback on_encoded_frame);
    void Shutdown();

    // Push incoming IMA-ADPCM compressed voice frame from a remote peer
    void OnRemoteAdpcmFrame(uint64_t peer_id, const uint8_t* data, size_t len);

    // Push incoming raw 16-bit 16kHz PCM samples (e.g., from Steam DecompressVoice)
    void OnRemotePcmSamples(uint64_t peer_id, const int16_t* samples, size_t sample_count);

    // Mute / unmute local microphone
    void SetMicMuted(bool muted) { mic_muted_.store(muted); }
    bool IsMicMuted() const { return mic_muted_.load(); }
    void ToggleMicMuted() { mic_muted_.store(!mic_muted_.load()); }

    // Global playback volume (1.0f = 100%)
    void SetPlaybackVolume(float vol) { playback_volume_.store(vol); }
    float GetPlaybackVolume() const { return playback_volume_.load(); }

    // Voice activity level [0.0 .. 1.0] for visual indicator
    float GetLocalVoiceLevel() const { return local_voice_level_.load(); }
    float GetPeerVoiceLevel(uint64_t peer_id);

    // Encode/Decode IMA-ADPCM helpers
    static void EncodeAdpcm(const int16_t* pcm, size_t sample_count, uint8_t* out_adpcm);
    static void DecodeAdpcm(const uint8_t* adpcm, size_t adpcm_bytes, int16_t* out_pcm, size_t max_samples);

private:
    static constexpr int kNumWaveBuffers = 8;

    struct PeerAudioStream {
        std::vector<int16_t> ring_buffer;
        size_t read_pos = 0;
        size_t write_pos = 0;
        size_t available = 0;
        float current_level = 0.0f;
        uint64_t last_packet_tick = 0;
        bool priming = true;

        PeerAudioStream() : ring_buffer(kSampleRate * 2, 0) {} // 2-second ring buffer
    };

    void AudioWorkerLoop();
    void MixOutputBuffer(int16_t* out_pcm, size_t sample_count);

    std::atomic<bool> running_{false};
    std::atomic<bool> mic_muted_{false};
    std::atomic<float> playback_volume_{1.0f};
    std::atomic<float> local_voice_level_{0.0f};

    SendVoiceCallback send_callback_;
    HANDLE worker_thread_ = nullptr;
    HANDLE audio_event_ = nullptr;

    HWAVEIN wave_in_ = nullptr;
    HWAVEOUT wave_out_ = nullptr;
    WAVEHDR in_headers_[kNumWaveBuffers] = {};
    WAVEHDR out_headers_[kNumWaveBuffers] = {};
    int16_t in_buffers_[kNumWaveBuffers][kFrameSamples] = {};
    int16_t out_buffers_[kNumWaveBuffers][kFrameSamples] = {};

    std::mutex peers_mutex_;
    std::unordered_map<uint64_t, PeerAudioStream> peer_streams_;
};

} // namespace EarthVoice
