#include "Mesh.h"

#include <ctime>

#if defined(USE_GL_RENDER)
#include "GL.h"
#elif defined(USE_SW_RENDER)
#include "SW/SW.h"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
uint16_t f32_to_f16(float value);
int16_t f32_to_s16(float value);
uint16_t f32_to_u16(float value);

struct orig_vertex_t {
    float p[3];
    float n[3];
    float b[3];
    float t0[2];
    float t1[2];
};
static_assert(sizeof(orig_vertex_t) == 52, "!");

struct orig_vertex_colored_t {
    float p[3];
    float n[3];
    float b[3];
    float t0[2];
    uint8_t c[4];
};
static_assert(sizeof(orig_vertex_colored_t) == 48, "!");

struct orig_vertex_skinned_t {
    orig_vertex_t v;
    int32_t bone_indices[4];
    float bone_weights[4];
};
static_assert(sizeof(orig_vertex_skinned_t) == 84, "!");

struct orig_vertex_skinned_colored_t {
    orig_vertex_colored_t v;
    int32_t bone_indices[4];
    float bone_weights[4];
};
static_assert(sizeof(orig_vertex_skinned_t) == 84, "!");

struct packed_vertex_data1_t {
    float p[3];
    uint16_t t0[2];
};
static_assert(sizeof(packed_vertex_data1_t) == 16, "!");

struct packed_vertex_data2_t {
    int16_t n_and_bx[4];
    int16_t byz[2];
    uint16_t t1[2];
};
static_assert(sizeof(packed_vertex_data2_t) == 16, "!");

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_data1_t, t0) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_data2_t, n_and_bx) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_data2_t, byz) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_data2_t, t1) % 4 == 0, "!");

struct packed_vertex_t {
    float p[3];
    int16_t n_and_bx[4];
    int16_t byz[2];
    uint16_t t0[2];
    uint16_t t1[2];
};
static_assert(sizeof(packed_vertex_t) == 32, "!");

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_t, n_and_bx) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_t, byz) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_t, t0) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_t, t1) % 4 == 0, "!");

struct packed_vertex_skinned_t {
    packed_vertex_t v;
    uint16_t bone_indices[4];
    uint16_t bone_weights[4];
};
static_assert(sizeof(packed_vertex_skinned_t) == 48, "!");

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_skinned_t, v.n_and_bx) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_skinned_t, v.byz) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_skinned_t, v.t0) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_skinned_t, v.t1) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_skinned_t, bone_indices) % 4 == 0, "!");
static_assert(offsetof(packed_vertex_skinned_t, bone_weights) % 4 == 0, "!");

struct packed_vertex_delta_t {
    float dp[3];
    int16_t dn[3]; // normalized, delta normal is limited but it is fine
    int16_t db[3];
};
static_assert(sizeof(packed_vertex_delta_t) == 24, "!");

void pack_vertex(const orig_vertex_t &in_v, packed_vertex_t &out_v) {
    out_v.p[0] = in_v.p[0];
    out_v.p[1] = in_v.p[1];
    out_v.p[2] = in_v.p[2];
    out_v.n_and_bx[0] = f32_to_s16(in_v.n[0]);
    out_v.n_and_bx[1] = f32_to_s16(in_v.n[1]);
    out_v.n_and_bx[2] = f32_to_s16(in_v.n[2]);
    out_v.n_and_bx[3] = f32_to_s16(in_v.b[0]);
    out_v.byz[0] = f32_to_s16(in_v.b[1]);
    out_v.byz[1] = f32_to_s16(in_v.b[2]);
    out_v.t0[0] = f32_to_f16(in_v.t0[0]);
    out_v.t0[1] = f32_to_f16(1.0f - in_v.t0[1]);
    out_v.t1[0] = f32_to_f16(in_v.t1[0]);
    out_v.t1[1] = f32_to_f16(1.0f - in_v.t1[1]);
}

void pack_vertex(const orig_vertex_colored_t &in_v, packed_vertex_t &out_v) {
    out_v.p[0] = in_v.p[0];
    out_v.p[1] = in_v.p[1];
    out_v.p[2] = in_v.p[2];
    out_v.n_and_bx[0] = f32_to_s16(in_v.n[0]);
    out_v.n_and_bx[1] = f32_to_s16(in_v.n[1]);
    out_v.n_and_bx[2] = f32_to_s16(in_v.n[2]);
    out_v.n_and_bx[3] = f32_to_s16(in_v.b[0]);
    out_v.byz[0] = f32_to_s16(in_v.b[1]);
    out_v.byz[1] = f32_to_s16(in_v.b[2]);
    out_v.t0[0] = f32_to_f16(in_v.t0[0]);
    out_v.t0[1] = f32_to_f16(1.0f - in_v.t0[1]);
    out_v.t1[0] = uint16_t(uint16_t(in_v.c[1]) << 8u) | uint16_t(in_v.c[0]);
    out_v.t1[1] = uint16_t(uint16_t(in_v.c[3]) << 8u) | uint16_t(in_v.c[2]);
}

void pack_vertex_data1(const orig_vertex_t &in_v, packed_vertex_data1_t &out_v) {
    out_v.p[0] = in_v.p[0];
    out_v.p[1] = in_v.p[1];
    out_v.p[2] = in_v.p[2];
    out_v.t0[0] = f32_to_f16(in_v.t0[0]);
    out_v.t0[1] = f32_to_f16(1.0f - in_v.t0[1]);
}

void pack_vertex_data2(const orig_vertex_t &in_v, packed_vertex_data2_t &out_v) {
    out_v.n_and_bx[0] = f32_to_s16(in_v.n[0]);
    out_v.n_and_bx[1] = f32_to_s16(in_v.n[1]);
    out_v.n_and_bx[2] = f32_to_s16(in_v.n[2]);
    out_v.n_and_bx[3] = f32_to_s16(in_v.b[0]);
    out_v.byz[0] = f32_to_s16(in_v.b[1]);
    out_v.byz[1] = f32_to_s16(in_v.b[2]);
    out_v.t1[0] = f32_to_f16(in_v.t1[0]);
    out_v.t1[1] = f32_to_f16(1.0f - in_v.t1[1]);
}

void pack_vertex(const orig_vertex_skinned_t &in_v, packed_vertex_skinned_t &out_v) {
    pack_vertex(in_v.v, out_v.v);

    out_v.bone_indices[0] = uint16_t(in_v.bone_indices[0]);
    out_v.bone_indices[1] = uint16_t(in_v.bone_indices[1]);
    out_v.bone_indices[2] = uint16_t(in_v.bone_indices[2]);
    out_v.bone_indices[3] = uint16_t(in_v.bone_indices[3]);

    out_v.bone_weights[0] = f32_to_u16(in_v.bone_weights[0]);
    out_v.bone_weights[1] = f32_to_u16(in_v.bone_weights[1]);
    out_v.bone_weights[2] = f32_to_u16(in_v.bone_weights[2]);
    out_v.bone_weights[3] = f32_to_u16(in_v.bone_weights[3]);
}

void pack_vertex(const orig_vertex_skinned_colored_t &in_v,
                 packed_vertex_skinned_t &out_v) {
    pack_vertex(in_v.v, out_v.v);

    out_v.bone_indices[0] = uint16_t(in_v.bone_indices[0]);
    out_v.bone_indices[1] = uint16_t(in_v.bone_indices[1]);
    out_v.bone_indices[2] = uint16_t(in_v.bone_indices[2]);
    out_v.bone_indices[3] = uint16_t(in_v.bone_indices[3]);

    out_v.bone_weights[0] = f32_to_u16(in_v.bone_weights[0]);
    out_v.bone_weights[1] = f32_to_u16(in_v.bone_weights[1]);
    out_v.bone_weights[2] = f32_to_u16(in_v.bone_weights[2]);
    out_v.bone_weights[3] = f32_to_u16(in_v.bone_weights[3]);
}

void pack_vertex_data1(const orig_vertex_colored_t &in_v, packed_vertex_data1_t &out_v) {
    out_v.p[0] = in_v.p[0];
    out_v.p[1] = in_v.p[1];
    out_v.p[2] = in_v.p[2];
    out_v.t0[0] = f32_to_f16(in_v.t0[0]);
    out_v.t0[1] = f32_to_f16(in_v.t0[1]);
}

void pack_vertex_data2(const orig_vertex_colored_t &in_v, packed_vertex_data2_t &out_v) {
    out_v.n_and_bx[0] = f32_to_s16(in_v.n[0]);
    out_v.n_and_bx[1] = f32_to_s16(in_v.n[1]);
    out_v.n_and_bx[2] = f32_to_s16(in_v.n[2]);
    out_v.n_and_bx[3] = f32_to_s16(in_v.b[0]);
    out_v.byz[0] = f32_to_s16(in_v.b[1]);
    out_v.byz[1] = f32_to_s16(in_v.b[2]);
    out_v.t1[0] = uint16_t(uint16_t(in_v.c[1]) << 8u) | uint16_t(in_v.c[0]);
    out_v.t1[1] = uint16_t(uint16_t(in_v.c[3]) << 8u) | uint16_t(in_v.c[2]);
}

void pack_vertex_delta(const VtxDelta &in_v, packed_vertex_delta_t &out_v) {
    out_v.dp[0] = in_v.dp[0];
    out_v.dp[1] = in_v.dp[1];
    out_v.dp[2] = in_v.dp[2];
    out_v.dn[0] = f32_to_s16(in_v.dn[0]);
    out_v.dn[1] = f32_to_s16(in_v.dn[1]);
    out_v.dn[2] = f32_to_s16(in_v.dn[2]);
    out_v.db[0] = f32_to_s16(in_v.db[0]);
    out_v.db[1] = f32_to_s16(in_v.db[1]);
    out_v.db[2] = f32_to_s16(in_v.db[2]);
}

} // namespace Ren

Ren::Mesh::Mesh(const char *name, const float *positions, const int vtx_count,
                const uint32_t *indices, const int ndx_count, BufferRef vertex_buf1,
                BufferRef vertex_buf2, BufferRef index_buf, eMeshLoadStatus *load_status,
                ILog *log) {
    name_ = String{name};
    Init(positions, vtx_count, indices, ndx_count, std::move(vertex_buf1),
         std::move(vertex_buf2), std::move(index_buf), load_status, log);
}

Ren::Mesh::Mesh(const char *name, std::istream *data,
                const material_load_callback &on_mat_load, BufferRef vertex_buf1,
                BufferRef vertex_buf2, BufferRef index_buf, BufferRef skin_vertex_buf,
                BufferRef delta_buf, eMeshLoadStatus *load_status, ILog *log) {
    name_ = String{name};
    Init(data, on_mat_load, std::move(vertex_buf1), std::move(vertex_buf2),
         std::move(index_buf), std::move(skin_vertex_buf), std::move(delta_buf),
         load_status, log);
}

void Ren::Mesh::Init(const float *positions, const int vtx_count, const uint32_t *indices,
                     const int ndx_count, BufferRef vertex_buf1, BufferRef vertex_buf2,
                     BufferRef index_buf, eMeshLoadStatus *load_status, ILog *log) {

    if (!positions) {
        // TODO: actually set to default mesh ('error' label like in source engine for
        // example)
        (*load_status) = eMeshLoadStatus::SetToDefault;
        return;
    }

    std::unique_ptr<packed_vertex_data1_t[]> vtx_data1(
        new packed_vertex_data1_t[vtx_count]);
    packed_vertex_data1_t *_vtx_data1 = vtx_data1.get();

    bbox_min_ =
        Vec3f{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max()};
    bbox_max_ =
        Vec3f{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
              std::numeric_limits<float>::lowest()};

    for (int i = 0; i < vtx_count; i++) {
        bbox_min_[0] = std::min(bbox_min_[0], positions[i * 3 + 0]);
        bbox_min_[1] = std::min(bbox_min_[1], positions[i * 3 + 1]);
        bbox_min_[2] = std::min(bbox_min_[2], positions[i * 3 + 2]);

        bbox_max_[0] = std::max(bbox_max_[0], positions[i * 3 + 0]);
        bbox_max_[1] = std::max(bbox_max_[1], positions[i * 3 + 1]);
        bbox_max_[2] = std::max(bbox_max_[2], positions[i * 3 + 2]);

        memcpy(&_vtx_data1[i].p[0], &positions[i * 3], 3 * sizeof(float));
        _vtx_data1[i].t0[0] = 0;
        _vtx_data1[i].t0[1] = 0;
    }

    type_ = eMeshType::Simple;
    flags_ = 0;
    ready_ = true;

    attribs_buf1_.size = vtx_count * sizeof(packed_vertex_data1_t);
    attribs_buf1_.offset = vertex_buf1->AllocRegion(attribs_buf1_.size, _vtx_data1);
    attribs_buf1_.buf = std::move(vertex_buf1);

    attribs_buf2_.size = vtx_count * sizeof(packed_vertex_data2_t);
    attribs_buf2_.offset = vertex_buf2->AllocRegion(attribs_buf2_.size, nullptr);
    attribs_buf2_.buf = std::move(vertex_buf2);

    assert(attribs_buf1_.offset == attribs_buf2_.offset && "Offsets do not match!");

    indices_buf_.size = ndx_count * sizeof(uint32_t);
    indices_buf_.offset = index_buf->AllocRegion(indices_buf_.size, indices);
    indices_buf_.buf = std::move(index_buf);

    (*load_status) = Ren::eMeshLoadStatus::CreatedFromData;
}

void Ren::Mesh::Init(std::istream *data, const material_load_callback &on_mat_load,
                     BufferRef vertex_buf1, BufferRef vertex_buf2, BufferRef index_buf,
                     BufferRef skin_vertex_buf, BufferRef delta_buf,
                     eMeshLoadStatus *load_status, ILog *log) {

    if (data) {
        char mesh_type_str[12];
        std::streampos pos = data->tellg();
        data->read(mesh_type_str, 12);
        data->seekg(pos, std::ios::beg);

        if (strcmp(mesh_type_str, "STATIC_MESH\0") == 0) {
            InitMeshSimple(*data, on_mat_load, std::move(vertex_buf1),
                           std::move(vertex_buf2), std::move(index_buf), log);
        } else if (strcmp(mesh_type_str, "COLORE_MESH\0") == 0) {
            InitMeshColored(*data, on_mat_load, std::move(vertex_buf1),
                            std::move(vertex_buf2), std::move(index_buf), log);
        } else if (strcmp(mesh_type_str, "SKELET_MESH\0") == 0 ||
                   strcmp(mesh_type_str, "SKECOL_MESH\0") == 0) {
            InitMeshSkeletal(*data, on_mat_load, std::move(skin_vertex_buf),
                             std::move(delta_buf), std::move(index_buf), log);
        }

        (*load_status) = eMeshLoadStatus::CreatedFromData;
    } else {
        // TODO: actually set to default mesh ('error' label like in source engine for
        // example)
        (*load_status) = eMeshLoadStatus::SetToDefault;
    }
}

void Ren::Mesh::InitMeshSimple(std::istream &data,
                               const material_load_callback &on_mat_load,
                               BufferRef vertex_buf1, BufferRef vertex_buf2,
                               BufferRef index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "STATIC_MESH\0") == 0);

    type_ = eMeshType::Simple;

    enum {
        MESH_INFO_CHUNK = 0,
        VTX_ATTR_CHUNK,
        VTX_NDX_CHUNK,
        MATERIALS_CHUNK,
        TRI_GROUPS_CHUNK
    };

    struct ChunkPos {
        int offset;
        int length;
    };

    struct Header {
        int num_chunks;
        ChunkPos p[5];
    } file_header = {};

    data.read((char *)&file_header, sizeof(file_header));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_min_ = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_max_ = MakeVec3(temp_f);

    const auto attribs_size = (uint32_t)file_header.p[VTX_ATTR_CHUNK].length;

    attribs_.reset(new char[attribs_size]);
    data.read((char *)attribs_.get(), attribs_size);

    indices_buf_.size = (uint32_t)file_header.p[VTX_NDX_CHUNK].length;
    indices_.reset(new char[indices_buf_.size]);
    data.read((char *)indices_.get(), indices_buf_.size);

    const int materials_count = file_header.p[MATERIALS_CHUNK].length / 64;
    std::array<char, 64> material_names[MaxMeshTriGroupsCount];
    for (int i = 0; i < materials_count; i++) {
        data.read(&material_names[i][0], 64);
    }

    flags_ = 0;

    const int tri_strips_count = file_header.p[TRI_GROUPS_CHUNK].length / 12;
    assert(tri_strips_count == materials_count);
    for (int i = 0; i < tri_strips_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        groups_[i].offset = (int)(index * sizeof(uint32_t));
        groups_[i].num_indices = (int)num_indices;
        groups_[i].flags = 0;

        if (alpha) {
            groups_[i].flags |= MeshHasAlpha;
            flags_ |= MeshHasAlpha;
        }

        groups_[i].mat = on_mat_load(&material_names[i][0]);
    }

    if (tri_strips_count < MaxMeshTriGroupsCount) {
        groups_[tri_strips_count].offset = -1;
    }

    const uint32_t vertex_count = attribs_size / sizeof(orig_vertex_t);
    std::unique_ptr<packed_vertex_data1_t[]> vertices_data1(
        new packed_vertex_data1_t[vertex_count]);
    std::unique_ptr<packed_vertex_data2_t[]> vertices_data2(
        new packed_vertex_data2_t[vertex_count]);

    const auto *orig_vertices = (const orig_vertex_t *)attribs_.get();

    for (uint32_t i = 0; i < vertex_count; i++) {
        pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
        pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
    }

    attribs_buf1_.size = vertex_count * sizeof(packed_vertex_data1_t);
    attribs_buf1_.offset =
        vertex_buf1->AllocRegion(attribs_buf1_.size, vertices_data1.get());
    attribs_buf1_.buf = std::move(vertex_buf1);

    attribs_buf2_.size = vertex_count * sizeof(packed_vertex_data2_t);
    attribs_buf2_.offset =
        vertex_buf2->AllocRegion(attribs_buf2_.size, vertices_data2.get());
    attribs_buf2_.buf = std::move(vertex_buf2);

    assert(attribs_buf1_.offset == attribs_buf2_.offset && "Offsets do not match!");

    indices_buf_.offset = index_buf->AllocRegion(indices_buf_.size, indices_.get());
    indices_buf_.buf = std::move(index_buf);

    ready_ = true;
}

void Ren::Mesh::InitMeshColored(std::istream &data,
                                const material_load_callback &on_mat_load,
                                BufferRef vertex_buf1, BufferRef vertex_buf2,
                                BufferRef index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "COLORE_MESH\0") == 0);

    type_ = eMeshType::Colored;

    enum {
        MESH_INFO_CHUNK = 0,
        VTX_ATTR_CHUNK,
        VTX_NDX_CHUNK,
        MATERIALS_CHUNK,
        TRI_GROUPS_CHUNK
    };

    struct ChunkPos {
        int offset;
        int length;
    };

    struct Header {
        int num_chunks;
        ChunkPos p[5];
    } file_header = {};

    data.read((char *)&file_header, sizeof(file_header));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_min_ = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_max_ = MakeVec3(temp_f);

    const auto attribs_size = (uint32_t)file_header.p[VTX_ATTR_CHUNK].length;

    attribs_.reset(new char[attribs_size]);
    data.read((char *)attribs_.get(), attribs_size);

    indices_buf_.size = (uint32_t)file_header.p[VTX_NDX_CHUNK].length;
    indices_.reset(new char[indices_buf_.size]);
    data.read((char *)indices_.get(), indices_buf_.size);

    const int materials_count = file_header.p[MATERIALS_CHUNK].length / 64;
    std::array<char, 64> material_names[MaxMeshTriGroupsCount];
    for (int i = 0; i < materials_count; i++) {
        data.read(&material_names[i][0], 64);
    }

    flags_ = 0;

    const int tri_strips_count = file_header.p[TRI_GROUPS_CHUNK].length / 12;
    assert(tri_strips_count == materials_count);
    for (int i = 0; i < tri_strips_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        groups_[i].offset = (int)(index * sizeof(uint32_t));
        groups_[i].num_indices = (int)num_indices;
        groups_[i].flags = 0;

        if (alpha) {
            groups_[i].flags |= MeshHasAlpha;
            flags_ |= MeshHasAlpha;
        }

        groups_[i].mat = on_mat_load(&material_names[i][0]);
    }

    if (tri_strips_count < MaxMeshTriGroupsCount) {
        groups_[tri_strips_count].offset = -1;
    }

    assert(attribs_size % sizeof(orig_vertex_colored_t) == 0);
    const uint32_t vertex_count = attribs_size / sizeof(orig_vertex_colored_t);
    std::unique_ptr<packed_vertex_data1_t[]> vertices_data1(
        new packed_vertex_data1_t[vertex_count]);
    std::unique_ptr<packed_vertex_data2_t[]> vertices_data2(
        new packed_vertex_data2_t[vertex_count]);

    const auto *orig_vertices = (const orig_vertex_colored_t *)attribs_.get();

    for (uint32_t i = 0; i < vertex_count; i++) {
        pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
        pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
    }

    attribs_buf1_.size = vertex_count * sizeof(packed_vertex_data1_t);
    attribs_buf1_.offset =
        vertex_buf1->AllocRegion(attribs_buf1_.size, vertices_data1.get());
    attribs_buf1_.buf = std::move(vertex_buf1);

    attribs_buf2_.size = vertex_count * sizeof(packed_vertex_data2_t);
    attribs_buf2_.offset =
        vertex_buf2->AllocRegion(attribs_buf2_.size, vertices_data2.get());
    attribs_buf2_.buf = std::move(vertex_buf2);

    assert(attribs_buf1_.offset == attribs_buf2_.offset && "Offsets do not match!");

    indices_buf_.offset = index_buf->AllocRegion(indices_buf_.size, indices_.get());
    indices_buf_.buf = std::move(index_buf);

    ready_ = true;
}

void Ren::Mesh::InitMeshSkeletal(std::istream &data,
                                 const material_load_callback &on_mat_load,
                                 BufferRef skin_vertex_buf, BufferRef delta_buf,
                                 BufferRef index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "SKELET_MESH\0") == 0 ||
           strcmp(mesh_type_str, "SKECOL_MESH\0") == 0);

    const bool vtx_color_present = strcmp(mesh_type_str, "SKECOL_MESH\0") == 0;

    type_ = eMeshType::Skeletal;

    enum {
        MESH_INFO_CHUNK = 0,
        VTX_ATTR_CHUNK,
        VTX_NDX_CHUNK,
        MATERIALS_CHUNK,
        TRI_GROUPS_CHUNK,
        BONES_CHUNK,
        SHAPE_KEYS_CHUNK
    };

    struct ChunkPos {
        int offset;
        int length;
    };

    struct Header {
        int num_chunks;
        ChunkPos p[7];
    } file_header = {};

    data.read((char *)&file_header.num_chunks, sizeof(int));
    data.read((char *)&file_header.p[0], file_header.num_chunks * sizeof(ChunkPos));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    { // Read bounding box
        float temp_f[3];
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        bbox_min_ = MakeVec3(temp_f);
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        bbox_max_ = MakeVec3(temp_f);
    }

    sk_attribs_buf_.size = (uint32_t)file_header.p[VTX_ATTR_CHUNK].length;
    attribs_.reset(new char[sk_attribs_buf_.size]);
    data.read((char *)attribs_.get(), sk_attribs_buf_.size);

    indices_buf_.size = (uint32_t)file_header.p[VTX_NDX_CHUNK].length;
    indices_.reset(new char[indices_buf_.size]);
    data.read((char *)indices_.get(), indices_buf_.size);

    const int materials_count = file_header.p[MATERIALS_CHUNK].length / 64;
    std::array<char, 64> material_names[MaxMeshTriGroupsCount];
    for (int i = 0; i < materials_count; i++) {
        data.read(&material_names[i][0], 64);
    }

    flags_ = 0;

    const int tri_groups_count = file_header.p[TRI_GROUPS_CHUNK].length / 12;
    assert(tri_groups_count == materials_count);
    for (int i = 0; i < tri_groups_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        groups_[i].offset = int(index * sizeof(uint32_t));
        groups_[i].num_indices = (int)num_indices;
        groups_[i].flags = 0;

        if (alpha) {
            groups_[i].flags |= MeshHasAlpha;
            flags_ |= MeshHasAlpha;
        }

        groups_[i].mat = on_mat_load(&material_names[i][0]);
    }

    if (tri_groups_count < MaxMeshTriGroupsCount) {
        groups_[tri_groups_count].offset = -1;
    }

    skel_.bones_count = file_header.p[BONES_CHUNK].length / (64 + 8 + 12 + 16);
    skel_.bones.reset(new Bone[skel_.bones_count]);

    for (int i = 0; i < skel_.bones_count; i++) {
        float temp_f[4];
        Vec3f temp_v;
        Quatf temp_q;
        data.read(skel_.bones[i].name, 64);
        data.read((char *)&skel_.bones[i].id, sizeof(int));
        data.read((char *)&skel_.bones[i].parent_id, sizeof(int));

        data.read((char *)&temp_f[0], sizeof(float) * 3);
        temp_v = MakeVec3(&temp_f[0]);
        skel_.bones[i].bind_matrix = Translate(skel_.bones[i].bind_matrix, temp_v);
        data.read((char *)&temp_f[0], sizeof(float) * 4);
        temp_q = MakeQuat(&temp_f[0]);
        skel_.bones[i].bind_matrix *= ToMat4(temp_q);
        skel_.bones[i].inv_bind_matrix = Inverse(skel_.bones[i].bind_matrix);

        if (skel_.bones[i].parent_id != -1) {
            skel_.bones[i].cur_matrix =
                skel_.bones[skel_.bones[i].parent_id].inv_bind_matrix *
                skel_.bones[i].bind_matrix;
            const Vec4f pos = skel_.bones[skel_.bones[i].parent_id].inv_bind_matrix *
                              skel_.bones[i].bind_matrix[3];
            skel_.bones[i].head_pos = MakeVec3(&pos[0]);
        } else {
            skel_.bones[i].cur_matrix = skel_.bones[i].bind_matrix;
            skel_.bones[i].head_pos = MakeVec3(&skel_.bones[i].bind_matrix[3][0]);
        }
        skel_.bones[i].cur_comb_matrix = skel_.bones[i].cur_matrix;
        skel_.bones[i].dirty = true;
    }

    if (file_header.num_chunks > 6) {
        uint32_t shape_keyed_vertices_start, shape_keyed_vertices_count;
        data.read((char *)&shape_keyed_vertices_start, sizeof(uint32_t));
        data.read((char *)&shape_keyed_vertices_count, sizeof(uint32_t));

        skel_.shapes_count =
            (file_header.p[SHAPE_KEYS_CHUNK].length - 2 * sizeof(uint32_t)) /
            (64 + shape_keyed_vertices_count *
                      (3 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float)));

        deltas_.reset(
            new VtxDelta[(size_t)skel_.shapes_count * shape_keyed_vertices_count]);
        skel_.shapes.reset(new ShapeKey[skel_.shapes_count]);

        for (int i = 0; i < skel_.shapes_count; i++) {
            ShapeKey &sh_key = skel_.shapes[i];

            data.read(sh_key.name, 64);
            sh_key.delta_offset = shape_keyed_vertices_count * i;
            sh_key.delta_count = shape_keyed_vertices_count;
            sh_key.cur_weight_packed = 0;

            data.read((char *)&deltas_[sh_key.delta_offset],
                      sh_key.delta_count * sizeof(VtxDelta));
        }

        std::unique_ptr<packed_vertex_delta_t[]> packed_deltas(
            new packed_vertex_delta_t[(size_t)skel_.shapes_count *
                                      shape_keyed_vertices_count]);

        for (uint32_t i = 0; i < skel_.shapes_count * shape_keyed_vertices_count; i++) {
            pack_vertex_delta(deltas_[i], packed_deltas[i]);
        }

        sk_deltas_buf_.size = uint32_t(skel_.shapes_count * shape_keyed_vertices_count *
                                       sizeof(packed_vertex_delta_t));
        sk_deltas_buf_.offset =
            delta_buf->AllocRegion(sk_deltas_buf_.size, packed_deltas.get());
        sk_deltas_buf_.buf = std::move(delta_buf);
    }

    // assert(max_gpu_bones);
    /*if (bones.size() <= (size_t)max_gpu_bones) {
        for (size_t s = 0; s < groups_.size(); s++) {
            if (groups_[s].offset == -1) {
                break;
            }
            BoneGroup grp;
            for (int i = 0; i < skel_.bones_count; i++) {
                grp.bone_ids.push_back((uint32_t)i);
            }
            grp.strip_ids.push_back((uint32_t)s);
            grp.strip_ids.push_back(groups_[s].offset / sizeof(uint32_t));
            grp.strip_ids.push_back(groups_[s].num_indices);
            skel_.bone_groups.push_back(grp);
        }
    } else {
        SplitMesh(max_gpu_bones);
    }*/

    const uint32_t vertex_count =
        sk_attribs_buf_.size / (vtx_color_present ? sizeof(orig_vertex_skinned_colored_t)
                                                  : sizeof(orig_vertex_skinned_t));
    std::unique_ptr<packed_vertex_skinned_t[]> vertices(
        new packed_vertex_skinned_t[vertex_count]);

    if (vtx_color_present) {
        const auto *orig_vertices = (const orig_vertex_skinned_colored_t *)attribs_.get();
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    } else {
        const auto *orig_vertices = (const orig_vertex_skinned_t *)attribs_.get();
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    }

    // allocate space for untransformed vertices
    sk_attribs_buf_.size = vertex_count * sizeof(packed_vertex_skinned_t);
    sk_attribs_buf_.offset =
        skin_vertex_buf->AllocRegion(sk_attribs_buf_.size, vertices.get());
    sk_attribs_buf_.buf = std::move(skin_vertex_buf);

    indices_buf_.offset = index_buf->AllocRegion(indices_buf_.size, indices_.get());

    ready_ = true;
}

/*void Ren::Mesh::SplitMesh(int bones_limit, ILog *log) {
    assert(type_ == MeshSkeletal);

    std::vector<int> bone_ids;
    bone_ids.reserve(12);

    auto *vtx_attribs = (float *)attribs_.get();
    size_t num_vtx_attribs = attribs_buf1_.size / sizeof(float);
    auto *vtx_indices = (unsigned short *)indices_.get();
    size_t num_vtx_indices = indices_buf_.size / sizeof(unsigned short);

    clock_t t1 = clock();

    for (size_t s = 0; s < groups_.size(); s++) {
        if (groups_[s].offset == -1)
            break;
        for (int i = (int)groups_[s].offset / 2;
             i < (int)(groups_[s].offset / 2 + groups_[s].num_indices - 2); i += 1) {
            bone_ids.clear();
            if (vtx_indices[i] == vtx_indices[i + 1] ||
                vtx_indices[i + 1] == vtx_indices[i + 2]) {
                continue;
            }
            for (int j = i; j < i + 3; j++) {
                for (int k = 8; k < 12; k += 1) {
                    if (vtx_attribs[vtx_indices[j] * 16 + k + 4] > 0.00001f) {
                        if (std::find(bone_ids.begin(), bone_ids.end(),
                                      (int)vtx_attribs[vtx_indices[j] * 16 + k]) ==
                            bone_ids.end()) {
                            bone_ids.push_back((int)vtx_attribs[vtx_indices[j] * 16 + k]);
                        }
                    }
                }
            }
            Ren::BoneGroup *best_fit = nullptr;
            int best_k = std::numeric_limits<int>::max();
            for (BoneGroup &g : skel_.bone_groups) {
                bool b = true;
                int k = 0;
                for (int bone_id : bone_ids) {
                    if (std::find(g.bone_ids.begin(), g.bone_ids.end(), bone_id) ==
                        g.bone_ids.end()) {
                        k++;
                        if (g.bone_ids.size() + k > (size_t)bones_limit) {
                            b = false;
                            break;
                        }
                    }
                }
                if (b && k < best_k) {
                    best_k = k;
                    best_fit = &g;
                }
            }

            if (!best_fit) {
                skel_.bone_groups.emplace_back();
                best_fit = &skel_.bone_groups[skel_.bone_groups.size() - 1];
            }
            for (int bone_id : bone_ids) {
                if (std::find(best_fit->bone_ids.begin(), best_fit->bone_ids.end(),
                              bone_id) == best_fit->bone_ids.end()) {
                    best_fit->bone_ids.push_back(bone_id);
                }
            }
            if (!best_fit->strip_ids.empty() &&
                s == best_fit->strip_ids[best_fit->strip_ids.size() - 3] &&
                best_fit->strip_ids[best_fit->strip_ids.size() - 2] +
                        best_fit->strip_ids[best_fit->strip_ids.size() - 1] - 0 ==
                    i) {
                best_fit->strip_ids[best_fit->strip_ids.size() - 1]++;
            } else {
                best_fit->strip_ids.push_back((int)s);
                if ((i - groups_[s].offset / 2) % 2 == 0) {
                    best_fit->strip_ids.push_back(i);
                } else {
                    best_fit->strip_ids.push_back(-i);
                }
                best_fit->strip_ids.push_back(1);
            }
        }
    }

    log->Info("%li", clock() - t1);
    t1 = clock();

    std::vector<unsigned short> new_indices;
    new_indices.reserve((size_t)(num_vtx_indices * 1.2f));
    for (BoneGroup &g : skel_.bone_groups) {
        std::sort(g.bone_ids.begin(), g.bone_ids.end());
        int cur_s = g.strip_ids[0];

        for (size_t i = 0; i < g.strip_ids.size(); i += 3) {
            bool sign = g.strip_ids[i + 1] >= 0;
            if (!sign) {
                g.strip_ids[i + 1] = -g.strip_ids[i + 1];
            }
            int beg = g.strip_ids[i + 1];
            int end = g.strip_ids[i + 1] + g.strip_ids[i + 2] + 2;

            if (g.strip_ids[i] == cur_s && i > 0) {
                new_indices.push_back(new_indices.back());
                new_indices.push_back(vtx_indices[beg]);
                g.strip_ids[i + 2 - 3] += 2;
                if ((!sign && (g.strip_ids[i + 2 - 3] % 2 == 0)) ||
                    (sign && (g.strip_ids[i + 2 - 3] % 2 != 0))) {
                    g.strip_ids[i + 2 - 3] += 1;
                    new_indices.push_back(vtx_indices[beg]);
                }
                g.strip_ids[i + 2 - 3] += g.strip_ids[i + 2] + 2;
                g.strip_ids.erase(g.strip_ids.begin() + i, g.strip_ids.begin() + i + 3);
                i -= 3;
            } else {
                cur_s = g.strip_ids[i];
                g.strip_ids[i + 2] += 2;
                g.strip_ids[i + 1] = (int)new_indices.size();
                if (!sign) {
                    g.strip_ids[i + 2] += 1;
                    new_indices.push_back(vtx_indices[beg]);
                }
            }
            new_indices.insert(new_indices.end(), &vtx_indices[beg], &vtx_indices[end]);
        }
    }
    new_indices.shrink_to_fit();
    log->Info("%li", clock() - t1);
    t1 = clock();

    clock_t find_time = 0;

    std::vector<bool> done_bools(num_vtx_attribs);
    std::vector<float> new_attribs(vtx_attribs, vtx_attribs + num_vtx_attribs);
    for (BoneGroup &g : skel_.bone_groups) {
        std::vector<int> moved_points;
        for (size_t i = 0; i < g.strip_ids.size(); i += 3) {
            for (int j = g.strip_ids[i + 1]; j < g.strip_ids[i + 1] + g.strip_ids[i + 2];
                 j++) {
                if (done_bools[new_indices[j]]) {
                    if (&g - &skel_.bone_groups[0] != 0) {
                        bool b = false;
                        for (size_t k = 0; k < moved_points.size(); k += 2) {
                            if (new_indices[j] == moved_points[k]) {
                                new_indices[j] = (unsigned short)moved_points[k + 1];
                                b = true;
                                break;
                            }
                        }
                        if (!b) {
                            new_attribs.insert(new_attribs.end(),
                                               &vtx_attribs[new_indices[j] * 16],
                                               &vtx_attribs[new_indices[j] * 16] + 16);
                            moved_points.push_back(new_indices[j]);
                            moved_points.push_back((int)new_attribs.size() / 16 - 1);
                            new_indices[j] =
                                (unsigned short)(new_attribs.size() / 16 - 1);
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                } else {
                    done_bools[new_indices[j]] = true;
                }
                for (int k = 8; k < 12; k += 1) {
                    if (new_attribs[new_indices[j] * 16 + k + 4] > 0.0f) {
                        int bone_ndx =
                            (int)(std::find(g.bone_ids.begin(), g.bone_ids.end(),
                                            (int)new_attribs[new_indices[j] * 16 + k]) -
                                  g.bone_ids.begin());
                        new_attribs[new_indices[j] * 16 + k] = (float)bone_ndx;
                    }
                }
            }
        }
    }

    log->Info("---------------------------");
    for (BoneGroup &g : skel_.bone_groups) {
        log->Info("%u", (unsigned int)g.strip_ids.size() / 3);
    }
    log->Info("---------------------------");

    log->Info("%li", clock() - t1);
    log->Info("find_time = %li", find_time);
    log->Info("after bone broups2");

    indices_buf_.size = (uint32_t)(new_indices.size() * sizeof(unsigned short));
    indices_.reset(new char[indices_buf_.size]);
    memcpy(indices_.get(), &new_indices[0], indices_buf_.size);

    attribs_buf1_.size = (uint32_t)(new_attribs.size() * sizeof(float));
    attribs_.reset(new char[attribs_buf1_.size]);
    memcpy(attribs_.get(), &new_attribs[0], attribs_buf1_.size);
}*/

#ifdef _MSC_VER
#pragma warning(pop)
#endif