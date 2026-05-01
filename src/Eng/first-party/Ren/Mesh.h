#pragma once

#include <cfloat>
#include <memory>

#include "AccStructure.h"
#include "Anim.h"
#include "Buffer.h"
#include "Material.h"
#include "utils/Bitmask.h"
#include "utils/SmallVector.h"
#include "utils/Span.h"
#include "utils/String.h"

namespace Ren {
class ILog;

enum class eMeshFlags : uint8_t { HasAlpha = 0 };

struct tri_group_t {
    int byte_offset = -1, num_indices = 0;
    MaterialHandle front_mat, back_mat, vol_mat;
    Bitmask<eMeshFlags> flags;
};

struct vtx_delta_t {
    float dp[3], dn[3], db[3];
};

struct BufferRange {
    BufferHandle buf;
    SubAllocation sub;
};

enum class eMeshType : uint8_t { Undefined, Simple, Colored, Skeletal };

using material_load_callback = std::function<std::array<MaterialHandle, 3>(std::string_view name)>;

enum class eMeshFileChunk { Info = 0, VtxAttributes, TriIndices, Materials, TriGroups, Bones, ShapeKeys };

struct mesh_chunk_pos_t {
    int32_t offset, length;
};
static_assert(sizeof(mesh_chunk_pos_t) == 8);

struct MeshFileInfo {
    char name[32] = "ModelName";
    float bbox_min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, bbox_max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
};
static_assert(sizeof(MeshFileInfo) == 56);
static_assert(offsetof(MeshFileInfo, bbox_min) == 32);
static_assert(offsetof(MeshFileInfo, bbox_max) == 44);

struct MeshMain {
    eMeshType type = eMeshType::Undefined;
    Bitmask<eMeshFlags> flags;

    BufferRange attribs_buf1, attribs_buf2, indices_buf;
    BufferRange sk_attribs_buf, sk_deltas_buf;
};

struct MeshCold {
    String name;
    std::vector<float> attribs;
    std::vector<uint32_t> indices;
    std::vector<vtx_delta_t> deltas;
    SmallVector<tri_group_t, 8> groups;
    Vec3f bbox_min, bbox_max;
    Skeleton skel;
    AccStructHandle blas;
};

bool Mesh_Init(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name, std::istream &data,
               const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1, ResizableBuffer &vertex_buf2,
               ResizableBuffer &index_buf, ResizableBuffer &skin_vertex_buf, ResizableBuffer &delta_buf, ILog *log);

// simple static mesh
bool Mesh_InitSimple(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                     std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                     ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ILog *log);
// simple mesh with 4 per-vertex colors
bool Mesh_InitColored(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                      std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                      ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ILog *log);
// mesh with 4 bone weights per vertex
bool Mesh_InitSkeletal(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                       std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &skin_vertex_buf,
                       ResizableBuffer &delta_buf, ResizableBuffer &index_buf, ILog *log);

bool Mesh_InitBufferData(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, ResizableBuffer &vertex_buf1,
                         ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ILog *log);

void Mesh_Destroy(SparseDualStorage<BufferMain, BufferCold> &buffers, MeshMain &mesh_main, MeshCold &mesh_cold);
} // namespace Ren