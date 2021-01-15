#include "RpSkinning.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Names.h"
#include "../Renderer_Structs.h"

void RpSkinning::Setup(RpBuilder &builder, const DrawList &list, const int orphan_index,
                       Ren::BufferRef vtx_buf1, Ren::BufferRef vtx_buf2,
                       Ren::BufferRef delta_buf, Ren::BufferRef skin_vtx_buf) {
    orphan_index_ = orphan_index;

    // fences_ = fences;
    skin_regions_ = list.skin_regions;

    vtx_buf1_ = std::move(vtx_buf1);
    vtx_buf2_ = std::move(vtx_buf2);
    delta_buf_ = std::move(delta_buf);
    skin_vtx_buf_ = std::move(skin_vtx_buf);

    input_[0] = builder.ReadBuffer(SKIN_TRANSFORMS_BUF);
    input_[1] = builder.ReadBuffer(SHAPE_KEYS_BUF);
    input_count_ = 2;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    // output_[1] = builder.WriteBuffer(input_[1], *this);
    output_count_ = 0;
}

void RpSkinning::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    skinning_prog_ = sh.LoadProgram(ctx, "skinning_prog", "internal/skinning.comp.glsl");
    assert(skinning_prog_->ready());

    initialized = true;
}
