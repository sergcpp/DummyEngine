#include "Context.h"

#include <algorithm>
#include <istream>

#if defined(USE_VK_RENDER)
#include "VKCtx.h"
#elif defined(USE_GL_RENDER)
#include "GLCtx.h"
#endif

const char *Ren::Version() { return "v0.1.0-unknown-commit"; }

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, const float *positions, const int vtx_count,
                                    const uint32_t *indices, const int ndx_count, eMeshLoadStatus *load_status) {
    return LoadMesh(name, positions, vtx_count, indices, ndx_count, default_vertex_buf1_, default_vertex_buf2_,
                    default_indices_buf_, load_status);
}

Ren::MeshRef Ren::Context::LoadMesh(std::string_view name, const float *positions, const int vtx_count,
                                    const uint32_t *indices, const int ndx_count, BufferRef &vertex_buf1,
                                    BufferRef &vertex_buf2, BufferRef &index_buf, eMeshLoadStatus *load_status) {
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
                                    const material_load_callback &on_mat_load, BufferRef &vertex_buf1,
                                    BufferRef &vertex_buf2, BufferRef &index_buf, BufferRef &skin_vertex_buf,
                                    BufferRef &delta_buf, eMeshLoadStatus *load_status) {
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

#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
Ren::ShaderRef Ren::Context::LoadShaderGLSL(std::string_view name, std::string_view shader_src, eShaderType type,
                                            eShaderLoadStatus *load_status) {
    ShaderRef ref = shaders_.FindByName(name);
    if (!ref) {
        ref = shaders_.Insert(name, api_ctx_.get(), shader_src, type, load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eShaderLoadStatus::Found;
            }
        } else if (!shader_src.empty()) {
            ref->Init(shader_src, type, load_status, log_);
        }
    }
    return ref;
}

#if defined(USE_VK_RENDER) || !defined(__ANDROID__)
Ren::ShaderRef Ren::Context::LoadShaderSPIRV(std::string_view name, Span<const uint8_t> shader_data, eShaderType type,
                                             eShaderLoadStatus *load_status) {
    ShaderRef ref = shaders_.FindByName(name);
    if (!ref) {
        ref = shaders_.Insert(name, api_ctx_.get(), shader_data, type, load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eShaderLoadStatus::Found;
            }
        } else if (!shader_data.empty()) {
            ref->Init(shader_data, type, load_status, log_);
        }
    }
    return ref;
}
#endif
#endif

Ren::ProgramRef Ren::Context::LoadProgram(std::string_view name, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
                                          ShaderRef tes_ref, ShaderRef gs_ref, eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);
    if (!ref) {
        ref = programs_.Insert(name, api_ctx_.get(), std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref),
                               std::move(tes_ref), std::move(gs_ref), load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eProgLoadStatus::Found;
            }
        } else if (!ref->ready() && vs_ref && fs_ref) {
            ref->Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref), std::move(tes_ref), std::move(gs_ref),
                      load_status, log_);
        }
    }
    return ref;
}

Ren::ProgramRef Ren::Context::LoadProgram(std::string_view name, ShaderRef cs_ref, eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);
    if (!ref) {
        ref = programs_.Insert(name, api_ctx_.get(), std::move(cs_ref), load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status)
                *load_status = eProgLoadStatus::Found;
        } else if (!ref->ready() && cs_ref) {
            ref->Init(std::move(cs_ref), load_status, log_);
        }
    }
    return ref;
}

#if defined(USE_VK_RENDER)
Ren::ProgramRef Ren::Context::LoadProgram2(std::string_view name, ShaderRef raygen_ref, ShaderRef closesthit_ref,
                                           ShaderRef anyhit_ref, ShaderRef miss_ref, ShaderRef intersection_ref,
                                           eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);
    if (!ref) {
        ref = programs_.Insert(name, api_ctx_.get(), std::move(raygen_ref), std::move(closesthit_ref),
                               std::move(anyhit_ref), std::move(miss_ref), std::move(intersection_ref), load_status,
                               log_, 0);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eProgLoadStatus::Found;
            }
        } else if (!ref->ready() && raygen_ref && (closesthit_ref || anyhit_ref) && miss_ref) {
            ref->Init2(std::move(raygen_ref), std::move(closesthit_ref), std::move(anyhit_ref), std::move(miss_ref),
                       std::move(intersection_ref), load_status, log_);
        }
    }
    return ref;
}
#endif

Ren::ProgramRef Ren::Context::GetProgram(const uint32_t index) { return {&programs_, index}; }

int Ren::Context::NumProgramsNotReady() {
    return int(std::count_if(programs_.begin(), programs_.end(), [](const Program &p) { return !p.ready(); }));
}

void Ren::Context::ReleasePrograms() {
    if (programs_.empty()) {
        return;
    }
    log_->Error("---------REMAINING PROGRAMS--------");
    for ([[maybe_unused]] const Program &p : programs_) {
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        log_->Error("%s %i", p.name().c_str(), int(p.id()));
#endif
    }
    log_->Error("-----------------------------------");
    programs_.clear();
}

Ren::Tex3DRef Ren::Context::LoadTexture3D(std::string_view name, const Tex3DParams &p, MemoryAllocators *mem_allocs,
                                          eTexLoadStatus *load_status) {
    Tex3DRef ref = textures_3D_.FindByName(name);
    if (!ref) {
        ref = textures_3D_.Insert(name, api_ctx_.get(), p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else if (ref->params != p) {
        ref->Init(p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::Reinitialized;
    } else {
        (*load_status) = eTexLoadStatus::Found;
    }
    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTexture2D(std::string_view name, const Tex2DParams &p, MemoryAllocators *mem_allocs,
                                          eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_2D_.FindByName(name);
    if (!ref) {
        ref = textures_2D_.Insert(name, api_ctx_.get(), p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else if (ref->params != p) {
        ref->Init(p, mem_allocs, log_);
        (*load_status) = eTexLoadStatus::Reinitialized;
    } else {
        (*load_status) = eTexLoadStatus::Found;
    }
    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTexture2D(std::string_view name, const TexHandle &handle, const Tex2DParams &p,
                                          MemAllocation &&alloc, eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_2D_.FindByName(name);
    if (!ref) {
        ref = textures_2D_.Insert(name, api_ctx_.get(), handle, p, std::move(alloc), log_);
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else if (ref->params != p) {
        ref->Init(handle, p, std::move(alloc), log_);
        (*load_status) = eTexLoadStatus::Reinitialized;
    } else {
        (*load_status) = eTexLoadStatus::Found;
    }
    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTexture2D(std::string_view name, Span<const uint8_t> data, const Tex2DParams &p,
                                          MemoryAllocators *mem_allocs, eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_2D_.FindByName(name);
    if (!ref) {
        ref = textures_2D_.Insert(name, api_ctx_.get(), data, p, mem_allocs, load_status, log_);
    } else {
        (*load_status) = eTexLoadStatus::Found;
        if (!ref->ready() && !data.empty()) {
            ref->Init(data, p, mem_allocs, load_status, log_);
        }
    }
    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTextureCube(std::string_view name, Span<const uint8_t> data[6], const Tex2DParams &p,
                                            MemoryAllocators *mem_allocs, eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_2D_.FindByName(name);
    if (!ref) {
        ref = textures_2D_.Insert(name, api_ctx_.get(), data, p, mem_allocs, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eTexLoadStatus::Found;
        } else if (data) {
            ref->Init(data, p, mem_allocs, load_status, log_);
        }
    }

    return ref;
}

void Ren::Context::VisitTextures(eTexFlags mask, const std::function<void(Texture2D &tex)> &callback) {
    for (Texture2D &tex : textures_2D_) {
        if (bool(tex.params.flags & mask)) {
            callback(tex);
        }
    }
}

int Ren::Context::NumTexturesNotReady() {
    return int(std::count_if(textures_2D_.begin(), textures_2D_.end(), [](const Texture2D &t) { return !t.ready(); }));
}

void Ren::Context::Release2DTextures() {
    if (textures_2D_.empty()) {
        return;
    }
    log_->Error("---------REMAINING 2D TEXTURES--------");
    for (const Texture2D &t : textures_2D_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    textures_2D_.clear();
}

Ren::Tex1DRef Ren::Context::CreateTexture1D(std::string_view name, BufferRef buf, const eTexFormat format,
                                            const uint32_t offset, const uint32_t size) {
    Tex1DRef ref = textures_1D_.FindByName(name);
    if (!ref) {
        ref = textures_1D_.Insert(name, std::move(buf), format, offset, size, log_);
    } else {
        ref->Init(std::move(buf), format, offset, size, log_);
    }

    return ref;
}

void Ren::Context::Release1DTextures() {
    if (textures_1D_.empty()) {
        return;
    }
    log_->Error("---------REMAINING 1D TEXTURES--------");
    for (const Texture1D &t : textures_1D_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    textures_1D_.clear();
}

Ren::TextureRegionRef Ren::Context::LoadTextureRegion(std::string_view name, Span<const uint8_t> data,
                                                      const Tex2DParams &p, eTexLoadStatus *load_status) {
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
                                                      const int data_len, CommandBuffer cmd_buf, const Tex2DParams &p,
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

Ren::BufferRef Ren::Context::LoadBuffer(std::string_view name, const eBufType type, const uint32_t initial_size,
                                        const uint32_t size_alignment, MemoryAllocators *mem_allocs) {
    Ren::BufferRef ref = buffers_.FindByName(name);
    if (!ref) {
        ref = buffers_.Insert(name, api_ctx_.get(), type, initial_size, size_alignment, mem_allocs);
    } else if (ref->size() < initial_size) {
        assert(ref->type() == type);
        ref->Resize(initial_size, false /* keep_content */);
    }
    return ref;
}

Ren::BufferRef Ren::Context::LoadBuffer(std::string_view name, const eBufType type, const BufHandle &handle,
                                        MemAllocation &&alloc, const uint32_t initial_size,
                                        const uint32_t size_alignment) {
    Ren::BufferRef ref = buffers_.FindByName(name);
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
        buffers_.Insert("default_vtx_buf1", api_ctx_.get(), eBufType::VertexAttribs, 16 * 1024 * 1024, 16);
    default_vertex_buf2_ =
        buffers_.Insert("default_vtx_buf2", api_ctx_.get(), eBufType::VertexAttribs, 16 * 1024 * 1024, 16);
    default_skin_vertex_buf_ =
        buffers_.Insert("default_skin_vtx_buf", api_ctx_.get(), eBufType::VertexAttribs, 16 * 1024 * 1024, 16);
    default_delta_buf_ =
        buffers_.Insert("default_delta_buf", api_ctx_.get(), eBufType::VertexAttribs, 16 * 1024 * 1024, 16);
    default_indices_buf_ =
        buffers_.Insert("default_ndx_buf", api_ctx_.get(), eBufType::VertexIndices, 16 * 1024 * 1024, 4);
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
    Release2DTextures();
    ReleaseTextureRegions();
    Release1DTextures();
    ReleaseBuffers();

    texture_atlas_ = {};
}

int Ren::Context::backend_frame() const { return api_ctx_->backend_frame; }

int Ren::Context::active_present_image() const { return api_ctx_->active_present_image; }

Ren::Tex2DRef Ren::Context::backbuffer_ref() const {
    return api_ctx_->present_image_refs[api_ctx_->active_present_image];
}

Ren::StageBufRef::StageBufRef(Context &_ctx, BufferRef &_buf, SyncFence &_fence, CommandBuffer _cmd_buf,
                              bool &_is_in_use)
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