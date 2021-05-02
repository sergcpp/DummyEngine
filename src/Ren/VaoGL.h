#pragma once

#include "Buffer.h"

namespace Ren {
enum class eType : uint8_t {
    Float16,
    Float32,
    Uint32,
    Uint16UNorm,
    Int16SNorm,
    Uint8UNorm,
    Int32,
    _Count
};

struct VtxAttribDesc {
    BufHandle buf;
    uint8_t loc;
    uint8_t size;
    eType type;
    uint8_t stride;
    uintptr_t pointer;
};
static_assert(sizeof(VtxAttribDesc) == 24, "!");

inline bool operator==(const VtxAttribDesc &lhs, const VtxAttribDesc &rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(VtxAttribDesc)) == 0;
}

const int MaxVertexAttributes = 8;

class Vao {
    uint32_t id_;

    VtxAttribDesc attribs_[MaxVertexAttributes] = {};
    int attribs_count_ = 0;
    BufHandle elem_buf_;

  public:
    Vao();
    Vao(const Vao &rhs) = delete;
    Vao(Vao &&rhs) = delete;
    ~Vao();

    uint32_t id() const { return id_; }

    bool Setup(const VtxAttribDesc attribs[], int attribs_count, BufHandle elem_buf);
};
} // namespace Ren