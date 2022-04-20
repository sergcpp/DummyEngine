#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSkinningExecutor : public RpExecutor {
    const Ren::Pipeline &pi_skinning_;
    const DrawList *&p_list_;

    RpResRef skin_vtx_buf_;
    RpResRef skin_transforms_buf_;
    RpResRef shape_keys_buf_;
    RpResRef delta_buf_;

    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;

  public:
    RpSkinningExecutor(const Ren::Pipeline &pi_skinning, const DrawList *&p_list, const RpResRef skin_vtx_buf,
                       const RpResRef skin_transforms_buf, const RpResRef shape_keys_buf, const RpResRef delta_buf,
                       const RpResRef vtx_buf1, const RpResRef vtx_buf2)
        : pi_skinning_(pi_skinning), p_list_(p_list), skin_vtx_buf_(skin_vtx_buf),
          skin_transforms_buf_(skin_transforms_buf), shape_keys_buf_(shape_keys_buf), delta_buf_(delta_buf),
          vtx_buf1_(vtx_buf1), vtx_buf2_(vtx_buf2) {}

    void Execute(RpBuilder &builder) override;
};