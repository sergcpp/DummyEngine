#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDebugRT::Setup(RpBuilder &builder, const ViewState *view_state, const DrawList &list,
                      const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
                      const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
                      const Ren::BufferRef &materials_buf, const char shared_data_buf_name[],
                      const Ren::Tex2DRef &dummy_black, const char output_tex_name[]) {
    render_flags_ = list.render_flags;
    view_state_ = view_state;
    draw_cam_ = &list.draw_cam;
    acc_struct_data_ = acc_struct_data;
    bindless_tex_ = bindless_tex;

    geo_data_buf_ = builder.ReadBuffer(acc_struct_data->rt_geo_data_buf, Ren::eResState::ShaderResource,
                                       Ren::eStageBits::RayTracingShader, *this);
    materials_buf_ =
        builder.ReadBuffer(materials_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    vtx_buf1_ = builder.ReadBuffer(vtx_buf1, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    vtx_buf2_ = builder.ReadBuffer(vtx_buf2, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    ndx_buf_ = builder.ReadBuffer(ndx_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::RayTracingShader, *this);
    env_tex_ =
        builder.ReadTexture(list.env.env_map, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    if (list.env.lm_direct) {
        lm_tex_[0] = builder.ReadTexture(list.env.lm_direct, Ren::eResState::ShaderResource,
                                         Ren::eStageBits::RayTracingShader, *this);
    } else {
        lm_tex_[0] = {};
    }

    for (int i = 0; i < 4; ++i) {
        if (list.env.lm_indir_sh[i]) {
            lm_tex_[i + 1] = builder.ReadTexture(list.env.lm_indir_sh[i], Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::RayTracingShader, *this);
        } else {
            lm_tex_[i + 1] = {};
        }
    }

    dummy_black_ =
        builder.ReadTexture(dummy_black, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    if (output_tex_name) {
        output_tex_ = builder.WriteTexture(output_tex_name, Ren::eResState::UnorderedAccess,
                                           Ren::eStageBits::RayTracingShader, *this);
    } else {
        output_tex_ = {};
    }
}
