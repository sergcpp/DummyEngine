#pragma once

#include <cfloat>
#include <memory>

#include "AccStructure.h"
#include "Anim.h"
#include "Bitmask.h"
#include "Buffer.h"
#include "Material.h"
#include "SmallVector.h"
#include "Span.h"
#include "String.h"

namespace Ren {
class ILog;

enum class eMeshFlags { HasAlpha = 0 };

struct TriGroup {
    int byte_offset = -1;
    int num_indices = 0;
    MaterialRef front_mat, back_mat;
    Bitmask<eMeshFlags> flags;

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
    WeakBufRef buf;
    SubAllocation sub;
    uint32_t size = 0;

    BufferRange() = default;
    BufferRange(BufRef &_buf, const SubAllocation _sub, uint32_t _size) : buf(_buf), sub(_sub), size(_size) {}
    ~BufferRange() { Release(); }

    BufferRange(const BufferRange &rhs) = delete;
    BufferRange(BufferRange &&rhs) noexcept = default;

    BufferRange &operator=(const BufferRange &rhs) = delete;
    BufferRange &operator=(BufferRange &&rhs) noexcept {
        Release();

        buf = std::move(rhs.buf);
        sub = std::exchange(rhs.sub, {});
        size = std::exchange(rhs.size, 0);

        return *this;
    }

    void Release() {
        if (buf) {
            const bool res = buf->FreeSubRegion(sub);
            assert(res);
        }
        buf = WeakBufRef{};
    }
};

enum class eMeshLoadStatus { Error, Found, CreatedFromData };

enum class eMeshType { Undefined, Simple, Colored, Skeletal };

using material_load_callback = std::function<std::pair<MaterialRef, MaterialRef>(std::string_view name)>;

enum class eMeshFileChunk { Info = 0, VtxAttributes, TriIndices, Materials, TriGroups, Bones, ShapeKeys };

struct MeshChunkPos {
    int32_t offset, length;
};
static_assert(sizeof(MeshChunkPos) == 8);

struct MeshFileInfo {
    char name[32] = "ModelName";
    float bbox_min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, bbox_max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
};
static_assert(sizeof(MeshFileInfo) == 56);
static_assert(offsetof(MeshFileInfo, bbox_min) == 32);
static_assert(offsetof(MeshFileInfo, bbox_max) == 44);

class Mesh : public RefCounter {
    eMeshType type_ = eMeshType::Undefined;
    Bitmask<eMeshFlags> flags_;
    bool ready_ = false;
    BufferRange attribs_buf1_, attribs_buf2_, sk_attribs_buf_, sk_deltas_buf_, indices_buf_;
    std::vector<float> attribs_;
    std::vector<uint32_t> indices_;
    std::vector<VtxDelta> deltas_;
    SmallVector<TriGroup, 8> groups_;
    Vec3f bbox_min_, bbox_max_;
    String name_;

    Skeleton skel_;

    // simple static mesh with normals
    void InitMeshSimple(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                        BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, ILog *log);
    // simple mesh with 4 per-vertex colors
    void InitMeshColored(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                         BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, ILog *log);
    // mesh with 4 bone weights per vertex
    void InitMeshSkeletal(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                          BufRef &skin_vertex_buf, BufRef &delta_buf, BufRef &index_buf, ILog *log);

  public:
    Mesh() = default;
    Mesh(std::string_view name, const float *positions, int vtx_count, const uint32_t *indices, int ndx_count,
         ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, eMeshLoadStatus *load_status,
         ILog *log);
    Mesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
         BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, BufRef &skin_vertex_buf, BufRef &delta_buf,
         eMeshLoadStatus *load_status, ILog *log);

    Mesh(const Mesh &rhs) = delete;
    Mesh(Mesh &&rhs) = default;

    Mesh &operator=(const Mesh &rhs) = delete;
    Mesh &operator=(Mesh &&rhs) = default;

    eMeshType type() const { return type_; }
    Bitmask<eMeshFlags> flags() const { return flags_; }
    bool ready() const { return ready_; }
    BufHandle attribs_buf1_handle() const { return attribs_buf1_.buf->handle(); }
    BufHandle attribs_buf2_handle() const { return attribs_buf2_.buf->handle(); }
    BufHandle indices_buf_handle() const { return indices_buf_.buf->handle(); }
    Span<const float> attribs() const { return attribs_; }
    const BufferRange &attribs_buf1() const { return attribs_buf1_; }
    const BufferRange &attribs_buf2() const { return attribs_buf2_; }
    const BufferRange &sk_attribs_buf() const { return sk_attribs_buf_; }
    const BufferRange &sk_deltas_buf() const { return sk_deltas_buf_; }
    Span<const uint32_t> indices() const { return indices_; }
    const BufferRange &indices_buf() const { return indices_buf_; }
    Span<const TriGroup> groups() const { return groups_; }
    SmallVectorImpl<TriGroup> &groups() { return groups_; }
    const Vec3f &bbox_min() const { return bbox_min_; }
    const Vec3f &bbox_max() const { return bbox_max_; }
    const String &name() const { return name_; }

    const Skeleton *skel() const { return &skel_; }
    Skeleton *skel() { return &skel_; }

    void Init(const float *positions, int vtx_count, const uint32_t *indices, int ndx_count, ApiContext *api_ctx,
              BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, eMeshLoadStatus *load_status, ILog *log);
    void Init(std::istream *data, const material_load_callback &on_mat_load, ApiContext *api_ctx, BufRef &vertex_buf1,
              BufRef &vertex_buf2, BufRef &index_buf, BufRef &skin_vertex_buf, BufRef &delta_buf,
              eMeshLoadStatus *load_status, ILog *log);

    void InitBufferData(ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf);
    void ReleaseBufferData();

    std::unique_ptr<IAccStructure> blas;
};

using MeshRef = StrongRef<Mesh, NamedStorage<Mesh>>;
using MeshStorage = NamedStorage<Mesh>;
} // namespace Ren