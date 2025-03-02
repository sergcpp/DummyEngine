#pragma once

#include <vector>

#include "Bitmask.h"
#include "Fence.h"
#include "MemoryAllocator.h"
#include "Resource.h"
#include "SmallVector.h"
#include "Storage.h"
#include "String.h"

namespace Ren {
class ILog;
struct ApiContext;
#define X(_0) _0,
enum class eType : uint8_t {
#include "Types.inl"
    _Count
};
#undef X
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

std::string_view TypeName(eType type);
eType Type(std::string_view name);

struct BufHandle {
#if defined(REN_VK_BACKEND)
    VkBuffer buf = {};
#elif defined(REN_GL_BACKEND) || defined(REN_SW_BACKEND)
    uint32_t buf = 0;
#endif
    uint32_t generation = 0;

    operator bool() const {
#if defined(REN_VK_BACKEND)
        return buf != VkBuffer{};
#elif defined(REN_GL_BACKEND) || defined(REN_SW_BACKEND)
        return buf != 0;
#endif
    }
};
inline bool operator==(const BufHandle &lhs, const BufHandle &rhs) {
    return lhs.buf == rhs.buf && lhs.generation == rhs.generation;
}
inline bool operator!=(const BufHandle &lhs, const BufHandle &rhs) {
    return lhs.buf != rhs.buf || lhs.generation != rhs.generation;
}
inline bool operator<(const BufHandle &lhs, const BufHandle &rhs) {
    if (lhs.buf < rhs.buf) {
        return true;
    } else if (lhs.buf == rhs.buf) {
        return lhs.generation < rhs.generation;
    }
    return false;
}

struct SubAllocation {
    uint32_t offset = 0xffffffff;
    uint32_t block = 0xffffffff;
};

class Buffer : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    BufHandle handle_;
    String name_;
    std::unique_ptr<FreelistAlloc> sub_alloc_;
    MemAllocation alloc_;
#if defined(REN_VK_BACKEND)
    MemAllocators *mem_allocs_ = nullptr;
    VkDeviceMemory dedicated_mem_ = {};
#endif
    eBufType type_ = eBufType::Undefined;
    uint32_t size_ = 0, size_alignment_ = 1;
    uint8_t *mapped_ptr_ = nullptr;
    uint32_t mapped_offset_ = 0xffffffff;

    static int g_GenCounter;

  public:
    Buffer() = default;
    Buffer(std::string_view name, ApiContext *api_ctx, eBufType type, uint32_t initial_size,
           uint32_t size_alignment = 1, MemAllocators *mem_allocs = nullptr);
    Buffer(std::string_view name, ApiContext *api_ctx, eBufType type, const BufHandle &handle, MemAllocation &&alloc,
           uint32_t initial_size, uint32_t size_alignment = 1)
        : api_ctx_(api_ctx), handle_(handle), name_(name), alloc_(std::move(alloc)), type_(type), size_(initial_size),
          size_alignment_(size_alignment) {}
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
#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkBuffer vk_handle() const { return handle_.buf; }
    [[nodiscard]] VkDeviceMemory mem() const { return dedicated_mem_; }
    [[nodiscard]] VkDeviceAddress vk_device_address() const;
#elif defined(REN_GL_BACKEND) || defined(REN_SW_BACKEND)
    [[nodiscard]] uint32_t id() const { return handle_.buf; }
#endif
    [[nodiscard]] uint32_t generation() const { return handle_.generation; }
    [[nodiscard]] const MemAllocation &mem_alloc() const { return alloc_; }

    [[nodiscard]] uint8_t *mapped_ptr() const { return mapped_ptr_; }

    SubAllocation AllocSubRegion(uint32_t size, uint32_t alignment, std::string_view tag,
                                 const Buffer *init_buf = nullptr, CommandBuffer cmd_buf = {}, uint32_t init_off = 0);
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

    mutable eResState resource_state = eResState::Undefined;
};

void CopyBufferToBuffer(Buffer &src, uint32_t src_offset, Buffer &dst, uint32_t dst_offset, uint32_t size,
                        CommandBuffer cmd_buf);
// Update buffer using stage buffer
bool UpdateBuffer(Buffer &dst, uint32_t dst_offset, uint32_t data_size, const void *data, Buffer &stage,
                  uint32_t map_offset, uint32_t map_size, CommandBuffer cmd_buf);

#if defined(REN_GL_BACKEND)
void GLUnbindBufferUnits(int start, int count);
#endif

using BufferRef = StrongRef<Buffer, NamedStorage<Buffer>>;
using WeakBufferRef = WeakRef<Buffer, NamedStorage<Buffer>>;
using BufferStorage = NamedStorage<Buffer>;
} // namespace Ren