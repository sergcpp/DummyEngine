#pragma once

#include <cstdint>

#include <string_view>

#include "Span.h"
#include "Storage.h"
#include "String.h"

namespace Snd {
class ILog;

enum class eBufFormat { Undefined, Mono8, Mono16, Stereo8, Stereo16, Count };

struct BufParams {
    eBufFormat format = eBufFormat::Undefined;
    uint32_t samples_per_sec = 0;
};

enum class eBufLoadStatus { Found, CreatedDefault, CreatedFromData };

class Buffer : public RefCounter {
    String name_;
#ifdef USE_AL_SOUND
    uint32_t buf_id_ = 0xffffffff;
#endif
    uint32_t size_ = 0;
    BufParams params_;

    void FreeBuf();

  public:
    Buffer(std::string_view name, Span<const uint8_t> data, const BufParams &params, eBufLoadStatus *load_status,
           ILog *log);
    ~Buffer();

    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept;

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    [[nodiscard]] const String &name() const { return name_; }

    [[nodiscard]] bool ready() const { return params_.format != eBufFormat::Undefined; }

#ifdef USE_AL_SOUND
    [[nodiscard]] uint32_t id() const { return buf_id_; }
#endif

    [[nodiscard]] float GetDurationS() const;

    void SetData(Span<const uint8_t> data, const BufParams &params);

    void Init(Span<const uint8_t> data, const BufParams &params, eBufLoadStatus *load_status, ILog *log);
};

typedef StrongRef<Buffer> BufferRef;
typedef Storage<Buffer> BufferStorage;
} // namespace Snd
