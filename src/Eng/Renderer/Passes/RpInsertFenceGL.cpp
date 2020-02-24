#include "RpInsertFence.h"

#include <Ren/GL.h>

void RpInsertFence::Execute(Graph::RpBuilder &builder) {
    assert(!fences_[orphan_index_]);
    fences_[orphan_index_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}