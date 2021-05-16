#pragma once

#include <memory>

#include "Anim.h"
#include "Buffer.h"
#include "Material.h"
#include "SmallVector.h"
#include "String.h"

namespace Ren {
class ILog;

enum eMeshFlags { MeshHasAlpha = 1 };

struct TriGroup {
    int offset = -1;
    int num_indices = 0;
    MaterialRef mat;
    uint32_t flags = 0;

    TriGroup() = default;
    TriGroup(const TriGroup &rhs) = delete;
    TriGroup(TriGroup &&rhs) noexcept = default;
    TriGroup &operator=(const TriGroup &rhs) = delete;
    TriGroup &operator=(TriGroup &&rhs) noexcept = default;
};

struct VtxDelta {
    float dp[3], dn[3], db[3];
};

struct BufferRange {
    BufferRef buf;
    uint32_t offset, size;

    BufferRange() : offset(0), size(0) {}
    BufferRange(BufferRef &_buf, const uint32_t _offset, const uint32_t _size)
        : buf(_buf), offset(_offset), size(_size) {}
    ~BufferRange() { Release(); }

    BufferRange(const BufferRange &rhs) = delete;
    BufferRange(BufferRange &&rhs) noexcept = default;

    BufferRange &operator=(const BufferRange &rhs) = delete;
    BufferRange &operator=(BufferRange &&rhs) noexcept {
        Release();

        buf = std::move(rhs.buf);
        offset = exchange(rhs.offset, 0);
        size = exchange(rhs.size, 0);

        return *this;
    }

    void Release() {
        if (buf) {
            const bool res = buf->FreeRegion(offset);
            assert(res);
        }
        buf = {};
    }
};

enum class eMeshLoadStatus { Found, SetToDefault, CreatedFromData };

enum class eMeshType { Undefined, Simple, Colored, Skeletal };

typedef std::function<MaterialRef(const char *name)> material_load_callback;

class Mesh : public RefCounter {
    eMeshType type_ = eMeshType::Undefined;
    uint32_t flags_ = 0;
    bool ready_ = false;
    BufferRange attribs_buf1_, attribs_buf2_, sk_attribs_buf_, sk_deltas_buf_,
        indices_buf_;
    std::unique_ptr<char[]> attribs_, indices_;
    std::unique_ptr<VtxDelta[]> deltas_;
    SmallVector<TriGroup, 8> groups_;
    Vec3f bbox_min_, bbox_max_;
    String name_;

    Skeleton skel_;

    // simple static mesh with normals
    void InitMeshSimple(std::istream &data, const material_load_callback &on_mat_load,
                        BufferRef vertex_buf1, BufferRef vertex_buf2, BufferRef index_buf,
                        ILog *log);
    // simple mesh with 4 per-vertex colors
    void InitMeshColored(std::istream &data, const material_load_callback &on_mat_load,
                         BufferRef vertex_buf1, BufferRef vertex_buf2,
                         BufferRef index_buf, ILog *log);
    // mesh with 4 bone weights per vertex
    void InitMeshSkeletal(std::istream &data, const material_load_callback &on_mat_load,
                          BufferRef skin_vertex_buf, BufferRef delta_buf,
                          BufferRef index_buf, ILog *log);

    // split skeletal mesh into chunks to fit uniforms limit in shader
    // void SplitMesh(int bones_limit, ILog *log);

  public:
    Mesh() = default;
    Mesh(const char *name, const float *positions, int vtx_count, const uint32_t *indices,
         int ndx_count, BufferRef vertex_buf1, BufferRef vertex_buf2, BufferRef index_buf,
         eMeshLoadStatus *load_status, ILog *log);
    Mesh(const char *name, std::istream *data, const material_load_callback &on_mat_load,
         BufferRef vertex_buf1, BufferRef vertex_buf2, BufferRef index_buf,
         BufferRef skin_vertex_buf, BufferRef delta_buf, eMeshLoadStatus *load_status,
         ILog *log);

    Mesh(const Mesh &rhs) = delete;
    Mesh(Mesh &&rhs) = default;

    Mesh &operator=(const Mesh &rhs) = delete;
    Mesh &operator=(Mesh &&rhs) = default;

    eMeshType type() const { return type_; }
    uint32_t flags() const { return flags_; }
    bool ready() const { return ready_; }
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t attribs_buf1_id() const { return attribs_buf1_.buf->id(); }
    uint32_t attribs_buf2_id() const { return attribs_buf2_.buf->id(); }
    uint32_t indices_buf_id() const { return indices_buf_.buf->id(); }
#endif
    BufHandle attribs_buf1_handle() const { return attribs_buf1_.buf->handle(); }
    BufHandle attribs_buf2_handle() const { return attribs_buf2_.buf->handle(); }
    BufHandle indices_buf_handle() const { return indices_buf_.buf->handle(); }
    const void *attribs() const { return attribs_.get(); }
    const BufferRange &attribs_buf1() const { return attribs_buf1_; }
    const BufferRange &attribs_buf2() const { return attribs_buf2_; }
    const BufferRange &sk_attribs_buf() const { return sk_attribs_buf_; }
    const BufferRange &sk_deltas_buf() const { return sk_deltas_buf_; }
    const void *indices() const { return indices_.get(); }
    const BufferRange &indices_buf() const { return indices_buf_; }
    const SmallVectorImpl<TriGroup> &groups() const { return groups_; }
    const Vec3f &bbox_min() const { return bbox_min_; }
    const Vec3f &bbox_max() const { return bbox_max_; }
    const String &name() const { return name_; }

    Skeleton *skel() { return &skel_; }

    const Skeleton *skel() const { return &skel_; }

    void Init(const float *positions, int vtx_count, const uint32_t *indices,
              int ndx_count, BufferRef vertex_buf1, BufferRef vertex_buf2,
              BufferRef index_buf, eMeshLoadStatus *load_status, ILog *log);
    void Init(std::istream *data, const material_load_callback &on_mat_load,
              BufferRef vertex_buf1, BufferRef vertex_buf2, BufferRef index_buf,
              BufferRef skin_vertex_buf, BufferRef delta_buf,
              eMeshLoadStatus *load_status, ILog *log);
};

typedef StrongRef<Mesh> MeshRef;
typedef Storage<Mesh> MeshStorage;
} // namespace Ren