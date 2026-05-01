#pragma once

#include <Ren/Common.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
struct view_state_t;
class ShaderLoader;

class ExOpaque final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle instances;
        FgBufROHandle instance_indices;
        FgBufROHandle shared_data;
        FgBufROHandle materials;
        FgBufROHandle cells;
        FgBufROHandle items;
        FgBufROHandle lights;
        FgBufROHandle decals;
        FgImgROHandle shadow_depth;
        FgResRef lm_tex[4];
        FgImgROHandle ssao;
        FgImgROHandle brdf_lut;
        FgImgROHandle noise;
        FgImgROHandle cone_rt_lut;
        FgImgROHandle dummy_black;
        FgImgRWHandle color;
        FgImgRWHandle normal;
        FgImgRWHandle spec;
        FgImgRWHandle depth;
    };

    ExOpaque(Ren::ApiContext &api, const DrawList **p_list, const view_state_t *view_state,
             const BindlessTextureData *bindless_tex, const Args *args)
        : api_(api), p_list_(p_list), view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}
    ~ExOpaque() final;

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::VertexInputHandle draw_pass_vi_;
    Ren::RenderPassHandle rp_opaque_;

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
    void DrawOpaque(const FgContext &fg, Ren::ImageRWHandle color, Ren::ImageRWHandle normal, Ren::ImageRWHandle spec,
                    Ren::ImageRWHandle depth);

#if defined(REN_VK_BACKEND)
    void InitDescrSetLayout();
#endif
};
} // namespace Eng
