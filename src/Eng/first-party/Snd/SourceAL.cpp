#include "Source.h"

#include <cassert>
#include <cstring>

#ifdef USE_AL_SOUND
#include "third-party/OpenAL/include/al.h"
#endif

#include "Buffer.h"

static_assert(AL_PLAYING - AL_INITIAL == int(Snd::eSrcState::Playing), "!");
static_assert(AL_PAUSED - AL_INITIAL == int(Snd::eSrcState::Paused), "!");
static_assert(AL_STOPPED - AL_INITIAL == int(Snd::eSrcState::Stopped), "!");

void Snd::Source::Init(const float gain, const float pos[3]) {
    assert(source_ == 0xffffffff);

    ALuint source;
    alGenSources(1, &source);

    source_ = uint32_t(source);

    set_gain(gain);
    set_position(pos);
}

void Snd::Source::Destroy() {
    if (source_ != 0xffffffff) {
        auto source = ALuint(source_);
        assert(alIsSource(source));
        alDeleteSources(1, &source);
    }
}

Snd::Source::~Source() { Destroy(); }

Snd::Source::Source(Source &&rhs) noexcept {
    source_ = std::exchange(rhs.source_, 0xffffffff);

    looping_ = std::exchange(rhs.looping_, false);
    pitch_ = std::exchange(rhs.pitch_, 1.0f);
    gain_ = std::exchange(rhs.gain_, 1.0f);
    memcpy(&position_[0], &rhs.position_[0], 3 * sizeof(float));
    rhs.position_[0] = rhs.position_[1] = rhs.position_[2] = 0.0f;
    memcpy(&velocity_[0], &rhs.velocity_[0], 3 * sizeof(float));
    rhs.velocity_[0] = rhs.velocity_[1] = rhs.velocity_[2] = 0.0f;

    for (int i = 0; i < rhs.bufs_count_; i++) {
        buf_refs_[i] = std::move(rhs.buf_refs_[i]);
    }
    bufs_count_ = std::exchange(rhs.bufs_count_, 0);
}

Snd::Source &Snd::Source::operator=(Source &&rhs) noexcept {
    ResetBuffers();
    Destroy();

    source_ = std::exchange(rhs.source_, 0xffffffff);

    looping_ = std::exchange(rhs.looping_, false);
    pitch_ = std::exchange(rhs.pitch_, 1.0f);
    gain_ = std::exchange(rhs.gain_, 1.0f);
    memcpy(&position_[0], &rhs.position_[0], 3 * sizeof(float));
    rhs.position_[0] = rhs.position_[1] = rhs.position_[2] = 0.0f;
    memcpy(&velocity_[0], &rhs.velocity_[0], 3 * sizeof(float));
    rhs.velocity_[0] = rhs.velocity_[1] = rhs.velocity_[2] = 0.0f;

    for (int i = 0; i < rhs.bufs_count_; i++) {
        buf_refs_[i] = std::move(rhs.buf_refs_[i]);
    }
    bufs_count_ = std::exchange(rhs.bufs_count_, 0);

    return *this;
}

void Snd::Source::ResetBuffers() {
    for (int i = 0; i < bufs_count_; i++) {
        buf_refs_[i] = {};
    }
    bufs_count_ = 0;
}

Snd::eSrcState Snd::Source::GetState() const {
    ALint state;
    alGetSourcei(ALuint(source_), AL_SOURCE_STATE, &state);
    return eSrcState(state - AL_INITIAL);
}

const Snd::BufferRef &Snd::Source::GetBuffer(const int i) const { return buf_refs_[i]; }

void Snd::Source::SetBuffer(const BufferRef &buf) {
    ResetBuffers();

    buf_refs_[0] = buf;
    bufs_count_ = 1;

    alSourcei(ALuint(source_), AL_BUFFER, ALint(buf->id()));
}

void Snd::Source::EnqueueBuffers(const BufferRef bufs[], const int bufs_count) {
    assert(bufs_count_ + bufs_count < MaxQueuedBuffers);

    ALuint al_buffers[MaxQueuedBuffers];
    for (int i = 0; i < bufs_count; i++) {
        buf_refs_[bufs_count_ + i] = bufs[i];
        al_buffers[i] = ALuint(bufs[i]->id());
    }
    bufs_count_ += bufs_count;

    alSourceQueueBuffers(ALuint(source_), bufs_count, al_buffers);
}

Snd::BufferRef Snd::Source::UnqueueProcessedBuffer() {
    ALint processed_count;
    alGetSourcei(ALuint(source_), AL_BUFFERS_PROCESSED, &processed_count);

    if (processed_count) {
        ALuint buf_id;
        alSourceUnqueueBuffers(ALuint(source_), 1, &buf_id);

        BufferRef ret_buf = std::move(buf_refs_[0]);
        --bufs_count_;
        assert(buf_id == ret_buf->id());

        for (int i = 0; i < bufs_count_; i++) {
            buf_refs_[i] = std::move(buf_refs_[i + 1]);
        }

        return ret_buf;
    } else {
        return {};
    }
}

void Snd::Source::Play() { alSourcePlay(ALuint(source_)); }

void Snd::Source::Pause() { alSourcePause(ALuint(source_)); }

void Snd::Source::Stop() { alSourceStop(ALuint(source_)); }

float Snd::Source::GetOffset() {
    float ret;
    alGetSourcef(ALuint(source_), AL_SEC_OFFSET, &ret);
    return ret;
}

void Snd::Source::SetOffset(const float offset_s) {
    alSourcef(ALuint(source_), AL_SEC_OFFSET, offset_s);
}

void Snd::Source::set_looping(const bool val) {
    alSourcei(ALuint(source_), AL_LOOPING, val ? AL_TRUE : AL_FALSE);
    looping_ = val;
}

void Snd::Source::set_pitch(const float pitch) {
    alSourcef(ALuint(source_), AL_PITCH, pitch);
    pitch_ = pitch;
}

void Snd::Source::set_gain(const float gain) {
    alSourcef(ALuint(source_), AL_GAIN, gain);
    gain_ = gain;
}

void Snd::Source::set_position(const float pos[3]) {
    alSource3f(ALuint(source_), AL_POSITION, pos[0], pos[1], pos[2]);
    memcpy(&position_[0], &pos[0], 3 * sizeof(float));
}

void Snd::Source::set_velocity(const float vel[3]) {
    alSource3f(ALuint(source_), AL_VELOCITY, vel[0], vel[1], vel[2]);
    memcpy(&velocity_[0], &vel[0], 3 * sizeof(float));
}
