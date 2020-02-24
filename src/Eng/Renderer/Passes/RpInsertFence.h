#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpInsertFence : public Graph::RenderPassBase {
    // temp data (valid only between Setup and Execute calls)
    int orphan_index_;
    void **fences_;

  public:
    void Setup(Graph::RpBuilder &builder, int orphan_index, void **fences);
    void Execute(Graph::RpBuilder &builder) override;

    const char *name() const override { return "INSERT FENCE"; }
};