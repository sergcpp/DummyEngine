#pragma once

#include <array>
#include <memory>

#include "Anim.h"
#include "Buffer.h"
#include "Material.h"

namespace Ren {
enum eMeshFlags { MeshHasAlpha = 1 };

struct TriGroup {
    int         offset = -1;
    int         num_indices = 0;
    MaterialRef mat;
    uint32_t    flags = 0;

    TriGroup() {}
    TriGroup(const TriGroup &rhs) = delete;
    TriGroup(TriGroup &&rhs) {
        (*this) = std::move(rhs);
    }
    TriGroup &operator=(const TriGroup &rhs) = delete;
    TriGroup &operator=(TriGroup &&rhs) {
        offset = rhs.offset;
        rhs.offset = -1;
        num_indices = rhs.num_indices;
        rhs.num_indices = 0;
        mat = std::move(rhs.mat);
        flags = rhs.flags;
        rhs.flags = 0;
        return *this;
    }
};

enum eMeshType { MeshUndefined, MeshSimple, MeshTerrain, MeshSkeletal };

typedef std::function<MaterialRef(const char *name)> material_load_callback;

class Mesh : public RefCounter {
    int             type_ = MeshUndefined;
    uint32_t        flags_ = 0;
    BufferRef       attribs_buf_, indices_buf_;
    uint32_t        attribs_offset_ = 0;
    uint32_t        indices_offset_ = 0;
    std::shared_ptr<void> attribs_;
    uint32_t        attribs_size_ = 0;
    std::shared_ptr<void> indices_;
    uint32_t        indices_size_ = 0;
    std::array<TriGroup, 16>    groups_;
    Vec3f           bbox_min_, bbox_max_;
    char            name_[32];

    Skeleton        skel_;

    // simple static mesh with normals
    void InitMeshSimple(std::istream &data, const material_load_callback &on_mat_load,
                        const BufferRef &vertex_buf, const BufferRef &index_buf);
    // simple mesh with tex index per vertex
    void InitMeshTerrain(std::istream &data, const material_load_callback &on_mat_load,
                         const BufferRef &vertex_buf, const BufferRef &index_buf);
    // mesh with 4 bone weights per vertex
    void InitMeshSkeletal(std::istream &data, const material_load_callback &on_mat_load,
                          const BufferRef &vertex_buf, const BufferRef &index_buf);

    // split skeletal mesh into chunks to fit uniforms limit in shader
    void SplitMesh(int bones_limit);
public:
    Mesh() {
        name_[0] = '\0';
    }
    Mesh(const char *name, std::istream &data, const material_load_callback &on_mat_load, const BufferRef &vertex_buf, const BufferRef &index_buf);

    int type() const {
        return type_;
    }
    uint32_t flags() const {
        return flags_;
    }
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t attribs_buf_id() const {
        return attribs_buf_->buf_id();
    }
    uint32_t indices_buf_id() const {
        return indices_buf_->buf_id();
    }
#endif
    const void *attribs() const {
        return attribs_.get();
    }
    uint32_t attribs_offset() const {
        return attribs_offset_;
    }
    uint32_t attribs_size() const {
        return attribs_size_;
    }
    const void *indices() const {
        return indices_.get();
    }
    uint32_t indices_offset() const {
        return indices_offset_;
    }
    uint32_t indices_size() const {
        return indices_size_;
    }
    const TriGroup &group(int i) const {
        return groups_[i];
    }
    const Vec3f &bbox_min() const {
        return bbox_min_;
    }
    const Vec3f &bbox_max() const {
        return bbox_max_;
    }
    const char *name() const {
        return &name_[0];
    }

    Skeleton *skel() {
        return &skel_;
    }

    void Init(const char *name, std::istream &data, const material_load_callback &on_mat_load,
              const BufferRef &vertex_buf, const BufferRef &index_buf);

    static int max_gpu_bones;
};

typedef StorageRef<Mesh> MeshRef;
typedef Storage<Mesh> MeshStorage;
}