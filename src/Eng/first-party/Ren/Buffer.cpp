#include "Buffer.h"

namespace Ren {
#define X(_0) #_0,
static const std::string_view g_type_names[] = {
#include "Types.inl"
};
#undef X
static_assert(std::size(g_type_names) == int(eType::_Count));
} // namespace Ren

std::string_view Ren::TypeName(const eType type) { return g_type_names[uint8_t(type)]; }

Ren::eType Ren::Type(std::string_view name) {
    for (int i = 0; i < int(eType::_Count); ++i) {
        if (name == g_type_names[i]) {
            return eType(i);
        }
    }
    return eType::Undefined;
}

bool Ren::UpdateBuffer(Buffer &dst, const uint32_t dst_offset, const uint32_t data_size, const void *data,
                       Buffer &stage, const uint32_t map_offset, const uint32_t map_size, CommandBuffer cmd_buf) {
    if (!data || !data_size) {
        return true;
    }

    uint8_t *stage_mem = stage.MapRange(map_offset, map_size);
    if (stage_mem) {
        memcpy(stage_mem, data, data_size);
        stage.Unmap();
    } else {
        return false;
    }

    CopyBufferToBuffer(stage, map_offset, dst, dst_offset, data_size, cmd_buf);

    return true;
}