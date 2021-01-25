#include "RpInsertFence.h"

void RpInsertFence::Setup(RpBuilder &builder, int orphan_index, void **fences) {
    orphan_index_ = orphan_index;
    fences_ = fences;
}
