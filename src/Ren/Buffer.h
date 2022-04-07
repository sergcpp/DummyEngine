#pragma once

#include <vector>

#include "Fence.h"
#include "LinearAlloc.h"
#include "Resource.h"
#include "SmallVector.h"
#include "Storage.h"
#include "String.h"

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
class ILog;
struct ApiContext;

enum class eType : uint8_t {
    Undefined,
    Float16,
    Float32,
    Uint32,
    Uint16,
    Uint16UNorm,
    Int16SNorm,
    Uint8UNorm,
    Int32,
    _Count
};
enum class eBufType : uint8_t {
    Undefined,
    VertexAttribs,
    VertexIndices,
    Texture,
    Uniform,
    Storage,
    Stage,
    AccStructure,
    ShaderBinding,
    Indirect,
    _Count
};

const uint8_t BufMapRead = (1u << 0u);
const uint8_t BufMapWrite = (1u << 1u);

struct BufHandle {
#if defined(USE_VK_RENDER)
    VkBuffer buf = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id = 0;
#endif
    uint32_t generation = 0;

    operator bool() const {
#if defined(USE_VK_RENDER)
        return buf != VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        return id != 0;
#endif
    }
};
inline bool operator==(const BufHandle lhs, const BufHandle rhs) {
    return
#if defined(USE_VK_RENDER)
        lhs.buf == rhs.buf &&
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        lhs.id == rhs.id &&
#endif
        lhs.generation == rhs.generation;
}

struct RangeFence {
    std::pair<uint32_t, uint32_t> range;
    SyncFence fence;

    RangeFence(const std::pair<uint32_t, uint32_t> _range, SyncFence &&_fence)
        : range(_range), fence(std::move(_fence)) {}
};

class Buffer : public RefCounter, public LinearAlloc {
    ApiContext *api_ctx_ = nullptr;
    BufHandle handle_;
    String name_;
#if defined(USE_VK_RENDER)
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
#endif
    eBufType type_ = eBufType::Undefined;
    uint32_t size_ = 0;
    uint8_t *mapped_ptr_ = nullptr;
    uint32_t mapped_offset_ = 0xffffffff;
#ifndef NDEBUG
    SmallVector<RangeFence, 4> flushed_ranges_;
#endif

    static int g_GenCounter;

  public:
    Buffer() = default;
    explicit Buffer(const char *name, ApiContext *api_ctx, eBufType type, uint32_t initial_size,
                    uint32_t suballoc_align = 1);
    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Buffer();

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    const String &name() const { return name_; }
    eBufType type() const { return type_; }
    // uint32_t size() const { return size_; }

    BufHandle handle() const { return handle_; }
#if defined(USE_VK_RENDER)
    ApiContext *api_ctx() const { return api_ctx_; }
    VkBuffer vk_handle() const { return handle_.buf; }
    VkDeviceMemory mem() const { return mem_; }
    VkDeviceAddress vk_device_address() const;
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id() const { return handle_.id; }
#endif
    uint32_t generation() const { return handle_.generation; }

    bool is_mapped() const { return mapped_ptr_ != nullptr; }
    uint8_t *mapped_ptr() const { return mapped_ptr_; }

    uint32_t AllocSubRegion(uint32_t size, const char *tag, const Buffer *init_buf = nullptr, void *cmd_buf = nullptr,
                            uint32_t init_off = 0);
    void UpdateSubRegion(uint32_t offset, uint32_t size, const Buffer &init_buf, uint32_t init_off = 0,
                         void *cmd_buf = nullptr);
    bool FreeSubRegion(uint32_t offset, uint32_t size);

    void Resize(uint32_t new_size, bool keep_content = true);
    void Free();

    uint32_t AlignMapOffset(uint32_t offset);

    uint8_t *Map(const uint8_t dir, const bool persistent = false) { return MapRange(dir, 0, size_, persistent); }
    uint8_t *MapRange(uint8_t dir, uint32_t offset, uint32_t size, bool persistent = false);
    void FlushMappedRange(uint32_t offset, uint32_t size);
    void Unmap();

    void Print(ILog *log);

    mutable eResState resource_state = eResState::Undefined;
};

void CopyBufferToBuffer(Buffer &src, uint32_t src_offset, Buffer &dst, uint32_t dst_offset, uint32_t size,
                        void *_cmd_buf);
void FillBuffer(Buffer &dst, uint32_t dst_offset, uint32_t size, uint32_t data, void *_cmd_buf);

bool UpdateBufferContents(const void *data, uint32_t data_size, Buffer &stage, uint32_t map_offset, uint32_t map_size,
                          Buffer &dst, uint32_t dst_offset, void *_cmd_buf);

#if defined(USE_GL_RENDER)
void GLUnbindBufferUnits(int start, int count);
#endif

using BufferRef = StrongRef<Buffer>;
using WeakBufferRef = WeakRef<Buffer>;
using BufferStorage = Storage<Buffer>;
} // namespace Ren