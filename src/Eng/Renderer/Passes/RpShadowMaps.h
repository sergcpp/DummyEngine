#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class RpShadowMaps : public RenderPassExecutor {
    bool initialized = false;
    int w_, h_;

    // lazily initialized data
    Ren::RenderPass rp_depth_only_;
    Ren::VertexInput vi_depth_pass_solid_, vi_depth_pass_vege_solid_, vi_depth_pass_transp_, vi_depth_pass_vege_transp_;

    Ren::Pipeline pi_solid_, pi_transp_;
    Ren::Pipeline pi_vege_solid_, pi_vege_transp_;

    Ren::Framebuffer shadow_fb_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    // inputs
    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;
    RpResRef instances_buf_;
    RpResRef instance_indices_buf_;
    RpResRef shared_data_buf_;
    RpResRef materials_buf_;
    RpResRef textures_buf_;
    RpResRef noise_tex_;

    // outputs
    RpResRef shadowmap_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &shadowmap_tex);
    void DrawShadowMaps(RpBuilder &builder, RpAllocTex &shadowmap_tex);

  public:
    RpShadowMaps(int w, int h) : w_(w), h_(h) {}

    void Setup(const DrawList **p_list, const RpResRef vtx_buf1, const RpResRef vtx_buf2, const RpResRef ndx_buf,
               const RpResRef materials_buf, const BindlessTextureData *bindless_tex, const RpResRef textures_buf,
               const RpResRef instances_buf, const RpResRef instance_indices_buf, const RpResRef shared_data_buf,
               const RpResRef noise_tex, const RpResRef shadowmap_tex) {
        p_list_ = p_list;
        bindless_tex_ = bindless_tex;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;

        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        materials_buf_ = materials_buf;
        textures_buf_ = textures_buf;
        noise_tex_ = noise_tex;

        shadowmap_tex_ = shadowmap_tex;
    }

    void Execute(RpBuilder &builder) override;
};
