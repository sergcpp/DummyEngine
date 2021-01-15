#include "RpInsertFence.h"

void RpInsertFence::Setup(RpBuilder &builder, int orphan_index, void **fences) {
    orphan_index_ = orphan_index;
    fences_ = fences;

    //input_[0] = in_skin_transforms_buf;
    input_count_ = 0;

    output_count_ = 0;
}
