#include "RpSSRPrepare.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpSSRPrepare::Setup(RpBuilder &builder, const ViewState *view_state, const char temp_variance_mask_name[],
                         const char ray_counter_name[], const char denoised_refl_name[]) {
    view_state_ = view_state;

    { // tile metadata mask
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = 2 * ((view_state->scr_res[0] + 7) / 8) * ((view_state->scr_res[1] + 7) / 8) * sizeof(uint32_t);

        temp_variance_mask_buf_ = builder.WriteBuffer(temp_variance_mask_name, desc, Ren::eResState::CopyDst,
                                                      Ren::eStageBits::Transfer, *this);
    }
    { // ray counter
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = 4 * sizeof(uint32_t);

        ray_counter_buf_ =
            builder.WriteBuffer(ray_counter_name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }
    { // reflection color texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        denoised_refl_tex_ =
            builder.WriteTexture(denoised_refl_name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }
}

void RpSSRPrepare::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    initialized = true;
}
