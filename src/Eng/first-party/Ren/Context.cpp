#include "Context.h"

#include <algorithm>
#include <istream>

#if defined(REN_VK_BACKEND)
#include "VKCtx.h"
#elif defined(REN_GL_BACKEND)
#include "GLCtx.h"
#endif

const char *Ren::Version() { return "v0.1.0-unknown-commit"; }

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, const float *positions, const int vtx_count,
                                    const uint32_t *indices, const int ndx_count, eMeshLoadStatus *load_status) {
    return LoadMesh(name, positions, vtx_count, indices, ndx_count, default_vertex_buf1_, default_vertex_buf2_,
                    default_indices_buf_, load_status);
}

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, const float *positions, const int vtx_count,
                                    const uint32_t *indices, const int ndx_count, BufRef &vertex_buf1,
                                    BufRef &vertex_buf2, BufRef &index_buf, eMeshLoadStatus *load_status) {
    MeshRef ref = meshes_.FindByName(name);
    if (!ref) {
        ref = meshes_.Insert(name, positions, vtx_count, indices, ndx_count, api_ctx_.get(), vertex_buf1, vertex_buf2,
                             index_buf, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eMeshLoadStatus::Found;
        } else if (positions) {
            ref->Init(positions, vtx_count, indices, ndx_count, api_ctx_.get(), vertex_buf1, vertex_buf2, index_buf,
                      load_status, log_);
        }
    }

    return ref;
}

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, std::istream *data,
                                    const material_load_callback &on_mat_load, eMeshLoadStatus *load_status) {
    return LoadMesh(name, data, on_mat_load, default_vertex_buf1_, default_vertex_buf2_, default_indices_buf_,
                    default_skin_vertex_buf_, default_delta_buf_, load_status);
}

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, std::istream *data,
                                    const material_load_callback &on_mat_load, BufRef &vertex_buf1, BufRef &vertex_buf2,
                                    BufRef &index_buf, BufRef &skin_vertex_buf, BufRef &delta_buf,
                                    eMeshLoadStatus *load_status) {
    MeshRef ref = meshes_.FindByName(name);
    if (!ref) {
        ref = meshes_.Insert(name, data, on_mat_load, api_ctx_.get(), vertex_buf1, vertex_buf2, index_buf,
                             skin_vertex_buf, delta_buf, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eMeshLoadStatus::Found;
        } else if (data) {
            ref->Init(data, on_mat_load, api_ctx_.get(), vertex_buf1, vertex_buf2, index_buf, skin_vertex_buf,
                      delta_buf, load_status, log_);
        }
    }

    return ref;
}

Ren::MaterialRef Ren::Context::LoadMaterial(std::string_view name, std::string_view mat_src, eMatLoadStatus *status,
                                            const pipelines_load_callback &on_pipes_load,
                                            const texture_load_callback &on_tex_load,
                                            const sampler_load_callback &on_sampler_load) {
    MaterialRef ref = materials_.FindByName(name);
    if (!ref) {
        ref = materials_.Insert(name, mat_src, status, on_pipes_load, on_tex_load, on_sampler_load, log_);
    } else {
        if (ref->ready()) {
            (*status) = eMatLoadStatus::Found;
        } else if (!ref->ready() && !mat_src.empty()) {
            ref->Init(mat_src, status, on_pipes_load, on_tex_load, on_sampler_load, log_);
        }
    }

    return ref;
}

int Ren::Context::NumMaterialsNotReady() {
    return int(std::count_if(materials_.begin(), materials_.end(), [](const Material &m) { return !m.ready(); }));
}

void Ren::Context::ReleaseMaterials() {
    if (materials_.empty()) {
        return;
    }
    log_->Error("---------REMAINING MATERIALS--------");
    for (const Material &m : materials_) {
        log_->Error("%s", m.name().c_str());
    }
    log_->Error("-----------------------------------");
    materials_.clear();
}

#if defined(REN_GL_BACKEND)
Ren::ShaderRef Ren::Context::LoadShaderGLSL(std::string_view name, std::string_view shader_src, eShaderType type) {
    ShaderRef ref = shaders_.FindByName(name);
    if (!ref) {
        ref = shaders_.Insert(name, api_ctx_.get(), shader_src, type, log_);
    } else if (!ref->ready() && !shader_src.empty()) {
        ref->Init(shader_src, type, log_);
    }
    return ref;
}
#endif

#if defined(REN_GL_BACKEND) || defined(REN_VK_BACKEND)
Ren::ShaderRef Ren::Context::LoadShaderSPIRV(std::string_view name, Span<const uint8_t> shader_data, eShaderType type) {
    ShaderRef ref = shaders_.FindByName(name);
    if (!ref) {
        ref = shaders_.Insert(name, api_ctx_.get(), shader_data, type, log_);
    } else if (!ref->ready() && !shader_data.empty()) {
        ref->Init(shader_data, type, log_);
    }
    return ref;
}
#endif

Ren::ProgramRef Ren::Context::LoadProgram(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
                                          ShaderRef gs_ref) {
    std::array<ShaderRef, int(eShaderType::_Count)> temp_shaders;
    temp_shaders[int(eShaderType::Vertex)] = vs_ref;
    temp_shaders[int(eShaderType::Fragment)] = fs_ref;
    temp_shaders[int(eShaderType::TesselationControl)] = tcs_ref;
    temp_shaders[int(eShaderType::TesselationEvaluation)] = tes_ref;
    temp_shaders[int(eShaderType::Geometry)] = gs_ref;
    ProgramRef ref = programs_.LowerBound([&](const Program &p) { return p.shaders() < temp_shaders; });
    if (!ref || ref->shaders() != temp_shaders) {
        assert(vs_ref && fs_ref);
        ref = programs_.Insert(api_ctx_.get(), std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref),
                               std::move(tes_ref), std::move(gs_ref), log_);
    }
    return ref;
}

Ren::ProgramRef Ren::Context::LoadProgram(ShaderRef cs_ref) {
    std::array<ShaderRef, int(eShaderType::_Count)> temp_shaders;
    temp_shaders[int(eShaderType::Compute)] = cs_ref;
    ProgramRef ref = programs_.LowerBound([&](const Program &p) { return p.shaders() < temp_shaders; });
    if (!ref || ref->shaders() != temp_shaders) {
        assert(cs_ref);
        ref = programs_.Insert(api_ctx_.get(), std::move(cs_ref), log_);
    }
    return ref;
}

#if defined(REN_VK_BACKEND)
Ren::ProgramRef Ren::Context::LoadProgram2(ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref,
                                           ShaderRef miss_ref, ShaderRef intersection_ref) {
    std::array<ShaderRef, int(eShaderType::_Count)> temp_shaders;
    temp_shaders[int(eShaderType::RayGen)] = raygen_ref;
    temp_shaders[int(eShaderType::ClosestHit)] = closesthit_ref;
    temp_shaders[int(eShaderType::AnyHit)] = anyhit_ref;
    temp_shaders[int(eShaderType::Miss)] = miss_ref;
    temp_shaders[int(eShaderType::Intersection)] = intersection_ref;
    ProgramRef ref = programs_.LowerBound([&](const Program &p) { return p.shaders() < temp_shaders; });
    if (!ref || ref->shaders() != temp_shaders) {
        assert(raygen_ref);
        ref = programs_.Insert(api_ctx_.get(), std::move(raygen_ref), std::move(closesthit_ref), std::move(anyhit_ref),
                               std::move(miss_ref), std::move(intersection_ref), log_, 0);
    }
    return ref;
}
#endif

Ren::ProgramRef Ren::Context::GetProgram(const uint32_t index) { return {&programs_, index}; }

void Ren::Context::ReleasePrograms() {
    if (programs_.empty()) {
        return;
    }
    log_->Error("---------REMAINING PROGRAMS--------");
    for (const Program &p : programs_) {
        std::string prog_name;
        for (const ShaderRef &sh : p.shaders()) {
            if (!sh) {
                continue;
            }
            if (!prog_name.empty()) {
                prog_name += "&";
            }
            prog_name += sh->name();
        }
#if defined(REN_GL_BACKEND) || defined(REN_SW_BACKEND)
        log_->Error("%s %i", prog_name.c_str(), int(p.id()));
#elif defined(REN_VK_BACKEND)
        log_->Error("%s", prog_name.c_str());
#endif
    }
    log_->Error("-----------------------------------");
    programs_.clear();
}

Ren::VertexInputRef Ren::Context::LoadVertexInput(Span<const VtxAttribDesc> attribs, const BufRef &elem_buf) {
    VertexInputRef ref = vtx_inputs_.LowerBound([&](const VertexInput &vi) {
        if (vi.elem_buf < elem_buf) {
            return true;
        } else if (vi.elem_buf == elem_buf) {
            return Span<const VtxAttribDesc>(vi.attribs) < attribs;
        }
        return false;
    });
    if (!ref || ref->elem_buf != elem_buf || Span<const VtxAttribDesc>(ref->attribs) != attribs) {
        ref = vtx_inputs_.Insert(attribs, elem_buf);
    }
    return ref;
}

Ren::RenderPassRef Ren::Context::LoadRenderPass(const RenderTargetInfo &depth_rt,
                                                Span<const RenderTargetInfo> color_rts) {
    Ren::RenderPassRef ref =
        render_passes_.LowerBound([&](const Ren::RenderPass &rp) { return rp.LessThan(depth_rt, color_rts); });
    if (!ref || !ref->Equals(depth_rt, color_rts)) {
        ref = render_passes_.Insert(api_ctx_.get(), depth_rt, color_rts, log_);
    }
    return ref;
}

Ren::PipelineRef Ren::Context::LoadPipeline(const ProgramRef &prog_ref, const int subgroup_size) {
    PipelineRef ref = pipelines_.LowerBound([&](const Pipeline &pi) { return pi.LessThan({}, prog_ref, {}, {}); });
    if (!ref || !ref->Equals({}, prog_ref, {}, {})) {
        assert(prog_ref);
        ref = pipelines_.Insert(api_ctx_.get(), prog_ref, log_, subgroup_size);
    }
    return ref;
}

Ren::PipelineRef Ren::Context::LoadPipeline(const RastState &rast_state, const ProgramRef &prog,
                                            const VertexInputRef &vtx_input, const RenderPassRef &render_pass,
                                            const uint32_t subpass_index) {
    Ren::PipelineRef ref = pipelines_.LowerBound(
        [&](const Ren::Pipeline &pi) { return pi.LessThan(rast_state, prog, vtx_input, render_pass); });
    if (!ref || !ref->Equals(rast_state, prog, vtx_input, render_pass)) {
        ref = pipelines_.Insert(api_ctx_.get(), rast_state, prog, vtx_input, render_pass, subpass_index, log_);
    }
    return ref;
}

Ren::TexRef Ren::Context::LoadTexture(std::string_view name, const TexParams &p, MemAllocators *mem_allocs,
                                      eTexLoadStatus *load_status) {
    TexRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Insert(name, api_ctx_.get(), p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else if (ref->params != p) {
        ref->Init(p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::Reinitialized;
    } else {
        (*load_status) = eTexLoadStatus::Found;
    }
    return ref;
}

Ren::TexRef Ren::Context::LoadTexture(std::string_view name, const TexHandle &handle, const TexParams &p,
                                      MemAllocation &&alloc, eTexLoadStatus *load_status) {
    TexRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Insert(name, api_ctx_.get(), handle, p, std::move(alloc), log_);
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else if (ref->params != p) {
        ref->Init(handle, p, std::move(alloc), log_);
        (*load_status) = eTexLoadStatus::Reinitialized;
    } else {
        (*load_status) = eTexLoadStatus::Found;
    }
    return ref;
}

Ren::TexRef Ren::Context::LoadTexture(std::string_view name, Span<const uint8_t> data, const TexParams &p,
                                      MemAllocators *mem_allocs, eTexLoadStatus *load_status) {
    TexRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Insert(name, api_ctx_.get(), data, p, mem_allocs, load_status, log_);
    } else {
        (*load_status) = eTexLoadStatus::Found;
        if ((ref->params.flags & eTexFlags::Stub) && !(p.flags & eTexFlags::Stub) && !data.empty()) {
            ref->Init(data, p, mem_allocs, load_status, log_);
        }
    }
    return ref;
}

Ren::TexRef Ren::Context::LoadTextureCube(std::string_view name, Span<const uint8_t> data[6], const TexParams &p,
                                          MemAllocators *mem_allocs, eTexLoadStatus *load_status) {
    TexRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Insert(name, api_ctx_.get(), data, p, mem_allocs, load_status, log_);
    } else {
        (*load_status) = eTexLoadStatus::Found;
        if ((ref->params.flags & eTexFlags::Stub) && (p.flags & eTexFlags::Stub) && data) {
            ref->Init(data, p, mem_allocs, load_status, log_);
        }
    }

    return ref;
}

void Ren::Context::VisitTextures(eTexFlags mask, const std::function<void(Texture &tex)> &callback) {
    for (Texture &tex : textures_) {
        if (bool(tex.params.flags & mask)) {
            callback(tex);
        }
    }
}

int Ren::Context::NumTexturesNotReady() {
    return int(std::count_if(textures_.begin(), textures_.end(),
                             [](const Texture &t) { return bool(t.params.flags & eTexFlags::Stub); }));
}

void Ren::Context::ReleaseTextures() {
    if (textures_.empty()) {
        return;
    }
    log_->Error("---------REMAINING TEXTURES--------");
    for (const Texture &t : textures_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    textures_.clear();
}

Ren::TexBufRef Ren::Context::CreateTextureBuffer(std::string_view name, BufRef buf, const eTexFormat format,
                                                 const uint32_t offset, const uint32_t size) {
    TexBufRef ref = texture_buffers_.FindByName(name);
    if (!ref) {
        ref = texture_buffers_.Insert(name, std::move(buf), format, offset, size, log_);
    } else {
        ref->Init(std::move(buf), format, offset, size, log_);
    }

    return ref;
}

void Ren::Context::ReleaseTextureBuffers() {
    if (texture_buffers_.empty()) {
        return;
    }
    log_->Error("---------REMAINING 1D TEXTURES--------");
    for (const TextureBuffer &t : texture_buffers_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    texture_buffers_.clear();
}

Ren::TextureRegionRef Ren::Context::LoadTextureRegion(std::string_view name, Span<const uint8_t> data,
                                                      const TexParams &p, eTexLoadStatus *load_status) {
    TextureRegionRef ref = texture_regions_.FindByName(name);
    if (!ref) {
        ref = texture_regions_.Insert(name, data, p, &texture_atlas_, load_status);
    } else {
        if (ref->ready()) {
            (*load_status) = eTexLoadStatus::Found;
        } else {
            ref->Init(data, p, &texture_atlas_, load_status);
        }
    }
    return ref;
}

Ren::TextureRegionRef Ren::Context::LoadTextureRegion(std::string_view name, const Buffer &sbuf, const int data_off,
                                                      const int data_len, CommandBuffer cmd_buf, const TexParams &p,
                                                      eTexLoadStatus *load_status) {
    TextureRegionRef ref = texture_regions_.FindByName(name);
    if (!ref) {
        ref = texture_regions_.Insert(name, sbuf, data_off, data_len, cmd_buf, p, &texture_atlas_, load_status);
    } else {
        if (ref->ready()) {
            (*load_status) = eTexLoadStatus::Found;
        } else {
            ref->Init(sbuf, data_off, data_len, cmd_buf, p, &texture_atlas_, load_status);
        }
    }
    return ref;
}

void Ren::Context::ReleaseTextureRegions() {
    if (texture_regions_.empty()) {
        return;
    }
    log_->Error("-------REMAINING TEX REGIONS-------");
    for (const TextureRegion &t : texture_regions_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    texture_regions_.clear();
}

Ren::SamplerRef Ren::Context::LoadSampler(SamplingParams params, eSamplerLoadStatus *load_status) {
    auto it = std::find_if(std::begin(samplers_), std::end(samplers_),
                           [&](const Sampler &sampler) { return sampler.params() == params; });
    if (it != std::end(samplers_)) {
        (*load_status) = eSamplerLoadStatus::Found;
        return SamplerRef{&samplers_, it.index()};
    } else {
        const uint32_t new_index = samplers_.emplace();
        samplers_.at(new_index).Init(api_ctx_.get(), params);
        (*load_status) = eSamplerLoadStatus::Created;
        return SamplerRef{&samplers_, new_index};
    }
}

void Ren::Context::ReleaseSamplers() {
    if (samplers_.empty()) {
        return;
    }
    log_->Error("--------REMAINING SAMPLERS---------");
    for ([[maybe_unused]] const Sampler &s : samplers_) {
        // log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    samplers_.clear();
}

Ren::AnimSeqRef Ren::Context::LoadAnimSequence(std::string_view name, std::istream &data) {
    AnimSeqRef ref = anims_.FindByName(name);
    if (!ref) {
        ref = anims_.Insert(name, data);
    } else {
        if (ref->ready()) {
        } else if (!ref->ready() && data) {
            ref->Init(data);
        }
    }

    return ref;
}

int Ren::Context::NumAnimsNotReady() {
    return int(std::count_if(anims_.begin(), anims_.end(), [](const AnimSequence &a) { return !a.ready(); }));
}

void Ren::Context::ReleaseAnims() {
    if (anims_.empty()) {
        return;
    }
    log_->Error("---------REMAINING ANIMS--------");
    for (const AnimSequence &a : anims_) {
        log_->Error("%s", a.name().c_str());
    }
    log_->Error("-----------------------------------");
    anims_.clear();
}

Ren::BufRef Ren::Context::LoadBuffer(std::string_view name, const eBufType type, const uint32_t initial_size,
                                     const uint32_t size_alignment, MemAllocators *mem_allocs) {
    Ren::BufRef ref = buffers_.FindByName(name);
    if (!ref) {
        ref = buffers_.Insert(name, api_ctx_.get(), type, initial_size, size_alignment, mem_allocs);
    } else if (ref->size() < initial_size) {
        assert(ref->type() == type);
        ref->Resize(initial_size, false /* keep_content */);
    }
    return ref;
}

Ren::BufRef Ren::Context::LoadBuffer(std::string_view name, const eBufType type, const BufHandle &handle,
                                     MemAllocation &&alloc, const uint32_t initial_size,
                                     const uint32_t size_alignment) {
    Ren::BufRef ref = buffers_.FindByName(name);
    if (!ref) {
        ref = buffers_.Insert(name, api_ctx_.get(), type, handle, std::move(alloc), initial_size, size_alignment);
    } else if (ref->size() < initial_size) {
        assert(ref->type() == type);
        ref->Resize(initial_size, false /* keep_content */);
    }
    return ref;
}

void Ren::Context::ReleaseBuffers() {
    if (buffers_.empty()) {
        return;
    }
    log_->Error("---------REMAINING BUFFERS--------");
    for (const Buffer &b : buffers_) {
        log_->Error("%s\t: %u", b.name().c_str(), b.size());
    }
    log_->Error("-----------------------------------");
    buffers_.clear();
}

void Ren::Context::InitDefaultBuffers() {
    default_vertex_buf1_ =
        buffers_.Insert("default_vtx_buf1", api_ctx_.get(), eBufType::VertexAttribs, 1 * 1024 * 1024, 16);
    default_vertex_buf2_ =
        buffers_.Insert("default_vtx_buf2", api_ctx_.get(), eBufType::VertexAttribs, 1 * 1024 * 1024, 16);
    default_skin_vertex_buf_ =
        buffers_.Insert("default_skin_vtx_buf", api_ctx_.get(), eBufType::VertexAttribs, 1 * 1024 * 1024, 16);
    default_delta_buf_ =
        buffers_.Insert("default_delta_buf", api_ctx_.get(), eBufType::VertexAttribs, 1 * 1024 * 1024, 16);
    default_indices_buf_ =
        buffers_.Insert("default_ndx_buf", api_ctx_.get(), eBufType::VertexIndices, 1 * 1024 * 1024, 4);
}

void Ren::Context::ReleaseDefaultBuffers() {
    default_vertex_buf1_ = {};
    default_vertex_buf2_ = {};
    default_skin_vertex_buf_ = {};
    default_delta_buf_ = {};
    default_indices_buf_ = {};
}

void Ren::Context::ReleaseAll() {
    meshes_.clear();

    ReleaseDefaultBuffers();

    ReleaseAnims();
    ReleaseMaterials();
    ReleaseTextures();
    ReleaseTextureRegions();
    ReleaseTextureBuffers();
    ReleaseBuffers();

    texture_atlas_ = {};
}

int Ren::Context::backend_frame() const { return api_ctx_->backend_frame; }

int Ren::Context::active_present_image() const { return api_ctx_->active_present_image; }

Ren::TexRef Ren::Context::backbuffer_ref() const {
    return api_ctx_->present_image_refs[api_ctx_->active_present_image];
}

Ren::StageBufRef::StageBufRef(Context &_ctx, BufRef &_buf, SyncFence &_fence, CommandBuffer _cmd_buf, bool &_is_in_use)
    : ctx(_ctx), buf(_buf), fence(_fence), cmd_buf(_cmd_buf), is_in_use(_is_in_use) {
    is_in_use = true;
    const eWaitResult res = fence.ClientWaitSync();
    assert(res == eWaitResult::Success);
    ctx.BegSingleTimeCommands(cmd_buf);
}

Ren::StageBufRef::~StageBufRef() {
    if (buf) {
        fence = ctx.EndSingleTimeCommands(cmd_buf);
        is_in_use = false;
    }
}