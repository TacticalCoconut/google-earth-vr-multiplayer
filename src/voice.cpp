#include "voice.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

namespace EarthVoice {

namespace {

const int kIndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int kStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

inline uint8_t EncodeNibble(int16_t sample, int& predictor, int& step_index) {
    int step = kStepTable[step_index];
    int diff = sample - predictor;
    uint8_t nibble = 0;
    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }
    int vpdiff = (step >> 3);
    if (diff >= step) {
        nibble |= 4;
        diff -= step;
        vpdiff += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 2;
        diff -= step;
        vpdiff += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 1;
        vpdiff += step;
    }
    if (nibble & 8) {
        predictor -= vpdiff;
    } else {
        predictor += vpdiff;
    }
    predictor = std::clamp(predictor, -32768, 32767);
    step_index = std::clamp(step_index + kIndexTable[nibble], 0, 88);
    return nibble & 0x0F;
}

inline int16_t DecodeNibble(uint8_t nibble, int& predictor, int& step_index) {
    int step = kStepTable[step_index];
    int vpdiff = (step >> 3);
    if (nibble & 4) vpdiff += step;
    if (nibble & 2) vpdiff += (step >> 1);
    if (nibble & 1) vpdiff += (step >> 2);
    if (nibble & 8) {
        predictor -= vpdiff;
    } else {
        predictor += vpdiff;
    }
    predictor = std::clamp(predictor, -32768, 32767);
    step_index = std::clamp(step_index + kIndexTable[nibble & 0x0F], 0, 88);
    return static_cast<int16_t>(predictor);
}

float ComputeRmsLevel(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;
    double sum_sq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double v = static_cast<double>(samples[i]) / 32768.0;
        sum_sq += v * v;
    }
    float rms = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(count)));
    // Map typical voice RMS (0.015 .. 0.25) to [0.0 .. 1.0]
    if (rms < 0.012f) return 0.0f;
    return std::clamp((rms - 0.012f) * 6.0f, 0.0f, 1.0f);
}

} // namespace

void GlobalVoiceManager::EncodeAdpcm(const int16_t* pcm, size_t sample_count, uint8_t* out_adpcm) {
    if (!pcm || !out_adpcm || sample_count == 0) return;
    int predictor = pcm[0];
    int step_index = 0;
    int16_t pred16 = static_cast<int16_t>(predictor);
    std::memcpy(out_adpcm, &pred16, 2);
    out_adpcm[2] = static_cast<uint8_t>(step_index);
    out_adpcm[3] = 0;

    uint8_t* dst = out_adpcm + 4;
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
        uint8_t n0 = EncodeNibble(pcm[i], predictor, step_index);
        uint8_t n1 = EncodeNibble(pcm[i + 1], predictor, step_index);
        *dst++ = static_cast<uint8_t>(n0 | (n1 << 4));
    }
}

void GlobalVoiceManager::DecodeAdpcm(const uint8_t* adpcm, size_t adpcm_bytes, int16_t* out_pcm, size_t max_samples) {
    if (!adpcm || !out_pcm || adpcm_bytes <= 4) return;
    int16_t pred16 = 0;
    std::memcpy(&pred16, adpcm, 2);
    int predictor = pred16;
    int step_index = std::clamp(static_cast<int>(adpcm[2]), 0, 88);

    size_t sample_idx = 0;
    for (size_t i = 4; i < adpcm_bytes && sample_idx + 1 < max_samples; ++i) {
        uint8_t byte = adpcm[i];
        out_pcm[sample_idx++] = DecodeNibble(byte & 0x0F, predictor, step_index);
        out_pcm[sample_idx++] = DecodeNibble((byte >> 4) & 0x0F, predictor, step_index);
    }
}

GlobalVoiceManager::GlobalVoiceManager() = default;

GlobalVoiceManager::~GlobalVoiceManager() {
    Shutdown();
}

bool GlobalVoiceManager::Initialize(SendVoiceCallback on_encoded_frame) {
    if (running_.load()) return true;
    send_callback_ = std::move(on_encoded_frame);

    audio_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!audio_event_) return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = kSampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = kSampleRate * 2;

    // Open global audio output (headset speakers)
    MMRESULT res_out = waveOutOpen(&wave_out_, WAVE_MAPPER, &wfx,
                                   reinterpret_cast<DWORD_PTR>(audio_event_), 0, CALLBACK_EVENT);
    if (res_out == MMSYSERR_NOERROR && wave_out_) {
        for (int i = 0; i < kNumWaveBuffers; ++i) {
            std::memset(&out_headers_[i], 0, sizeof(WAVEHDR));
            std::memset(out_buffers_[i], 0, sizeof(out_buffers_[i]));
            out_headers_[i].lpData = reinterpret_cast<LPSTR>(out_buffers_[i]);
            out_headers_[i].dwBufferLength = kFrameSamples * sizeof(int16_t);
            waveOutPrepareHeader(wave_out_, &out_headers_[i], sizeof(WAVEHDR));
            waveOutWrite(wave_out_, &out_headers_[i], sizeof(WAVEHDR));
        }
    }

    // Open microphone input
    MMRESULT res_in = waveInOpen(&wave_in_, WAVE_MAPPER, &wfx,
                                 reinterpret_cast<DWORD_PTR>(audio_event_), 0, CALLBACK_EVENT);
    if (res_in == MMSYSERR_NOERROR && wave_in_) {
        for (int i = 0; i < kNumWaveBuffers; ++i) {
            std::memset(&in_headers_[i], 0, sizeof(WAVEHDR));
            std::memset(in_buffers_[i], 0, sizeof(in_buffers_[i]));
            in_headers_[i].lpData = reinterpret_cast<LPSTR>(in_buffers_[i]);
            in_headers_[i].dwBufferLength = kFrameSamples * sizeof(int16_t);
            waveInPrepareHeader(wave_in_, &in_headers_[i], sizeof(WAVEHDR));
            waveInAddBuffer(wave_in_, &in_headers_[i], sizeof(WAVEHDR));
        }
        waveInStart(wave_in_);
    }

    running_.store(true);
    worker_thread_ = CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        static_cast<GlobalVoiceManager*>(param)->AudioWorkerLoop();
        return 0;
    }, this, 0, nullptr);

    return true;
}

void GlobalVoiceManager::Shutdown() {
    if (!running_.exchange(false)) return;
    if (audio_event_) {
        SetEvent(audio_event_);
    }
    if (worker_thread_) {
        WaitForSingleObject(worker_thread_, 1500);
        CloseHandle(worker_thread_);
        worker_thread_ = nullptr;
    }
    if (wave_in_) {
        waveInStop(wave_in_);
        waveInReset(wave_in_);
        for (int i = 0; i < kNumWaveBuffers; ++i) {
            waveInUnprepareHeader(wave_in_, &in_headers_[i], sizeof(WAVEHDR));
        }
        waveInClose(wave_in_);
        wave_in_ = nullptr;
    }
    if (wave_out_) {
        waveOutReset(wave_out_);
        for (int i = 0; i < kNumWaveBuffers; ++i) {
            waveOutUnprepareHeader(wave_out_, &out_headers_[i], sizeof(WAVEHDR));
        }
        waveOutClose(wave_out_);
        wave_out_ = nullptr;
    }
    if (audio_event_) {
        CloseHandle(audio_event_);
        audio_event_ = nullptr;
    }
}

void GlobalVoiceManager::AudioWorkerLoop() {
    uint8_t adpcm_packet[kAdpcmPayloadBytes];
    int silence_hold_frames = 0;

    while (running_.load()) {
        WaitForSingleObject(audio_event_, 10);
        if (!running_.load()) break;

        // 1. Process captured microphone buffers
        if (wave_in_) {
            for (int i = 0; i < kNumWaveBuffers; ++i) {
                if (in_headers_[i].dwFlags & WHDR_DONE) {
                    if (!mic_muted_.load()) {
                        float level = ComputeRmsLevel(in_buffers_[i], kFrameSamples);
                        float prev = local_voice_level_.load();
                        local_voice_level_.store(level > prev ? level : (prev * 0.8f));

                        if (level > 0.01f) {
                            silence_hold_frames = 12; // 240 ms tail after speaking stops
                        }
                        if (silence_hold_frames > 0) {
                            --silence_hold_frames;
                            if (send_callback_) {
                                EncodeAdpcm(in_buffers_[i], kFrameSamples, adpcm_packet);
                                send_callback_(adpcm_packet, kAdpcmPayloadBytes);
                            }
                        }
                    } else {
                        local_voice_level_.store(0.0f);
                        silence_hold_frames = 0;
                    }
                    in_headers_[i].dwFlags &= ~WHDR_DONE;
                    waveInAddBuffer(wave_in_, &in_headers_[i], sizeof(WAVEHDR));
                }
            }
        }

        // 2. Refill global audio playback buffers
        if (wave_out_) {
            for (int i = 0; i < kNumWaveBuffers; ++i) {
                if (out_headers_[i].dwFlags & WHDR_DONE) {
                    MixOutputBuffer(out_buffers_[i], kFrameSamples);
                    out_headers_[i].dwFlags &= ~WHDR_DONE;
                    waveOutWrite(wave_out_, &out_headers_[i], sizeof(WAVEHDR));
                }
            }
        }
    }
}

void GlobalVoiceManager::OnRemoteAdpcmFrame(uint64_t peer_id, const uint8_t* data, size_t len) {
    if (!data || len < kAdpcmPayloadBytes) return;
    int16_t decoded[kFrameSamples];
    DecodeAdpcm(data, kAdpcmPayloadBytes, decoded, kFrameSamples);
    OnRemotePcmSamples(peer_id, decoded, kFrameSamples);
}

void GlobalVoiceManager::OnRemotePcmSamples(uint64_t peer_id, const int16_t* samples, size_t sample_count) {
    if (!samples || sample_count == 0) return;
    float level = ComputeRmsLevel(samples, sample_count);

    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto& stream = peer_streams_[peer_id];
    stream.current_level = std::max(level, stream.current_level * 0.85f);
    stream.last_packet_tick = GetTickCount64();

    const size_t cap = stream.ring_buffer.size();
    for (size_t i = 0; i < sample_count; ++i) {
        stream.ring_buffer[stream.write_pos] = samples[i];
        stream.write_pos = (stream.write_pos + 1) % cap;
        if (stream.available < cap) {
            stream.available++;
        } else {
            stream.read_pos = (stream.read_pos + 1) % cap;
        }
    }
    // Wait for 2 frames (40ms) of jitter buffer before starting playback
    if (stream.priming && stream.available >= kFrameSamples * 2) {
        stream.priming = false;
    }
}

void GlobalVoiceManager::MixOutputBuffer(int16_t* out_pcm, size_t sample_count) {
    std::memset(out_pcm, 0, sample_count * sizeof(int16_t));
    float gain = playback_volume_.load();
    uint64_t now = GetTickCount64();

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& kv : peer_streams_) {
        auto& stream = kv.second;
        if (now - stream.last_packet_tick > 250) {
            stream.current_level *= 0.7f;
            if (stream.current_level < 0.01f) stream.current_level = 0.0f;
        }
        if (stream.priming) continue;
        if (stream.available < sample_count) {
            stream.priming = true;
            continue;
        }
        const size_t cap = stream.ring_buffer.size();
        for (size_t i = 0; i < sample_count; ++i) {
            int32_t mixed = out_pcm[i] + static_cast<int32_t>(stream.ring_buffer[stream.read_pos] * gain);
            out_pcm[i] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
            stream.read_pos = (stream.read_pos + 1) % cap;
        }
        stream.available -= sample_count;
    }
}

float GlobalVoiceManager::GetPeerVoiceLevel(uint64_t peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peer_streams_.find(peer_id);
    if (it == peer_streams_.end()) return 0.0f;
    uint64_t now = GetTickCount64();
    if (now - it->second.last_packet_tick > 300) return 0.0f;
    return it->second.current_level;
}

} // namespace EarthVoice
