#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpRTReflections::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
                            Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
                            const DrawList &list, const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2,
                            const Ren::BufferRef &ndx_buf, const AccelerationStructureData *acc_struct_data,
                            const BindlessTextureData *bindless_tex, const Ren::BufferRef &materials_buf,
                            const char shared_data_buf_name[], const char depth_tex[], const char normal_tex[],
                            const Ren::Tex2DRef &dummy_black, const char ray_list_name[], const char indir_args_name[],
                            const char out_raylen_name[]) {
    render_flags_ = list.render_flags;
    view_state_ = view_state;
    draw_cam_ = &list.draw_cam;
    acc_struct_data_ = acc_struct_data;
    bindless_tex_ = bindless_tex;

    sobol_buf_ =
        builder.ReadBuffer(sobol_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    scrambling_tile_buf_ = builder.ReadBuffer(scrambling_tile_buf, Ren::eResState::ShaderResource,
                                              Ren::eStageBits::RayTracingShader, *this);
    ranking_tile_buf_ =
        builder.ReadBuffer(ranking_tile_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    geo_data_buf_ = builder.ReadBuffer(acc_struct_data->rt_geo_data_buf, Ren::eResState::ShaderResource,
                                       Ren::eStageBits::RayTracingShader, *this);
    materials_buf_ =
        builder.ReadBuffer(materials_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    vtx_buf1_ = builder.ReadBuffer(vtx_buf1, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    vtx_buf2_ = builder.ReadBuffer(vtx_buf2, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    ndx_buf_ = builder.ReadBuffer(ndx_buf, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::RayTracingShader, *this);
    depth_tex_ =
        builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    env_tex_ =
        builder.ReadTexture(list.env.env_map, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    ray_list_buf_ =
        builder.ReadBuffer(ray_list_name, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);

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
        builder.ReadTexture(dummy_black, Ren::eResState::ShaderResource, Ren::eStageBits::RayTracingShader, *this);

    out_raylen_tex_ = builder.WriteTexture(out_raylen_name, Ren::eResState::UnorderedAccess,
                                           Ren::eStageBits::RayTracingShader, *this);
}

void RpRTReflections::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
                            Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
                            const DrawList &list, const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2,
                            const Ren::BufferRef &ndx_buf, const AccelerationStructureData *acc_struct_data,
                            const BindlessTextureData *bindless_tex, const Ren::BufferRef &materials_buf,
                            const char shared_data_buf_name[], const char depth_tex[], const char normal_tex[],
                            const Ren::Tex2DRef &dummy_black, const char ray_list_name[], const char indir_args_name[],
                            const char out_refl_tex_name[], const char out_raylen_name[]) {
    Setup(builder, view_state, sobol_buf, scrambling_tile_buf, ranking_tile_buf, list, vtx_buf1, vtx_buf2, ndx_buf,
          acc_struct_data, bindless_tex, materials_buf, shared_data_buf_name, depth_tex, normal_tex, dummy_black,
          ray_list_name, indir_args_name, out_raylen_name);

    out_refl_tex_ = builder.WriteTexture(out_refl_tex_name, Ren::eResState::UnorderedAccess,
                                         Ren::eStageBits::RayTracingShader, *this);
}

void RpRTReflections::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
                            Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
                            const DrawList &list, const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2,
                            const Ren::BufferRef &ndx_buf, const AccelerationStructureData *acc_struct_data,
                            const BindlessTextureData *bindless_tex, const Ren::BufferRef &materials_buf,
                            const char shared_data_buf_name[], const char depth_tex[], const char normal_tex[],
                            const Ren::Tex2DRef &dummy_black, const char ray_list_name[], const char indir_args_name[],
                            Ren::WeakTex2DRef out_refl_tex, const char out_raylen_name[]) {
    Setup(builder, view_state, sobol_buf, scrambling_tile_buf, ranking_tile_buf, list, vtx_buf1, vtx_buf2, ndx_buf,
          acc_struct_data, bindless_tex, materials_buf, shared_data_buf_name, depth_tex, normal_tex, dummy_black,
          ray_list_name, indir_args_name, out_raylen_name);

    out_refl_tex_ =
        builder.WriteTexture(out_refl_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::RayTracingShader, *this);
}
