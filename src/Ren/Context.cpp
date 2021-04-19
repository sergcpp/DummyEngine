#include "Context.h"

#include <algorithm>

Ren::MeshRef Ren::Context::LoadMesh(const char *name, const float *positions,
                                    int vtx_count, const uint32_t *indices, int ndx_count,
                                    eMeshLoadStatus *load_status) {
    return LoadMesh(name, positions, vtx_count, indices, ndx_count, default_vertex_buf1_,
                    default_vertex_buf2_, default_indices_buf_, load_status);
}

Ren::MeshRef Ren::Context::LoadMesh(const char *name, const float *positions,
                                    int vtx_count, const uint32_t *indices, int ndx_count,
                                    BufferRef &vertex_buf1, BufferRef &vertex_buf2,
                                    BufferRef &index_buf, eMeshLoadStatus *load_status) {
    MeshRef ref = meshes_.FindByName(name);
    if (!ref) {
        ref = meshes_.Add(name, positions, vtx_count, indices, ndx_count, vertex_buf1,
                          vertex_buf2, index_buf, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eMeshLoadStatus::Found;
        } else if (positions) {
            ref->Init(positions, vtx_count, indices, ndx_count, vertex_buf1, vertex_buf2,
                      index_buf, load_status, log_);
        }
    }

    return ref;
}

Ren::MeshRef Ren::Context::LoadMesh(const char *name, std::istream *data,
                                    const material_load_callback &on_mat_load,
                                    eMeshLoadStatus *load_status) {
    return LoadMesh(name, data, on_mat_load, default_vertex_buf1_, default_vertex_buf2_,
                    default_indices_buf_, default_skin_vertex_buf_, default_delta_buf_,
                    load_status);
}

Ren::MeshRef Ren::Context::LoadMesh(const char *name, std::istream *data,
                                    const material_load_callback &on_mat_load,
                                    BufferRef &vertex_buf1, BufferRef &vertex_buf2,
                                    BufferRef &index_buf, BufferRef &skin_vertex_buf,
                                    BufferRef &delta_buf, eMeshLoadStatus *load_status) {
    MeshRef ref = meshes_.FindByName(name);
    if (!ref) {
        ref = meshes_.Add(name, data, on_mat_load, vertex_buf1, vertex_buf2, index_buf,
                          skin_vertex_buf, delta_buf, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eMeshLoadStatus::Found;
        } else if (data) {
            ref->Init(data, on_mat_load, vertex_buf1, vertex_buf2, index_buf,
                      skin_vertex_buf, delta_buf, load_status, log_);
        }
    }

    return ref;
}

Ren::MaterialRef Ren::Context::LoadMaterial(const char *name, const char *mat_src,
                                            eMatLoadStatus *status,
                                            const program_load_callback &on_prog_load,
                                            const texture_load_callback &on_tex_load) {
    MaterialRef ref = materials_.FindByName(name);
    if (!ref) {
        ref = materials_.Add(name, mat_src, status, on_prog_load, on_tex_load, log_);
    } else {
        if (ref->ready()) {
            (*status) = eMatLoadStatus::Found;
        } else if (!ref->ready() && mat_src) {
            ref->Init(mat_src, status, on_prog_load, on_tex_load, log_);
        }
    }

    return ref;
}

int Ren::Context::NumMaterialsNotReady() {
    return (int)std::count_if(materials_.begin(), materials_.end(),
                              [](const Material &m) { return !m.ready(); });
}

void Ren::Context::ReleaseMaterials() {
    if (!materials_.size()) {
        return;
    }
    log_->Error("---------REMAINING MATERIALS--------");
    for (const Material &m : materials_) {
        log_->Error("%s", m.name().c_str());
    }
    log_->Error("-----------------------------------");
    materials_.clear();
}

Ren::ProgramRef Ren::Context::GetProgram(uint32_t index) { return {&programs_, index}; }

int Ren::Context::NumProgramsNotReady() {
    return (int)std::count_if(programs_.begin(), programs_.end(),
                              [](const Program &p) { return !p.ready(); });
}

void Ren::Context::ReleasePrograms() {
    if (!programs_.size()) {
        return;
    }
    log_->Error("---------REMAINING PROGRAMS--------");
    for (const Program &p : programs_) {
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        log_->Error("%s %i", p.name().c_str(), (int)p.id());
#endif
    }
    log_->Error("-----------------------------------");
    programs_.clear();
}

Ren::Tex2DRef Ren::Context::LoadTexture2D(const char *name, const Tex2DParams &p,
                                          eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Add(name, p, log_);
        (*load_status) = eTexLoadStatus::TexCreatedDefault;
    } else if (ref->params() != p) {
        ref->Init(p, log_);
        (*load_status) = eTexLoadStatus::TexFoundReinitialized;
    } else {
        (*load_status) = eTexLoadStatus::TexFound;
    }
    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTexture2D(const char *name, const void *data, int size,
                                          const Tex2DParams &p,
                                          eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Add(name, data, size, p, load_status, log_);
    } else {
        (*load_status) = eTexLoadStatus::TexFound;
        if (!ref->ready() && data) {
            ref->Init(data, size, p, load_status, log_);
        }
    }

    return ref;
}

Ren::Tex2DRef Ren::Context::LoadTextureCube(const char *name, const void *data[6],
                                            const int size[6], const Tex2DParams &p,
                                            eTexLoadStatus *load_status) {
    Tex2DRef ref = textures_.FindByName(name);
    if (!ref) {
        ref = textures_.Add(name, data, size, p, load_status, log_);
    } else {
        if (ref->ready()) {
            (*load_status) = eTexLoadStatus::TexFound;
        } else if (data) {
            ref->Init(data, size, p, load_status, log_);
        }
    }

    return ref;
}

void Ren::Context::VisitTextures(uint32_t mask,
                                 const std::function<void(Texture2D &tex)> &callback) {
    for (Texture2D &tex : textures_) {
        if (tex.params().flags & mask) {
            callback(tex);
        }
    }
}

int Ren::Context::NumTexturesNotReady() {
    return (int)std::count_if(textures_.begin(), textures_.end(),
                              [](const Texture2D &t) { return !t.ready(); });
}

void Ren::Context::Release2DTextures() {
    if (!textures_.size()) {
        return;
    }
    log_->Error("---------REMAINING 2D TEXTURES--------");
    for (const Texture2D &t : textures_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    textures_.clear();
}

Ren::Tex1DRef Ren::Context::CreateTexture1D(const char *name, BufferRef buf,
                                            const eTexFormat format,
                                            const uint32_t offset, const uint32_t size) {
    Tex1DRef ref = textures_1D_.FindByName(name);
    if (!ref) {
        ref = textures_1D_.Add(name, std::move(buf), format, offset, size, log_);
    } else {
        ref->Init(std::move(buf), format, offset, size, log_);
    }

    return ref;
}

void Ren::Context::Release1DTextures() {
    if (!textures_1D_.size()) {
        return;
    }
    log_->Error("---------REMAINING 1D TEXTURES--------");
    for (const Texture1D &t : textures_1D_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    textures_1D_.clear();
}

Ren::TextureRegionRef Ren::Context::LoadTextureRegion(const char *name, const void *data,
                                                      const int size,
                                                      const Tex2DParams &p,
                                                      eTexLoadStatus *load_status) {
    TextureRegionRef ref = texture_regions_.FindByName(name);
    if (!ref) {
        ref = texture_regions_.Add(name, data, size, p, &texture_atlas_, load_status);
    } else {
        if (ref->ready()) {
            (*load_status) = eTexLoadStatus::TexFound;
        } else {
            ref->Init(data, size, p, &texture_atlas_, load_status);
        }
    }
    return ref;
}

void Ren::Context::ReleaseTextureRegions() {
    if (!texture_regions_.size())
        return;
    log_->Error("-------REMAINING TEX REGIONS-------");
    for (const TextureRegion &t : texture_regions_) {
        log_->Error("%s", t.name().c_str());
    }
    log_->Error("-----------------------------------");
    texture_regions_.clear();
}

Ren::AnimSeqRef Ren::Context::LoadAnimSequence(const char *name, std::istream &data) {
    AnimSeqRef ref = anims_.FindByName(name);
    if (!ref) {
        ref = anims_.Add(name, data);
    } else {
        if (ref->ready()) {
        } else if (!ref->ready() && data) {
            ref->Init(data);
        }
    }

    return ref;
}

int Ren::Context::NumAnimsNotReady() {
    return (int)std::count_if(anims_.begin(), anims_.end(),
                              [](const AnimSequence &a) { return !a.ready(); });
}

void Ren::Context::ReleaseAnims() {
    if (!anims_.size()) {
        return;
    }
    log_->Error("---------REMAINING ANIMS--------");
    for (const AnimSequence &a : anims_) {
        log_->Error("%s", a.name().c_str());
    }
    log_->Error("-----------------------------------");
    anims_.clear();
}

Ren::BufferRef Ren::Context::CreateBuffer(const char *name, const eBufferType type,
                                          const eBufferAccessType access,
                                          const eBufferAccessFreq freq,
                                          const uint32_t initial_size) {
    return buffers_.Add(name, type, access, freq, initial_size);
}

void Ren::Context::ReleaseBuffers() {
    if (!buffers_.size()) {
        return;
    }
    log_->Error("---------REMAINING BUFFERS--------");
    for (const Buffer &b : buffers_) {
        log_->Error("%u", b.size());
    }
    log_->Error("-----------------------------------");
    buffers_.clear();
}

void Ren::Context::ReleaseAll() {
    meshes_.clear();
    default_vertex_buf1_ = {};
    default_vertex_buf2_ = {};
    default_skin_vertex_buf_ = {};
    default_delta_buf_ = {};
    default_indices_buf_ = {};

    ReleaseAnims();
    ReleaseMaterials();
    Release2DTextures();
    ReleaseTextureRegions();
    Release1DTextures();
    ReleaseBuffers();

    texture_atlas_ = {};
}
