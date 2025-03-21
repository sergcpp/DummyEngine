#pragma once

#include <cstdint>

#include "Buffer.h"

namespace Snd {
const int MaxQueuedBuffers = 8;
 
enum class eSrcState { Initial, Playing, Paused, Stopped };

class Source {
#ifdef USE_AL_SOUND
    uint32_t source_ = 0xffffffff;
#endif

    bool looping_ = false;
    float pitch_ = 1.0f;
    float gain_ = 1.0f;
    float position_[3] = {};
    float velocity_[3] = {};

    BufferRef buf_refs_[MaxQueuedBuffers];
    int bufs_count_ = 0;

  public:
    Source() = default;
    ~Source();
    Source(const Source &rhs) = delete;
    Source(Source &&rhs) noexcept;
    Source &operator=(const Source &rhs) = delete;
    Source &operator=(Source &&rhs) noexcept;

    void Init(float gain, const float pos[3]);
    void Destroy();

    [[nodiscard]] eSrcState GetState() const;
    
    [[nodiscard]] const BufferRef &GetBuffer(int i) const;
    void SetBuffer(const BufferRef& buf);
    void EnqueueBuffers(const BufferRef bufs[], int bufs_count);

    BufferRef UnqueueProcessedBuffer();

    void Play() const;
    void Pause() const;
    void Stop() const;

    float GetOffset() const;
    void SetOffset(float offset_s);
    void ResetBuffers();

    [[nodiscard]] bool looping() const { return looping_; }
    [[nodiscard]] float pitch() const { return pitch_; }
    [[nodiscard]] float gain() const { return gain_; }
    [[nodiscard]] const float *position() const { return position_; }
    [[nodiscard]] const float *velocity() const { return velocity_; }

    void set_looping(bool val);
    void set_pitch(float pitch);
    void set_gain(float gain);
    void set_position(const float pos[3]);
    void set_velocity(const float vel[3]);
};
} // namespace Snd
