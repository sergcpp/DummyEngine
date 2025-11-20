#include "ImageAtlas.h"

int Ren::ImageAtlasArray::Allocate(const Buffer &sbuf, const int data_off, const int data_len, CommandBuffer cmd_buf,
                                     const eFormat format, const int res[2], int out_pos[3], const int border) {
    const int alloc_res[] = {res[0] < splitters_[0].resx() ? res[0] + border : res[0],
                             res[1] < splitters_[1].resy() ? res[1] + border : res[1]};

    for (int i = 0; i < layer_count_; i++) {
        const int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

            SetSubImage(0, i, out_pos[0], out_pos[1], res[0], res[1], format, sbuf, data_off, data_len, cmd_buf);

            return index;
        }
    }

    return -1;
}

bool Ren::ImageAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
