#include "RpSSRPrepare.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpSSRPrepare::Setup(RpBuilder &builder, const ViewState *view_state, const char ray_counter_buf_name[],
                         const char raylen_tex_name[]) {
    view_state_ = view_state;

    { // ray counter
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = 6 * sizeof(uint32_t);

        ray_counter_buf_ =
            builder.WriteBuffer(ray_counter_buf_name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }
    { // Ray length texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR16F;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        raylen_tex_ =
            builder.WriteTexture(raylen_tex_name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }
}

void RpSSRPrepare::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    initialized = true;
}
