#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute(FgContext &ctx) {
    LazyInit(ctx.ren_ctx(), ctx.sh());
    if (ctx.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(ctx);
    } else {
        Execute_SWRT(ctx);
    }
}

void Eng::ExSampleLights::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (ctx.capabilities.hwrt) {
            pi_sample_lights_ = sh.LoadPipeline(subgroup_select("internal/sample_lights@HWRT.comp.glsl",
                                                                "internal/sample_lights@HWRT;NO_SUBGROUP.comp.glsl"));
        } else {
            pi_sample_lights_ = sh.LoadPipeline(
                subgroup_select("internal/sample_lights.comp.glsl", "internal/sample_lights@NO_SUBGROUP.comp.glsl"));
        }
        initialized_ = true;
    }
}

void Eng::ExSampleLights::Execute_SWRT(FgContext &ctx) {
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocBuf &random_seq_buf = ctx.AccessROBuffer(args_->random_seq);

    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &rt_blas_buf = ctx.AccessROBuffer(args_->swrt.rt_blas_buf);

    FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = ctx.AccessROBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = ctx.AccessROBuffer(args_->swrt.mesh_instances_buf);

    FgAllocTex &albedo_tex = ctx.AccessROTexture(args_->albedo_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &norm_tex = ctx.AccessROTexture(args_->norm_tex);
    FgAllocTex &spec_tex = ctx.AccessROTexture(args_->spec_tex);

    FgAllocTex &out_diffuse_tex = ctx.AccessRWTexture(args_->out_diffuse_tex);
    FgAllocTex &out_specular_tex = ctx.AccessRWTexture(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocBuf &nodes_buf = ctx.AccessROBuffer(args_->nodes_buf);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, *random_seq_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, *nodes_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, SampleLights::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, SampleLights::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::ALBEDO_TEX_SLOT, *albedo_tex.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, SampleLights::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::SPEC_TEX_SLOT, *spec_tex.ref},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_DIFFUSE_IMG_SLOT, *out_diffuse_tex.ref},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_SPECULAR_IMG_SLOT, *out_specular_tex.ref}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->ren_res[0] + SampleLights::GRP_SIZE_X - 1u) / SampleLights::GRP_SIZE_X,
                   (view_state_->ren_res[1] + SampleLights::GRP_SIZE_Y - 1u) / SampleLights::GRP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = uint32_t(lights_buf.desc.size / sizeof(light_item_t));
    uniform_params.frame_index = view_state_->frame_index;

    DispatchCompute(*pi_sample_lights_, grp_count, bindings, &uniform_params, sizeof(uniform_params), ctx.descr_alloc(),
                    ctx.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExSampleLights::Execute_HWRT(FgContext &ctx) { assert(false && "Not implemented!"); }
#endif