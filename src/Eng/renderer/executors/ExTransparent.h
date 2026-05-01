#pragma once

#include <Ren/Common.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
class PrimDraw;
class ShaderLoader;
struct view_state_t;

class ExTransparent final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle instances;
        FgBufROHandle instance_indices;
        FgBufROHandle shared_data;
        FgBufROHandle cells;
        FgBufROHandle items;
        FgBufROHandle lights;
        FgBufROHandle decals;
        FgBufROHandle materials;
        FgResRef lm_tex[4];
        FgImgROHandle brdf_lut;
        FgImgROHandle noise;
        FgImgROHandle cone_rt_lut;
        FgImgROHandle dummy_black;
        FgImgROHandle shadow_depth;
        FgImgROHandle ssao;
        FgImgRWHandle color;
        FgImgRWHandle normal;
        FgImgRWHandle spec;
        FgImgRWHandle depth;
    };

    ExTransparent(const Ren::ApiContext &api, const DrawList **p_list, const view_state_t *view_state,
                  const BindlessTextureData *bindless_tex, const Args *args)
        : api_(api), p_list_(p_list), view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}
    ~ExTransparent() final;

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::VertexInputHandle draw_pass_vi_;
    Ren::RenderPassHandle rp_transparent_;

#if defined(REN_VK_BACKEND)
    VkDescriptorSetLayout descr_set_layout_ = {};
#endif

    // temp data (valid only between Setup and Execute calls)
    const Ren::ApiContext &api_;
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle color, Ren::ImageRWHandle normal,
                  Ren::ImageRWHandle spec, Ren::ImageRWHandle depth);
    void DrawTransparent(const FgContext &fg, Ren::ImageRWHandle color, Ren::ImageRWHandle normal,
                         Ren::ImageRWHandle spec, Ren::ImageRWHandle depth);

    void DrawTransparent_Simple(const FgContext &fg, Ren::BufferROHandle instances,
                                Ren::BufferROHandle instance_indices, Ren::BufferROHandle unif_shared_data,
                                Ren::BufferROHandle materials, Ren::BufferROHandle cells, Ren::BufferROHandle items,
                                Ren::BufferROHandle lights, Ren::BufferROHandle decals, Ren::ImageROHandle shad,
                                Ren::ImageRWHandle color, Ren::ImageRWHandle normal, Ren::ImageRWHandle spec,
                                Ren::ImageRWHandle depth, Ren::ImageROHandle ssao);
    void DrawTransparent_OIT_MomentBased(const FgContext &fg);
    void DrawTransparent_OIT_WeightedBlended(const FgContext &fg);

#if defined(REN_VK_BACKEND)
    void InitDescrSetLayout();
#endif
};
} // namespace Eng
