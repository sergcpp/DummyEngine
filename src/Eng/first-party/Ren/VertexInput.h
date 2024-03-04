#pragma once

#include "Buffer.h"
#include "Span.h"

namespace Ren {
struct VtxAttribDesc {
    BufHandle buf;
    uint8_t loc;
    uint8_t size;
    eType type;
    uint8_t stride;
    uint32_t offset;

    VtxAttribDesc(const BufHandle &_buf, int _loc, uint8_t _size, eType _type, int _stride, uint32_t _offset)
        : buf(_buf), loc(_loc), size(_size), type(_type), stride(_stride), offset(_offset) {}
    VtxAttribDesc(const BufferRef &_buf, int _loc, uint8_t _size, eType _type, int _stride, uint32_t _offset)
        : buf(_buf->handle()), loc(_loc), size(_size), type(_type), stride(_stride), offset(_offset) {}
};
inline bool operator==(const VtxAttribDesc &lhs, const VtxAttribDesc &rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(VtxAttribDesc)) == 0;
}

class VertexInput {
#if defined(USE_GL_RENDER)
    uint32_t gl_vao_ = 0;
#endif
  public:
    SmallVector<VtxAttribDesc, 8> attribs;
    BufHandle elem_buf;

    VertexInput();
    VertexInput(const VertexInput &rhs) = delete;
    VertexInput(VertexInput &&rhs) noexcept { (*this) = std::move(rhs); }
    ~VertexInput();

    VertexInput &operator=(const VertexInput &rhs) = delete;
    VertexInput &operator=(VertexInput &&rhs) noexcept;

#if defined(USE_VK_RENDER)
    void BindBuffers(ApiContext *api_ctx, VkCommandBuffer cmd_buf, uint32_t index_offset, int index_type) const;
    void FillVKDescriptions(SmallVectorImpl<VkVertexInputBindingDescription, 4> &out_bindings,
                            SmallVectorImpl<VkVertexInputAttributeDescription, 4> &out_attribs) const;
#elif defined(USE_GL_RENDER)
    uint32_t gl_vao() const { return gl_vao_; }
#endif

    bool Setup(Span<const VtxAttribDesc> attribs, const BufHandle &elem_buf);
    bool Setup(Span<const VtxAttribDesc> attribs, const BufferRef &elem_buf) {
        return Setup(attribs, elem_buf->handle());
    }
};
} // namespace Ren