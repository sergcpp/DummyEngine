#include "RpSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../shaders/sample_lights_interface.h"

void Eng::RpSampleLights::Execute_HWRT(RpBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::RpSampleLights::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocBuf &random_seq_buf = builder.GetReadBuffer(pass_data_->random_seq);

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);

    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(pass_data_->swrt.textures_buf);

    RpAllocTex &albedo_tex = builder.GetReadTexture(pass_data_->albedo_tex);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &norm_tex = builder.GetReadTexture(pass_data_->norm_tex);
    RpAllocTex &spec_tex = builder.GetReadTexture(pass_data_->spec_tex);

    RpAllocTex &out_diffuse_tex = builder.GetWriteTexture(pass_data_->out_diffuse_tex);
    RpAllocTex &out_specular_tex = builder.GetWriteTexture(pass_data_->out_specular_tex);

    if (!pass_data_->lights_buf) {
        return;
    }

    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocBuf &nodes_buf = builder.GetReadBuffer(pass_data_->nodes_buf);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (!random_seq_buf.tbos[0] || random_seq_buf.tbos[0]->params().size != random_seq_buf.ref->size()) {
        random_seq_buf.tbos[0] = builder.ctx().CreateTexture1D(
            "Random Seq Buf TBO", random_seq_buf.ref, Ren::eTexFormat::RawR32UI, 0, random_seq_buf.ref->size());
    }
    if (!lights_buf.tbos[0] || lights_buf.tbos[0]->params().size != lights_buf.ref->size()) {
        lights_buf.tbos[0] = builder.ctx().CreateTexture1D("Stoch Lights Buf TBO", lights_buf.ref,
                                                           Ren::eTexFormat::RawRGBA32F, 0, lights_buf.ref->size());
    }
    if (!nodes_buf.tbos[0] || nodes_buf.tbos[0]->params().size != nodes_buf.ref->size()) {
        nodes_buf.tbos[0] = builder.ctx().CreateTexture1D("Stoch Lights Nodes Buf TBO", nodes_buf.ref,
                                                          Ren::eTexFormat::RawRGBA32F, 0, nodes_buf.ref->size());
    }

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] = builder.ctx().CreateTexture1D("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RawRGBA32F,
                                                         0, vtx_buf1.ref->size());
    }

    if (!ndx_buf.tbos[0] || ndx_buf.tbos[0]->params().size != ndx_buf.ref->size()) {
        ndx_buf.tbos[0] = builder.ctx().CreateTexture1D("Index Buf TBO", ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                                        ndx_buf.ref->size());
    }

    if (!prim_ndx_buf.tbos[0] || prim_ndx_buf.tbos[0]->params().size != prim_ndx_buf.ref->size()) {
        prim_ndx_buf.tbos[0] = builder.ctx().CreateTexture1D("Prim Ndx TBO", prim_ndx_buf.ref,
                                                             Ren::eTexFormat::RawR32UI, 0, prim_ndx_buf.ref->size());
    }

    if (!rt_blas_buf.tbos[0] || rt_blas_buf.tbos[0]->params().size != rt_blas_buf.ref->size()) {
        rt_blas_buf.tbos[0] = builder.ctx().CreateTexture1D("RT BLAS TBO", rt_blas_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                                            0, rt_blas_buf.ref->size());
    }

    if (!rt_tlas_buf.tbos[0] || rt_tlas_buf.tbos[0]->params().size != rt_tlas_buf.ref->size()) {
        rt_tlas_buf.tbos[0] = builder.ctx().CreateTexture1D("RT TLAS TBO", rt_tlas_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                                            0, rt_tlas_buf.ref->size());
    }

    if (!mesh_instances_buf.tbos[0] || mesh_instances_buf.tbos[0]->params().size != mesh_instances_buf.ref->size()) {
        mesh_instances_buf.tbos[0] =
            builder.ctx().CreateTexture1D("Mesh Instances TBO", mesh_instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                          mesh_instances_buf.ref->size());
    }

    if (!meshes_buf.tbos[0] || meshes_buf.tbos[0]->params().size != meshes_buf.ref->size()) {
        meshes_buf.tbos[0] = builder.ctx().CreateTexture1D("Meshes TBO", meshes_buf.ref, Ren::eTexFormat::RawRG32UI, 0,
                                                           meshes_buf.ref->size());
    }

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, *random_seq_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, *lights_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, *nodes_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, SampleLights::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, SampleLights::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::UTBuf, SampleLights::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DSampled, SampleLights::ALBEDO_TEX_SLOT, *albedo_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, SampleLights::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::Tex2DSampled, SampleLights::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, SampleLights::SPEC_TEX_SLOT, *spec_tex.ref},
        {Ren::eBindTarget::Image2D, SampleLights::OUT_DIFFUSE_IMG_SLOT, *out_diffuse_tex.ref},
        {Ren::eBindTarget::Image2D, SampleLights::OUT_SPECULAR_IMG_SLOT, *out_specular_tex.ref}};

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SampleLights::LOCAL_GROUP_SIZE_X - 1u) / SampleLights::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SampleLights::LOCAL_GROUP_SIZE_Y - 1u) / SampleLights::LOCAL_GROUP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.lights_count = uint32_t(lights_buf.desc.size / sizeof(LightItem));
    uniform_params.frame_index = view_state_->frame_index;

    Ren::DispatchCompute(pi_sample_lights_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                         ctx.default_descr_alloc(), ctx.log());
}
