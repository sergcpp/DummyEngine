#include "Context.h"

#include <algorithm>

Ren::MeshRef Ren::Context::LoadMesh(const char *name, std::istream &data, material_load_callback on_mat_load) {
    return LoadMesh(name, data, on_mat_load, default_vertex_buf1_, default_vertex_buf2_, default_indices_buf_, default_skin_vertex_buf_);
}

Ren::MeshRef Ren::Context::LoadMesh(const char *name, std::istream &data, material_load_callback on_mat_load,
                                    BufferRef &vertex_buf1, BufferRef &vertex_buf2, BufferRef &index_buf, BufferRef &skin_vertex_buf) {
    MeshRef ref;
    for (auto it = meshes_.begin(); it != meshes_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &meshes_, it.index() };
        }
    }

    if (!ref) {
        ref = meshes_.Add(name, data, on_mat_load, vertex_buf1, vertex_buf2, index_buf, skin_vertex_buf);
    }

    return ref;
}

Ren::MaterialRef Ren::Context::LoadMaterial(const char *name, const char *mat_src, eMatLoadStatus *status, const program_load_callback &on_prog_load,
        const texture_load_callback &on_tex_load) {
    MaterialRef ref;
    for (auto it = materials_.begin(); it != materials_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &materials_, it.index() };
        }
    }

    if (!ref) {
        ref = materials_.Add(name, mat_src, status, on_prog_load, on_tex_load);
    } else {
        if (ref->ready()) {
            if (status) *status = MatFound;
        } else if (!ref->ready() && mat_src) {
            ref->Init(name, mat_src, status, on_prog_load, on_tex_load);
        }
    }

    return ref;
}

Ren::MaterialRef Ren::Context::GetMaterial(size_t index) {
    return { &materials_, index };
}

int Ren::Context::NumMaterialsNotReady() {
    return (int)std::count_if(materials_.begin(), materials_.end(), [](const Material &m) {
        return !m.ready();
    });
}

void Ren::Context::ReleaseMaterials() {
    if (!materials_.Size()) return;
    fprintf(stderr, "---------REMAINING MATERIALS--------\n");
    for (const auto &m : materials_) {
        fprintf(stderr, "%s\n", m.name());
    }
    fprintf(stderr, "-----------------------------------\n");
    materials_.Clear();
}

Ren::ProgramRef Ren::Context::GetProgram(size_t index) {
    return { &programs_, index };
}

int Ren::Context::NumProgramsNotReady() {
    return (int)std::count_if(programs_.begin(), programs_.end(), [](const Program &p) {
        return !p.ready();
    });
}

void Ren::Context::ReleasePrograms() {
    if (!programs_.Size()) return;
    fprintf(stderr, "---------REMAINING PROGRAMS--------\n");
    for (const auto &p : programs_) {
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        fprintf(stderr, "%s %i\n", p.name(), (int)p.prog_id());
#endif
    }
    fprintf(stderr, "-----------------------------------\n");
    programs_.Clear();
}

Ren::Texture2DRef Ren::Context::LoadTexture2D(const char *name, const void *data, int size,
        const Texture2DParams &p, eTexLoadStatus *load_status) {
    Texture2DRef ref;
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &textures_, it.index() };
            break;
        }
    }

    if (!ref) {
        ref = textures_.Add(name, data, size, p, load_status);
    } else {
        if (load_status) *load_status = TexFound;
        if (!ref->ready() && data) {
            ref->Init(name, data, size, p, load_status);
        }
    }

    return ref;
}

Ren::Texture2DRef Ren::Context::LoadTextureCube(const char *name, const void *data[6], const int size[6],
        const Texture2DParams &p, eTexLoadStatus *load_status) {
    Texture2DRef ref;
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &textures_, it.index() };
            break;
        }
    }

    if (!ref) {
        ref = textures_.Add(name, data, size, p, load_status);
    } else {
        if (ref->ready()) {
            if (load_status) *load_status = TexFound;
        } else if (!ref->ready() && data) {
            ref->Init(name, data, size, p, load_status);
        }
    }

    return ref;
}

int Ren::Context::NumTexturesNotReady() {
    return (int)std::count_if(textures_.begin(), textures_.end(), [](const Texture2D &t) {
        return !t.ready();
    });
}

void Ren::Context::ReleaseTextures() {
    if (!textures_.Size()) return;
    fprintf(stderr, "---------REMAINING TEXTURES--------\n");
    for (const auto &t : textures_) {
        fprintf(stderr, "%s\n", t.name());
    }
    fprintf(stderr, "-----------------------------------\n");
    textures_.Clear();
}

Ren::AnimSeqRef Ren::Context::LoadAnimSequence(const char *name, std::istream &data) {
    AnimSeqRef ref;
    for (auto it = anims_.begin(); it != anims_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &anims_, it.index() };
            break;
        }
    }

    if (!ref) {
        ref = anims_.Add(name, data);
    } else {
        if (ref->ready()) {
        } else if (!ref->ready() && data) {
            ref->Init(name, data);
        }
    }

    return ref;
}

int Ren::Context::NumAnimsNotReady() {
    return (int)std::count_if(anims_.begin(), anims_.end(), [](const AnimSequence &a) {
        return !a.ready();
    });
}

void Ren::Context::ReleaseAnims() {
    if (!anims_.Size()) return;
    fprintf(stderr, "---------REMAINING ANIMS--------\n");
    for (const auto &a : anims_) {
        fprintf(stderr, "%s\n", a.name());
    }
    fprintf(stderr, "-----------------------------------\n");
    anims_.Clear();
}

Ren::BufferRef Ren::Context::CreateBuffer(uint32_t initial_size) {
    return buffers_.Add(initial_size);
}

void Ren::Context::ReleaseBuffers() {
    if (!buffers_.Size()) return;
    fprintf(stderr, "---------REMAINING BUFFERS--------\n");
    for (const auto &b : buffers_) {
        fprintf(stderr, "%u\n", b.size());
    }
    fprintf(stderr, "-----------------------------------\n");
    buffers_.Clear();
}

void Ren::Context::ReleaseAll() {
    meshes_.Clear();
    default_vertex_buf1_ = {};
    default_vertex_buf2_ = {};
    default_skin_vertex_buf_ = {};
    default_indices_buf_ = {};

    ReleaseAnims();
    ReleaseMaterials();
    ReleaseTextures();
    ReleaseBuffers();
}