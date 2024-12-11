#include "Buffer.h"

#include <al.h>

namespace Snd {
const uint32_t g_al_formats[] = {0xffffffff, AL_FORMAT_MONO8, AL_FORMAT_MONO16, AL_FORMAT_STEREO8, AL_FORMAT_STEREO16};
static_assert(std::size(g_al_formats) == size_t(eBufFormat::Count), "!");
const int g_bytes_per_sample[] = {0, 1, 2, 2, 4};
static_assert(std::size(g_bytes_per_sample) == size_t(eBufFormat::Count), "!");
} // namespace Snd

Snd::Buffer::Buffer(std::string_view name, Span<const uint8_t> data, const BufParams &params,
                    eBufLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, params, load_status, log);
}

void Snd::Buffer::FreeBuf() {
    if (buf_id_ != 0xffffffff) {
        ALuint buf_id = std::exchange(buf_id_, 0xffffffff);
        alDeleteBuffers(1, &buf_id);
    }
}

Snd::Buffer::~Buffer() { FreeBuf(); }

Snd::Buffer::Buffer(Buffer &&rhs) noexcept { (*this) = std::move(rhs); }

Snd::Buffer &Snd::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(std::move((RefCounter &)rhs));

    FreeBuf();

    name_ = std::move(rhs.name_);
    buf_id_ = std::exchange(rhs.buf_id_, 0xffffffff);
    size_ = std::exchange(rhs.size_, 0);
    params_ = std::exchange(rhs.params_, {});

    return *this;
}

void Snd::Buffer::Init(Span<const uint8_t> data, const BufParams &params, eBufLoadStatus *load_status, ILog *log) {
    if (buf_id_ == 0xffffffff) {
        ALuint buf_id;
        alGenBuffers(1, &buf_id);
        buf_id_ = uint32_t(buf_id);
    }

    if (!data.empty()) {
        SetData(data, params);

        if (load_status) {
            (*load_status) = eBufLoadStatus::CreatedFromData;
        }
    } else {
        if (load_status) {
            (*load_status) = eBufLoadStatus::CreatedDefault;
        }
    }
}

float Snd::Buffer::GetDurationS() const {
    return float(size_ / g_bytes_per_sample[int(params_.format)]) / float(params_.samples_per_sec);
}

void Snd::Buffer::SetData(Span<const uint8_t> data, const BufParams &params) {
    assert(buf_id_ != 0xffffffff);

    alBufferData(ALuint(buf_id_), g_al_formats[int(params.format)], data.data(), ALsizei(data.size()),
                 params.samples_per_sec);
    size_ = uint32_t(data.size());
    params_ = params;
}
