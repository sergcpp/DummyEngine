#pragma once

#include <Ren/Fwd.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class ShaderLoader;
class ExSkinning final : public FgExecutor {
    Ren::PipelineHandle pi_skinning_;
    const DrawList *&p_list_;

    FgBufROHandle skin_vtx_;
    FgBufROHandle skin_transforms_;
    FgBufROHandle shape_keys_;
    FgBufROHandle delta_;

    FgBufRWHandle vtx_buf1_;
    FgBufRWHandle vtx_buf2_;

    void LazyInit(const FgContext &fg);

  public:
    ExSkinning(const DrawList *&p_list, const FgBufROHandle skin_vtx, const FgBufROHandle skin_transforms,
               const FgBufROHandle shape_keys, const FgBufROHandle delta, const FgBufRWHandle vtx_buf1,
               const FgBufRWHandle vtx_buf2)
        : p_list_(p_list), skin_vtx_(skin_vtx), skin_transforms_(skin_transforms), shape_keys_(shape_keys),
          delta_(delta), vtx_buf1_(vtx_buf1), vtx_buf2_(vtx_buf2) {}

    void Execute(const FgContext &fg) override;
};
} // namespace Eng