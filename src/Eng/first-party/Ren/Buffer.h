#pragma once

#include <vector>

#include "Bitmask.h"
#include "Fence.h"
#include "FreelistAlloc.h"
#include "Resource.h"
#include "SmallVector.h"
#include "Storage.h"
#include "String.h"

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
    Upload,
    Readback,
    AccStructure,
    ShaderBinding,
    Indirect,
    _Count
};

struct BufHandle {
#if defined(USE_VK_RENDER)
    VkBuffer buf = {};
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id = 0;
#endif
    uint32_t generation = 0;

    operator bool() const {
#if defined(USE_VK_RENDER)
        return buf != VkBuffer{};
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

struct SubAllocation {
    uint32_t offset = 0xffffffff;
    uint32_t block = 0xffffffff;
};

class Buffer : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    BufHandle handle_;
    String name_;
    std::unique_ptr<FreelistAlloc> alloc_;
#if defined(USE_VK_RENDER)
    VkDeviceMemory mem_ = {};
#endif
    eBufType type_ = eBufType::Undefined;
    uint32_t size_ = 0, suballoc_align_ = 1;
    uint8_t *mapped_ptr_ = nullptr;
    uint32_t mapped_offset_ = 0xffffffff;
#ifndef NDEBUG
    SmallVector<RangeFence, 4> flushed_ranges_;
#endif

    static int g_GenCounter;

  public:
    Buffer() = default;
    explicit Buffer(std::string_view name, ApiContext *api_ctx, eBufType type, uint32_t initial_size,
                    uint32_t suballoc_align = 1);
    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Buffer();

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    [[nodiscard]] const String &name() const { return name_; }
    [[nodiscard]] eBufType type() const { return type_; }
    [[nodiscard]] uint32_t size() const { return size_; }

    [[nodiscard]] BufHandle handle() const { return handle_; }
    [[nodiscard]] ApiContext *api_ctx() const { return api_ctx_; }
#if defined(USE_VK_RENDER)
    [[nodiscard]] VkBuffer vk_handle() const { return handle_.buf; }
    [[nodiscard]] VkDeviceMemory mem() const { return mem_; }
    [[nodiscard]] VkDeviceAddress vk_device_address() const;
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    [[nodiscard]] uint32_t id() const { return handle_.id; }
#endif
    [[nodiscard]] uint32_t generation() const { return handle_.generation; }

    [[nodiscard]] uint8_t *mapped_ptr() const { return mapped_ptr_; }

    SubAllocation AllocSubRegion(uint32_t size, std::string_view tag, const Buffer *init_buf = nullptr,
                                 CommandBuffer cmd_buf = {}, uint32_t init_off = 0);
    void UpdateSubRegion(uint32_t offset, uint32_t size, const Buffer &init_buf, uint32_t init_off = 0,
                         CommandBuffer cmd_buf = {});
    bool FreeSubRegion(SubAllocation alloc);

    void Resize(uint32_t new_size, bool keep_content = true);
    void Free();
    void FreeImmediate();

    uint32_t AlignMapOffset(uint32_t offset);

    uint8_t *Map(const bool persistent = false) { return MapRange(0, size_, persistent); }
    uint8_t *MapRange(uint32_t offset, uint32_t size, bool persistent = false);
    void Unmap();

    void Fill(uint32_t dst_offset, uint32_t size, uint32_t data, CommandBuffer cmd_buf);
    void UpdateImmediate(uint32_t dst_offset, uint32_t size, const void *data, CommandBuffer cmd_buf);

    void Print(ILog *log);

    mutable eResState resource_state = eResState::Undefined;
};

void CopyBufferToBuffer(Buffer &src, uint32_t src_offset, Buffer &dst, uint32_t dst_offset, uint32_t size,
                        CommandBuffer cmd_buf);
// Update buffer using stage buffer
bool UpdateBuffer(Buffer &dst, uint32_t dst_offset, uint32_t data_size, const void *data, Buffer &stage,
                  uint32_t map_offset, uint32_t map_size, CommandBuffer cmd_buf);

#if defined(USE_GL_RENDER)
void GLUnbindBufferUnits(int start, int count);
#endif

using BufferRef = StrongRef<Buffer>;
using WeakBufferRef = WeakRef<Buffer>;
using BufferStorage = Storage<Buffer>;
} // namespace Ren