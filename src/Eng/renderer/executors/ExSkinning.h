#pragma once

#include <Ren/Fwd.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class ShaderLoader;
class ExSkinning final : public FgExecutor {
    Ren::PipelineRef pi_skinning_;
    const DrawList *&p_list_;

    FgResRef skin_vtx_buf_;
    FgResRef skin_transforms_buf_;
    FgResRef shape_keys_buf_;
    FgResRef delta_buf_;

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

  public:
    ExSkinning(const DrawList *&p_list, const FgResRef skin_vtx_buf, const FgResRef skin_transforms_buf,
               const FgResRef shape_keys_buf, const FgResRef delta_buf, const FgResRef vtx_buf1,
               const FgResRef vtx_buf2)
        : p_list_(p_list), skin_vtx_buf_(skin_vtx_buf), skin_transforms_buf_(skin_transforms_buf),
          shape_keys_buf_(shape_keys_buf), delta_buf_(delta_buf), vtx_buf1_(vtx_buf1), vtx_buf2_(vtx_buf2) {}

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng