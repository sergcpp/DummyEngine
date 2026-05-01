#include "Context.h"

#include <algorithm>
#include <istream>

#include "Framebuffer.h"
#include "ResizableBuffer.h"

#if defined(REN_VK_BACKEND)
#include "Vk/VKCtx.h"
#elif defined(REN_GL_BACKEND)
#include "Gl/GLCtx.h"
#endif

const char *Ren::Version() { return "v0.1.0-unknown-commit"; }

Ren::MeshHandle Ren::Context::CreateMesh(Ren::String name, std::istream &data,
                                         const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                                         ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf,
                                         ResizableBuffer &skin_vertex_buf, ResizableBuffer &delta_buf) {
    const MeshHandle ret = meshes_.Emplace();

    const auto &[mesh_main, mesh_cold] = meshes_[ret];
    if (!Mesh_Init(*api_, mesh_main, mesh_cold, name, data, on_mat_load, vertex_buf1, vertex_buf2, index_buf,
                   skin_vertex_buf, delta_buf, log_)) {
        meshes_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::MeshHandle Ren::Context::CreateMesh(Ren::String name, std::istream &data,
                                         const material_load_callback &on_mat_load) {
    return CreateMesh(name, data, on_mat_load, *default_vertex_buf1_, *default_vertex_buf2_, *default_indices_buf_,
                      *default_skin_vertex_buf_, *default_delta_vertex_buf_);
}

void Ren::Context::ReleaseMesh(const MeshHandle handle) {
    if (!handle) {
        return;
    }
    const auto &[mesh_main, mesh_cold] = meshes_[handle];
    Mesh_Destroy(buffers_, mesh_main, mesh_cold);
    meshes_.Erase(handle);
}

void Ren::Context::ReleaseMeshes() {
    if (meshes_.empty()) {
        return;
    }
    log_->Error("----------REMAINING MESHES---------");
    for (const auto &m : meshes_) {
        log_->Error("%s", m.second.name.c_str());
        Mesh_Destroy(buffers_, m.first, m.second);
    }
    meshes_.Clear();
    log_->Error("-----------------------------------");
}

Ren::MaterialHandle Ren::Context::CreateMaterial(Ren::String name, const Bitmask<eMatFlags> flags,
                                                 Span<const PipelineHandle> pipelines, Span<const ImageHandle> textures,
                                                 Span<const SamplerHandle> samplers, Span<const Vec4f> params) {
    const MaterialHandle ret = materials_.Emplace();

    const auto &[material_main, material_cold] = materials_[ret];
    if (!Material_Init(material_main, material_cold, name, flags, pipelines, textures, samplers, params, log_)) {
        materials_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::MaterialHandle Ren::Context::CreateMaterial(Ren::String name, std::string_view mat_src,
                                                 const pipelines_load_callback &on_pipes_load,
                                                 const texture_load_callback &on_tex_load,
                                                 const sampler_load_callback &on_sampler_load) {
    const MaterialHandle ret = materials_.Emplace();

    const auto &[material_main, material_cold] = materials_[ret];
    if (!Material_Init(material_main, material_cold, name, mat_src, on_pipes_load, on_tex_load, on_sampler_load,
                       log_)) {
        materials_.Erase(ret);
        return {};
    }
    return ret;
}

void Ren::Context::ReleaseMaterial(const MaterialHandle handle) { materials_.Erase(handle); }

void Ren::Context::ReleaseMaterials() {
    if (materials_.empty()) {
        return;
    }
    log_->Error("---------REMAINING MATERIALS--------");
    for (const auto &m : materials_) {
        log_->Error("%s", m.second.name.c_str());
    }
    materials_.Clear();
    log_->Error("-----------------------------------");
}

#if defined(REN_GL_BACKEND)
Ren::ShaderHandle Ren::Context::CreateShader(const Ren::String &name, std::string_view shader_src,
                                             const eShaderType type) {
    ShaderHandle ret = shaders_.Emplace();

    const auto &[shader_main, shader_cold] = shaders_[ret];
    if (!Shader_Init(*api_, shader_main, shader_cold, shader_src, name, type, log_)) {
        shaders_.Erase(ret);
        return {};
    }
    return ret;
}
#endif

Ren::ShaderHandle Ren::Context::CreateShader(const Ren::String &name, Span<const uint8_t> spirv_data,
                                             const eShaderType type) {
    const ShaderHandle ret = shaders_.Emplace();

    const auto &[shader_main, shader_cold] = shaders_[ret];
    if (!Shader_Init(*api_, shader_main, shader_cold, spirv_data, name, type, log_)) {
        shaders_.Erase(ret);
        return {};
    }
    return ret;
}

void Ren::Context::ReleaseShader(const ShaderHandle handle) {
    if (!handle) {
        return;
    }
    const auto &[sh_main, sh_cold] = shaders_[handle];
    Shader_Destroy(*api_, sh_main, sh_cold);
    shaders_.Erase(handle);
}

void Ren::Context::ReleaseShaders() {
    if (shaders_.empty()) {
        return;
    }
    log_->Error("---------REMAINING SHADERS---------");
    for (const auto &sh : shaders_) {
        log_->Error("%s", sh.second.name.c_str());
        Shader_Destroy(*api_, sh.first, sh.second);
    }
    shaders_.Clear();
    log_->Error("-----------------------------------");
}

Ren::ProgramHandle Ren::Context::CreateProgram(const ShaderROHandle vs, const ShaderROHandle fs,
                                               const ShaderROHandle tcs, const ShaderROHandle tes,
                                               const ShaderROHandle gs) {
    const ProgramHandle ret = programs_.Emplace();

    const auto &[prog_main, prog_cold] = programs_[ret];
    if (!Program_Init(*api_, shaders_, prog_main, prog_cold, vs, fs, tcs, tes, gs, log_)) {
        programs_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::ProgramHandle Ren::Context::CreateProgram(const ShaderROHandle cs) {
    const ProgramHandle ret = programs_.Emplace();

    const auto &[prog_main, prog_cold] = programs_[ret];
    if (!Program_Init(*api_, shaders_, prog_main, prog_cold, cs, log_)) {
        programs_.Erase(ret);
        return {};
    }
    return ret;
}

#if defined(REN_VK_BACKEND)
Ren::ProgramHandle Ren::Context::CreateProgram2(const ShaderROHandle rgs, const ShaderROHandle chs,
                                                const ShaderROHandle ahs, const ShaderROHandle ms,
                                                const ShaderROHandle is) {
    const ProgramHandle ret = programs_.Emplace();

    const auto &[prog_main, prog_cold] = programs_[ret];
    if (!Program_Init2(*api_, shaders_, prog_main, prog_cold, rgs, chs, ahs, ms, is, log_)) {
        programs_.Erase(ret);
        return {};
    }
    return ret;
}
#endif

void Ren::Context::ReleaseProgram(const ProgramHandle handle) {
    if (!handle) {
        return;
    }
    const auto &[prog_main, prog_cold] = programs_[handle];
    Program_Destroy(*api_, prog_main, prog_cold);
    programs_.Erase(handle);
}

void Ren::Context::ReleasePrograms() {
    if (programs_.empty()) {
        return;
    }
    log_->Error("---------REMAINING PROGRAMS--------");
    for (const auto &pr : programs_) {
        Program_Destroy(*api_, pr.first, pr.second);
    }
    programs_.Clear();
    log_->Error("-----------------------------------");
}

Ren::VertexInputHandle Ren::Context::CreateVertexInput(Span<const VtxAttribDesc> attribs) {
    const VertexInputHandle ret = vtx_inputs_.Emplace();

    VertexInput &vtx_input = vtx_inputs_[ret];
    if (!VertexInput_Init(vtx_input, attribs)) {
        vtx_inputs_.Erase(ret);
        return {};
    }
    return ret;
}

void Ren::Context::ReleaseVertexInput(const VertexInputHandle handle) {
    if (!handle) {
        return;
    }
    VertexInput &vtx_input = vtx_inputs_[handle];
    VertexInput_Destroy(vtx_input);
    vtx_inputs_.Erase(handle);
}

void Ren::Context::ReleaseVertexInputs() {
    if (vtx_inputs_.empty()) {
        return;
    }
    log_->Error("--------REMAINING VTX INPUTS-------");
    for (auto &vi : vtx_inputs_) {
        log_->Error("%i attribs", int(vi.attribs.size()));
        VertexInput_Destroy(vi);
    }
    vtx_inputs_.Clear();
    log_->Error("-----------------------------------");
}

Ren::RenderPassHandle Ren::Context::CreateRenderPass(const RenderTargetInfo &depth_rt,
                                                     Span<const RenderTargetInfo> color_rts) {
    const RenderPassHandle ret = render_passes_.Emplace();

    RenderPass &rp = render_passes_[ret];
    if (!RenderPass_Init(*api_, rp, depth_rt, color_rts, log_)) {
        return {};
    }
    return ret;
}

Ren::RenderPassHandle Ren::Context::CreateRenderPass(const RenderTarget &depth_rt, Span<const RenderTarget> color_rts) {
    Ren::SmallVector<Ren::RenderTargetInfo, 4> color_infos;
    Ren::RenderTargetInfo depth_info;
    { //
        for (int i = 0; i < color_rts.size(); ++i) {
            const auto &[img_main, img_cold] = images_[color_rts[i].img];
            const Ren::eImageLayout layout = ImageLayoutForState(img_main.resource_state);
            color_infos.emplace_back(img_cold.params.format, uint8_t(img_cold.params.samples), layout,
                                     color_rts[i].load, color_rts[i].store, color_rts[i].stencil_load,
                                     color_rts[i].stencil_store);
        }
        if (depth_rt) {
            const auto &[img_main, img_cold] = images_[depth_rt.img];
            const Ren::eImageLayout layout = ImageLayoutForState(img_main.resource_state);
            depth_info = {img_cold.params.format,
                          uint8_t(img_cold.params.samples),
                          layout,
                          depth_rt.load,
                          depth_rt.store,
                          depth_rt.stencil_load,
                          depth_rt.stencil_store};
        }
    }
    return CreateRenderPass(depth_info, color_infos);
}

void Ren::Context::ReleaseRenderPass(const RenderPassHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    RenderPass &rp = render_passes_[handle];
    if (immediately) {
        RenderPass_DestroyImmediately(*api_, rp);
    } else {
        RenderPass_Destroy(*api_, rp);
    }
    render_passes_.Erase(handle);
}

void Ren::Context::ReleaseRenderPasses() {
    if (render_passes_.empty()) {
        return;
    }
    log_->Error("------REMAINING RENDER PASSES------");
    for (auto it = render_passes_.begin(); it != render_passes_.end(); ++it) {
        // TODO: Report details
        RenderPass_Destroy(*api_, *it);
    }
    render_passes_.Clear();
    log_->Error("-----------------------------------");
}

Ren::PipelineHandle Ren::Context::CreatePipeline(const ProgramROHandle prog, const int subgroup_size) {
    const PipelineHandle ret = pipelines_.Emplace();

    const auto &[pi_main, pi_cold] = pipelines_[ret];
    if (!Pipeline_Init(*api_, shaders_, programs_, buffers_, pi_main, pi_cold, prog, log_)) {
        pipelines_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::PipelineHandle Ren::Context::CreatePipeline(const RastState &rast_state, const ProgramROHandle prog,
                                                 const VertexInputROHandle vtx_input,
                                                 const RenderPassROHandle render_pass, const uint32_t subpass_index) {
    const PipelineHandle ret = pipelines_.Emplace();

    const auto &[pi_main, pi_cold] = pipelines_[ret];
    if (!Pipeline_Init(*api_, storages_, pi_main, pi_cold, rast_state, prog, vtx_input, render_pass, subpass_index,
                       log_)) {
        pipelines_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::PipelineHandle Ren::Context::CreatePipeline(PipelineMain &&_pi_main, PipelineCold &&_pi_cold) {
    const PipelineHandle ret = pipelines_.Emplace();
    const auto &[pi_main, pi_cold] = pipelines_[ret];
    pi_main = std::move(_pi_main);
    pi_cold = std::move(_pi_cold);
    return ret;
}

void Ren::Context::ReleasePipeline(const PipelineHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    const auto &[pi_main, pi_cold] = pipelines_[handle];
    if (immediately) {
        Pipeline_DestroyImmediately(*api_, buffers_, pi_main, pi_cold);
    } else {
        Pipeline_Destroy(*api_, buffers_, pi_main, pi_cold);
    }
    pipelines_.Erase(handle);
}

void Ren::Context::ReleasePipelines() {
    if (pipelines_.empty()) {
        return;
    }
    log_->Error("--------REMAINING PIPELINES--------");
    for (const auto &pi : pipelines_) {
        Pipeline_Destroy(*api_, buffers_, pi.first, pi.second);
    }
    pipelines_.Clear();
    log_->Error("-----------------------------------");
}

Ren::ImageHandle Ren::Context::CreateImage(const String &name, Span<const uint8_t> data, const ImgParams &p,
                                           MemAllocators *mem_allocs) {
    const ImageHandle ret = images_.Emplace();

    const auto &[img_main, img_cold] = images_[ret];
    if (!Image_Init(*api_, img_main, img_cold, name, p, data, mem_allocs, log_)) {
        images_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::ImageHandle Ren::Context::CreateImage(const String &name, Span<const uint8_t> data[6], const ImgParams &p,
                                           MemAllocators *mem_allocs) {
    const ImageHandle ret = images_.Emplace();

    const auto &[img_main, img_cold] = images_[ret];
    if (!Image_Init(*api_, img_main, img_cold, name, p, data, mem_allocs, log_)) {
        images_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::ImageHandle Ren::Context::CreateImage(const String &name, const ImgParams &p, const ImageMain &_img_main,
                                           MemAllocation &&alloc) {
    const ImageHandle ret = images_.Emplace();

    const auto &[img_main, img_cold] = images_[ret];
    img_main = _img_main;
    if (!Image_Init(*api_, img_cold, name, p, std::move(alloc), log_)) {
        images_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::ImageHandle Ren::Context::CreateImage(const ImageHandle src, const ImgParams &p, MemAllocators *mem_allocs,
                                           CommandBuffer cmd_buf) {
    const ImageHandle ret = images_.Emplace();

    const auto &[src_main, src_cold] = images_[src];
    const auto &[img_main, img_cold] = images_[ret];
    if (!Image_Init(*api_, img_main, img_cold, src_cold.name, p, {}, mem_allocs, log_)) {
        images_.Erase(ret);
        return {};
    }

    if (src_cold.params.format == img_cold.params.format) {
        // copy data from src texture
        int src_mip = 0, dst_mip = 0;
        while (std::max(src_cold.params.w >> src_mip, 1) != std::max(img_cold.params.w >> dst_mip, 1) ||
               std::max(src_cold.params.h >> src_mip, 1) != std::max(img_cold.params.h >> dst_mip, 1)) {
            if (std::max(src_cold.params.w >> src_mip, 1) > std::max(img_cold.params.w >> dst_mip, 1) ||
                std::max(src_cold.params.h >> src_mip, 1) > std::max(img_cold.params.h >> dst_mip, 1)) {
                ++src_mip;
            } else {
                ++dst_mip;
            }
        }

        const TransitionInfo transitions[] = {{src, eResState::CopySrc}, {ret, eResState::CopyDst}};
        TransitionResourceStates(*api_, storages_, cmd_buf, AllStages, AllStages, transitions);

        for (; src_mip < int(src_cold.params.mip_count) && dst_mip < img_cold.params.mip_count; ++src_mip, ++dst_mip) {
            Image_CmdCopyToImage(
                *api_, cmd_buf, src_main, src_cold, src_mip, Vec3i{0}, img_main, img_cold, dst_mip, Vec3i{0}, 0,
                Vec3i{std::max(img_cold.params.w >> dst_mip, 1), std::max(img_cold.params.h >> dst_mip, 1), 1});

#ifdef TEX_VERBOSE_LOGGING
            log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif
        }
    }

    return ret;
}

void Ren::Context::ReleaseImage(const ImageHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    const auto &[img_main, img_cold] = images_[handle];
    if (immediately) {
        Image_DestroyImmediately(*api_, img_main, img_cold);
    } else {
        Image_Destroy(*api_, img_main, img_cold);
    }
    images_.Erase(handle);
}

int Ren::Context::CreateImageView(const ImageHandle handle, const eFormat format, const int mip_level,
                                  const int mip_count, const int base_layer, const int layer_count) {
    const auto &[img_main, img_cold] = images_[handle];
    return Image_AddView(*api_, img_main, img_cold, format, mip_level, mip_count, base_layer, layer_count);
}

void Ren::Context::CmdClearImage(const ImageHandle handle, const ClearColor &col, const CommandBuffer cmd_buf) {
    const auto &[img_main, img_cold] = images_[handle];
    Image_CmdClear(*api_, img_main, img_cold, col, cmd_buf);
}

void Ren::Context::CmdCopyImageToBuffer(const ImageROHandle img, const BufferRWHandle buf, const CommandBuffer cmd_buf,
                                        const uint32_t data_off) {
    const auto &[img_main, img_cold] = images_[img];
    const auto &[buf_main, buf_cold] = buffers_[buf];
    Image_CmdCopyToBuffer(*api_, img_main, img_cold, buf_main, buf_cold, cmd_buf, data_off);
}

void Ren::Context::CmdCopyImageToImage(const CommandBuffer cmd_buf, const ImageROHandle src, const uint32_t src_level,
                                       const Vec3i &src_offset, const ImageRWHandle dst, const uint32_t dst_level,
                                       const Vec3i &dst_offset, const uint32_t dst_face, const Vec3i &size) {
    const auto &[src_main, src_cold] = images_[src];
    const auto &[dst_main, dst_cold] = images_[dst];
    Image_CmdCopyToImage(*api_, cmd_buf, src_main, src_cold, src_level, src_offset, dst_main, dst_cold, dst_level,
                         dst_offset, dst_face, size);
}

void Ren::Context::ReleaseImages() {
    if (images_.empty()) {
        return;
    }
    log_->Error("----------REMAINING IMAGES---------");
    for (const auto &img : images_) {
        log_->Error("%s", img.second.name.c_str());
        Image_Destroy(*api_, img.first, img.second);
    }
    images_.Clear();
    log_->Error("-----------------------------------");
}

Ren::FramebufferHandle Ren::Context::CreateFramebuffer(const RenderPassROHandle render_pass,
                                                       const FramebufferAttachment &depth,
                                                       const FramebufferAttachment &stencil,
                                                       Span<const FramebufferAttachment> color_attachments) {
    const FramebufferHandle ret = framebuffers_.Emplace();

    const auto &[fb_main, fb_cold] = framebuffers_[ret];
    if (!Framebuffer_Init(*api_, fb_main, fb_cold, storages_, render_pass, depth, stencil, color_attachments, log_)) {
        framebuffers_.Erase(ret);
        return {};
    }
    return ret;
}

void Ren::Context::ReleaseFramebuffer(const FramebufferHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    const auto &[fb_main, fb_cold] = framebuffers_[handle];
    if (immediately) {
        Framebuffer_DestroyImmediately(*api_, fb_main, fb_cold);
    } else {
        Framebuffer_Destroy(*api_, fb_main, fb_cold);
    }
    framebuffers_.Erase(handle);
}

void Ren::Context::ReleaseFramebuffers() {
    if (framebuffers_.empty()) {
        return;
    }
    log_->Error("-------REMAINING FRAMEBUFFERS------");
    for (const auto &fb : framebuffers_) {
        // TODO: Report details
        Framebuffer_Destroy(*api_, fb.first, fb.second);
    }
    framebuffers_.Clear();
    log_->Error("-----------------------------------");
}

Ren::AccStructHandle Ren::Context::CreateAccStruct() { return acc_structs_.Emplace(); }

void Ren::Context::ReleaseAccStruct(const AccStructHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    const auto &[acc_main, acc_cold] = acc_structs_[handle];
    if (immediately) {
        AccStruct_DestroyImmediately(*api_, acc_main, acc_cold);
    } else {
        AccStruct_Destroy(*api_, acc_main, acc_cold);
    }
    acc_structs_.Erase(handle);
}

void Ren::Context::ReleaseAccStructs() {
    if (acc_structs_.empty()) {
        return;
    }
    log_->Error("-------REMAINING ACC STRUCTS-------");
    for (const auto &acc : acc_structs_) {
        AccStruct_Destroy(*api_, acc.first, acc.second);
    }
    acc_structs_.Clear();
    log_->Error("-----------------------------------");
}

Ren::ImageRegionHandle Ren::Context::CreateImageRegion(String name, Span<const uint8_t> data, const ImgParams &p,
                                                       CommandBuffer cmd_buf) {
    const ImageRegionHandle ret = image_regions_.Emplace();

    const auto &[reg_main, reg_cold] = image_regions_[ret];
    if (!ImageRegion_Init(reg_main, reg_cold, name, data, p, cmd_buf, &image_atlas_, log_)) {
        image_regions_.Erase(ret);
        return {};
    }
    return ret;
}

Ren::ImageRegionHandle Ren::Context::CreateImageRegion(String name, const BufferMain &sbuf, const int data_off,
                                                       const int data_len, const ImgParams &p, CommandBuffer cmd_buf) {
    const ImageRegionHandle ret = image_regions_.Emplace();

    const auto &[reg_main, reg_cold] = image_regions_[ret];
    if (!ImageRegion_Init(reg_main, reg_cold, name, sbuf, data_off, data_len, p, cmd_buf, &image_atlas_, log_)) {
        image_regions_.Erase(ret);
        return {};
    }
    return ret;
}

void Ren::Context::ReleaseImageRegion(const ImageRegionHandle handle) {
    if (!handle) {
        return;
    }
    image_regions_.Erase(handle);
}

void Ren::Context::ReleaseImageRegions() {
    if (image_regions_.empty()) {
        return;
    }
    /*log_->Error("-------REMAINING TEX REGIONS-------");
    for (const auto &reg : image_regions_) {
        log_->Error("%s", reg.second.name.c_str());
    }
    log_->Error("-----------------------------------");*/
    image_regions_.Clear();
}

Ren::SamplerHandle Ren::Context::CreateSampler(const SamplingParams params) {
    SamplerHandle ret = samplers_.Emplace();

    Sampler &sampler = samplers_[ret];

    if (!Sampler_Init(*api_, sampler, params)) {
        samplers_.Erase(ret);
        ret = {};
    }

    return ret;
}

void Ren::Context::ReleaseSampler(const SamplerHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    Sampler &sampler = samplers_[handle];
    if (immediately) {
        Sampler_DestroyImmediately(*api_, sampler);
    } else {
        Sampler_Destroy(*api_, sampler);
    }
    samplers_.Erase(handle);
}

void Ren::Context::ReleaseSamplers() {
    if (samplers_.empty()) {
        return;
    }
    log_->Error("--------REMAINING SAMPLERS---------");
    for (auto it = samplers_.begin(); it != samplers_.end(); ++it) {
        // TODO: Report details
        Sampler_Destroy(*api_, *it);
    }
    samplers_.Clear();
    log_->Error("-----------------------------------");
}

Ren::AnimSeqHandle Ren::Context::CreateAnimSequence(const String &name, std::istream &data) {
    AnimSeqHandle ret = anims_.Emplace();

    const auto &[anim_main, anim_cold] = anims_[ret];

    if (!AnimSeq_Init(anim_main, anim_cold, name, data, log_)) {
        anims_.Erase(ret);
        ret = {};
    }

    return ret;
}

void Ren::Context::ReleaseAnimSequence(const AnimSeqHandle handle) { anims_.Erase(handle); }

void Ren::Context::ReleaseAnimSequences() {
    if (anims_.empty()) {
        return;
    }
    log_->Error("----------REMAINING ANIMS----------");
    for (const auto &a : anims_) {
        log_->Error("%s", a.second.name.c_str());
    }
    anims_.Clear();
    log_->Error("-----------------------------------");
}

Ren::BufferHandle Ren::Context::CreateBuffer(const String &name, const eBufType type, const uint32_t initial_size,
                                             const uint32_t size_alignment, MemAllocators *mem_allocs) {
    BufferHandle ret = buffers_.Emplace();

    const auto &[buf_main, buf_cold] = buffers_[ret];

    if (!Buffer_Init(*api_, buf_main, buf_cold, name, type, initial_size, log_, size_alignment, mem_allocs)) {
        buffers_.Erase(ret);
        ret = {};
    }

    return ret;
}

Ren::BufferHandle Ren::Context::CreateBuffer(const String &name, const eBufType type, const BufferMain &_buf_main,
                                             MemAllocation &&alloc, const uint32_t initial_size,
                                             const uint32_t size_alignment) {
    BufferHandle ret = buffers_.Emplace();

    const auto &[buf_main, buf_cold] = buffers_[ret];

    buf_main = _buf_main;
    if (!Buffer_Init(*api_, buf_cold, name, type, std::move(alloc), initial_size, log_, size_alignment)) {
        buffers_.Erase(ret);
        ret = {};
    }

    return ret;
}

bool Ren::Context::ResizeBuffer(const BufferHandle handle, const uint32_t new_size, const bool keep_content,
                                const bool release_immediately) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    return Buffer_Resize(*api_, buf_main, buf_cold, new_size, log_, keep_content, release_immediately);
}

int Ren::Context::FindOrCreateBufferView(const BufferHandle handle, const eFormat format) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    for (int i = 0; i < int(buf_main.views.size()); ++i) {
        if (buf_main.views[i].first == format) {
            return i;
        }
    }
    return Buffer_AddView(*api_, buf_main, buf_cold, format);
}

uint8_t *Ren::Context::MapBufferRange(const BufferHandle handle, const uint32_t offset, const uint32_t size,
                                      const bool persistent) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    return Buffer_MapRange(*api_, buf_main, buf_cold, offset, size, persistent);
}

uint8_t *Ren::Context::MapBuffer(const BufferHandle handle, const bool persistent) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    return Buffer_MapRange(*api_, buf_main, buf_cold, 0, buf_cold.size, persistent);
}

void Ren::Context::UnmapBuffer(const BufferHandle handle) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    Buffer_Unmap(*api_, buf_main, buf_cold);
}

Ren::SubAllocation Ren::Context::AllocBufferSubRegion(const BufferHandle handle, const uint32_t req_size,
                                                      const uint32_t req_alignment, std::string_view tag,
                                                      const BufferMain *init_buf, CommandBuffer cmd_buf,
                                                      const uint32_t init_off) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    return Buffer_AllocSubRegion(*api_, buf_main, buf_cold, req_size, req_alignment, tag, log_, init_buf, cmd_buf,
                                 init_off);
}

bool Ren::Context::FreeBufferSubRegion(BufferHandle handle, SubAllocation alloc) {
    const auto &[buf_main, buf_cold] = buffers_[handle];
    return Buffer_FreeSubRegion(buf_cold, alloc);
}

void Ren::Context::ReleaseBuffer(const BufferHandle handle, const bool immediately) {
    if (!handle) {
        return;
    }
    const auto &[buf_main, buf_cold] = buffers_[handle];
    if (immediately) {
        Buffer_DestroyImmediately(*api_, buf_main, buf_cold);
    } else {
        Buffer_Destroy(*api_, buf_main, buf_cold);
    }
    buffers_.Erase(handle);
}

void Ren::Context::ReleaseBuffers() {
    if (buffers_.empty()) {
        return;
    }
    log_->Error("---------REMAINING BUFFERS--------");
    for (const auto &buf : buffers_) {
        log_->Error("%s\t: %u", buf.second.name.c_str(), buf.second.size);
        Buffer_Destroy(*api_, buf.first, buf.second);
    }
    buffers_.Clear();
    log_->Error("-----------------------------------");
}

void Ren::Context::InitDefaultBuffers() {
    assert(!default_vertex_buf1_);
    assert(!default_vertex_buf2_);
    assert(!default_indices_buf_);
    assert(!default_skin_vertex_buf_);
    assert(!default_delta_vertex_buf_);

    default_vertex_buf1_ =
        std::make_unique<ResizableBuffer>(*api_, "Default Vtx Buf 1", eBufType::VertexAttribs, 16, buffers_);
    default_vertex_buf1_->Resize(1 * 1024 * 1024, log_);
    default_vertex_buf1_->AddView(eFormat::RGBA32F);

    default_vertex_buf2_ =
        std::make_unique<ResizableBuffer>(*api_, "Default Vtx Buf 2", eBufType::VertexAttribs, 16, buffers_);
    default_vertex_buf2_->Resize(1 * 1024 * 1024, log_);
    default_vertex_buf2_->AddView(eFormat::RGBA32UI);

    default_indices_buf_ =
        std::make_unique<ResizableBuffer>(*api_, "Default Ndx Buf", eBufType::VertexIndices, 4, buffers_);
    default_indices_buf_->Resize(1 * 1024 * 1024, log_);
    default_indices_buf_->AddView(eFormat::R32UI);

    default_skin_vertex_buf_ =
        std::make_unique<ResizableBuffer>(*api_, "Default Skin Vtx Buf", eBufType::VertexAttribs, 16, buffers_);
    default_skin_vertex_buf_->Resize(1 * 1024 * 1024, log_);

    default_delta_vertex_buf_ =
        std::make_unique<ResizableBuffer>(*api_, "Default Delta Vtx Buf", eBufType::VertexAttribs, 16, buffers_);
    default_delta_vertex_buf_->Resize(1 * 1024 * 1024, log_);
}

void Ren::Context::ReleaseDefaultBuffers() {
    default_vertex_buf1_ = {};
    default_vertex_buf2_ = {};
    default_indices_buf_ = {};
    default_skin_vertex_buf_ = {};
    default_delta_vertex_buf_ = {};
}

void Ren::Context::ReleaseAll() {
    ReleaseDefaultBuffers();

    ReleaseMeshes();
    ReleaseAnimSequences();
    ReleaseMaterials();
    ReleasePrograms();
    ReleaseShaders();
    ReleaseImages();
    ReleaseImageRegions();
    ReleaseBuffers();
    ReleaseVertexInputs();
    ReleaseRenderPasses();
    ReleaseSamplers();
    ReleaseAccStructs();

    image_atlas_ = {};
}

Ren::DescrMultiPoolAlloc &Ren::Context::default_descr_alloc() const {
    return *default_descr_alloc_[api_->backend_frame];
}

int Ren::Context::backend_frame() const { return api_->backend_frame; }

int Ren::Context::active_present_image() const { return api_->active_present_image; }

Ren::ImageHandle Ren::Context::backbuffer_img() const {
    return api_->present_image_handles[api_->active_present_image];
}

Ren::StageBufRef::StageBufRef(Context &_ctx, const BufferHandle _buf, SyncFence &_fence, CommandBuffer _cmd_buf,
                              bool &_is_in_use)
    : ctx(_ctx), buf(_buf), fence(_fence), cmd_buf(_cmd_buf), is_in_use(_is_in_use) {
    is_in_use = true;
    const eWaitResult res = fence.ClientWaitSync();
    if (res != eWaitResult::Success) {
        _ctx.log()->Error("StageBufRef: fence wait failed (%i), staging buffer may be in use", int(res));
        return;
    }
    ctx.BegSingleTimeCommands(cmd_buf);
    cmd_started = true;
}

Ren::StageBufRef::~StageBufRef() {
    if (buf && cmd_started) {
        fence = ctx.EndSingleTimeCommands(cmd_buf);
        is_in_use = false;
    }
}