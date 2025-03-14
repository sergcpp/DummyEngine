#pragma once

#include "Buffer.h"
#include "Span.h"

namespace Ren {
struct VtxAttribDesc {
    WeakBufRef buf;
    uint8_t loc = 0;
    uint8_t size = 0;
    eType type = eType::Undefined;
    uint8_t stride = 0;
    uint32_t offset = 0;

    VtxAttribDesc() = default;
    VtxAttribDesc(const BufRef &_buf, int _loc, uint8_t _size, eType _type, int _stride, uint32_t _offset)
        : buf(_buf), loc(_loc), size(_size), type(_type), stride(_stride), offset(_offset) {}
};
static_assert(sizeof(VtxAttribDesc) == 32);

inline bool operator==(const VtxAttribDesc &lhs, const VtxAttribDesc &rhs) {
    return lhs.buf == rhs.buf && lhs.loc == rhs.loc && lhs.size == rhs.size && lhs.type == rhs.type &&
           lhs.stride == rhs.stride && lhs.offset == rhs.offset;
}
inline bool operator!=(const VtxAttribDesc &lhs, const VtxAttribDesc &rhs) {
    return lhs.buf != rhs.buf || lhs.loc != rhs.loc || lhs.size != rhs.size || lhs.type != rhs.type ||
           lhs.stride != rhs.stride || lhs.offset != rhs.offset;
}
inline bool operator<(const VtxAttribDesc &lhs, const VtxAttribDesc &rhs) {
    return std::tie(lhs.buf, lhs.loc, lhs.size, lhs.type, lhs.stride, lhs.offset) <
           std::tie(rhs.buf, rhs.loc, rhs.size, rhs.type, rhs.stride, rhs.offset);
}

class VertexInput : public RefCounter {
#if defined(REN_GL_BACKEND)
    mutable uint32_t gl_vao_ = 0;
    mutable SmallVector<BufHandle, 4> attribs_buf_handles_;
    mutable BufHandle elem_buf_handle_;
#endif
  public:
    SmallVector<VtxAttribDesc, 4> attribs;
    WeakBufRef elem_buf;

    VertexInput();
    VertexInput(Span<const VtxAttribDesc> attribs, const BufRef &elem_buf) { Init(attribs, elem_buf); }
    VertexInput(const VertexInput &rhs) = delete;
    VertexInput(VertexInput &&rhs) noexcept { (*this) = std::move(rhs); }
    ~VertexInput();

    VertexInput &operator=(const VertexInput &rhs) = delete;
    VertexInput &operator=(VertexInput &&rhs) noexcept;

    bool operator==(const VertexInput &rhs) const { return elem_buf == rhs.elem_buf && attribs == rhs.attribs; }
    bool operator!=(const VertexInput &rhs) const { return elem_buf != rhs.elem_buf || attribs != rhs.attribs; }
    bool operator<(const VertexInput &rhs) const {
        if (elem_buf < rhs.elem_buf) {
            return true;
        } else if (elem_buf == rhs.elem_buf) {
            return attribs < rhs.attribs;
        }
        return false;
    }

#if defined(REN_VK_BACKEND)
    void BindBuffers(ApiContext *api_ctx, VkCommandBuffer cmd_buf, uint32_t index_offset, int index_type) const;
    void FillVKDescriptions(
        SmallVectorImpl<VkVertexInputBindingDescription, aligned_allocator<VkVertexInputBindingDescription, 4>>
            &out_bindings,
        SmallVectorImpl<VkVertexInputAttributeDescription, aligned_allocator<VkVertexInputAttributeDescription, 4>>
            &out_attribs) const;
#elif defined(REN_GL_BACKEND)
    uint32_t GetVAO() const;
#endif

    void Init(Span<const VtxAttribDesc> attribs, const BufRef &elem_buf);
};

using VertexInputRef = StrongRef<VertexInput, SortedStorage<VertexInput>>;
using WeakVertexInputRef = WeakRef<VertexInput, SortedStorage<VertexInput>>;
using VertexInputStorage = SortedStorage<VertexInput>;
} // namespace Ren