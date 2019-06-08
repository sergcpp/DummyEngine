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

const int MaxMeshTriGroupsCount = 32;

struct BufferRange {
    BufferRef buf;
    uint32_t  offset, size;

    BufferRange() : offset(0), size(0) {}
    BufferRange(BufferRef &_buf, uint32_t _offset, uint32_t _size) : buf(_buf), offset(_offset), size(_size) {}
    ~BufferRange() {
        Release();
    }

    BufferRange(const BufferRange &rhs) = delete;
    BufferRange(BufferRange &&rhs) {
        buf = std::move(rhs.buf);
        offset = rhs.offset;
        rhs.offset = 0;
        size = rhs.size;
        rhs.size = 0;
    }

    BufferRange &operator=(const BufferRange &rhs) = delete;
    BufferRange &operator=(BufferRange &&rhs) {
        Release();

        buf = std::move(rhs.buf);
        offset = rhs.offset;
        rhs.offset = 0;
        size = rhs.size;
        rhs.size = 0;

        return *this;
    }

    void Release() {
        if (buf && size) {
            bool res = buf->Free(offset);
            assert(res);
            size = 0;
            offset = 0;
        }
        buf = {};
    }
};

enum eMeshType { MeshUndefined, MeshSimple, MeshTerrain, MeshSkeletal };

typedef std::function<MaterialRef(const char *name)> material_load_callback;

class Mesh : public RefCounter {
    int             type_ = MeshUndefined;
    uint32_t        flags_ = 0;
    BufferRange     attribs_buf_, sk_attribs_buf_, indices_buf_, sk_indices_buf_;
    std::unique_ptr <char[]> attribs_, indices_;
    std::array<TriGroup, MaxMeshTriGroupsCount>    groups_;
    Vec3f           bbox_min_, bbox_max_;
    char            name_[48];

    Skeleton        skel_;

    // simple static mesh with normals
    void InitMeshSimple(std::istream &data, const material_load_callback &on_mat_load, BufferRef &vertex_buf, BufferRef &index_buf);
    // simple mesh with tex index per vertex
    void InitMeshTerrain(std::istream &data, const material_load_callback &on_mat_load, BufferRef &vertex_buf, BufferRef &index_buf);
    // mesh with 4 bone weights per vertex
    void InitMeshSkeletal(std::istream &data, const material_load_callback &on_mat_load,
                          BufferRef &vertex_buf, BufferRef &index_buf, BufferRef &skin_vertex_buf, BufferRef &skin_index_buf);

    // split skeletal mesh into chunks to fit uniforms limit in shader
    void SplitMesh(int bones_limit);
public:
    Mesh() {
        name_[0] = '\0';
    }
    Mesh(const char *name, std::istream &data, const material_load_callback &on_mat_load,
         BufferRef &vertex_buf, BufferRef &index_buf, BufferRef &skin_vertex_buf, BufferRef &skin_index_buf);

    int type() const {
        return type_;
    }
    uint32_t flags() const {
        return flags_;
    }
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t attribs_buf_id() const {
        return attribs_buf_.buf->buf_id();
    }
    uint32_t indices_buf_id() const {
        return indices_buf_.buf->buf_id();
    }
#endif
    const void *attribs() const {
        return attribs_.get();
    }
    const BufferRange &attribs_buf() const {
        return attribs_buf_;
    }
    const BufferRange &sk_attribs_buf() const {
        return sk_attribs_buf_;
    }
    const void *indices() const {
        return indices_.get();
    }
    const BufferRange &indices_buf() const {
        return indices_buf_;
    }
    const BufferRange &sk_indices_buf() const {
        return sk_indices_buf_;
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

    const Skeleton *skel() const {
        return &skel_;
    }

    void Init(const char *name, std::istream &data, const material_load_callback &on_mat_load,
              BufferRef &vertex_buf, BufferRef &index_buf, BufferRef &skin_vertex_buf, BufferRef &skin_index_buf);

    static int max_gpu_bones;
};

typedef StorageRef<Mesh> MeshRef;
typedef Storage<Mesh> MeshStorage;
}