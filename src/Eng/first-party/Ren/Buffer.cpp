#include "Buffer.h"

bool Ren::UpdateBuffer(Buffer &dst, uint32_t dst_offset, uint32_t data_size, const void *data, Buffer &stage,
                       uint32_t map_offset, uint32_t map_size, void *_cmd_buf) {
    if (!data || !data_size) {
        return true;
    }

    uint8_t *stage_mem = stage.MapRange(Ren::BufMapWrite, map_offset, map_size);
    if (stage_mem) {
        memcpy(stage_mem, data, data_size);
        stage.FlushMappedRange(0, stage.AlignMapOffset(data_size));
        stage.Unmap();
    } else {
        return false;
    }

    Ren::CopyBufferToBuffer(stage, map_offset, dst, 0, data_size, _cmd_buf);

    return true;
}