#include "Mesh.h"

#include <ctime>
#include <istream>

#include "ApiContext.h"
#include "Pipeline.h"
#include "Utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
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
    out_v.t0[1] = f32_to_f16(in_v.t0[1]);
    out_v.t1[0] = f32_to_f16(in_v.t1[0]);
    out_v.t1[1] = f32_to_f16(in_v.t1[1]);
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
    out_v.t0[1] = f32_to_f16(in_v.t0[1]);
    out_v.t1[0] = uint16_t(uint16_t(in_v.c[1]) << 8u) | uint16_t(in_v.c[0]);
    out_v.t1[1] = uint16_t(uint16_t(in_v.c[3]) << 8u) | uint16_t(in_v.c[2]);
}

void pack_vertex_data1(const orig_vertex_t &in_v, packed_vertex_data1_t &out_v) {
    out_v.p[0] = in_v.p[0];
    out_v.p[1] = in_v.p[1];
    out_v.p[2] = in_v.p[2];
    out_v.t0[0] = f32_to_f16(in_v.t0[0]);
    out_v.t0[1] = f32_to_f16(in_v.t0[1]);
}

void pack_vertex_data2(const orig_vertex_t &in_v, packed_vertex_data2_t &out_v) {
    out_v.n_and_bx[0] = f32_to_s16(in_v.n[0]);
    out_v.n_and_bx[1] = f32_to_s16(in_v.n[1]);
    out_v.n_and_bx[2] = f32_to_s16(in_v.n[2]);
    out_v.n_and_bx[3] = f32_to_s16(in_v.b[0]);
    out_v.byz[0] = f32_to_s16(in_v.b[1]);
    out_v.byz[1] = f32_to_s16(in_v.b[2]);
    out_v.t1[0] = f32_to_f16(in_v.t1[0]);
    out_v.t1[1] = f32_to_f16(in_v.t1[1]);
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

void pack_vertex(const orig_vertex_skinned_colored_t &in_v, packed_vertex_skinned_t &out_v) {
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

Ren::Mesh::Mesh(std::string_view name, const float *positions, const int vtx_count, const uint32_t *indices,
                const int ndx_count, ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf,
                eMeshLoadStatus *load_status, ILog *log) {
    name_ = String{name};
    Init(positions, vtx_count, indices, ndx_count, api_ctx, vertex_buf1, vertex_buf2, index_buf, load_status, log);
}

Ren::Mesh::Mesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load,
                ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf,
                BufRef &skin_vertex_buf, BufRef &delta_buf, eMeshLoadStatus *load_status, ILog *log) {
    name_ = String{name};
    Init(data, on_mat_load, api_ctx, vertex_buf1, vertex_buf2, index_buf, skin_vertex_buf, delta_buf, load_status, log);
}

void Ren::Mesh::Init(const float *positions, const int vtx_count, const uint32_t *indices, const int ndx_count,
                     ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf,
                     eMeshLoadStatus *load_status, ILog *log) {

    if (!positions) {
        (*load_status) = eMeshLoadStatus::Error;
        return;
    }

    attribs_buf1_.size = vtx_count * sizeof(packed_vertex_data1_t);
    attribs_buf2_.size = vtx_count * sizeof(packed_vertex_data2_t);
    indices_buf_.size = ndx_count * sizeof(uint32_t);

    const uint32_t total_mem_required = attribs_buf1_.size + indices_buf_.size;
    auto stage_buf = Buffer{"Temp Stage Buf", api_ctx, eBufType::Upload, total_mem_required};

    auto *_vtx_data1 = reinterpret_cast<packed_vertex_data1_t *>(stage_buf.Map());

    bbox_min_ =
        Vec3f{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    bbox_max_ = Vec3f{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
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

    auto *_ndx_data = reinterpret_cast<uint32_t *>(_vtx_data1 + vtx_count);
    assert(uintptr_t(_ndx_data) % alignof(uint32_t) == 0);
    memcpy(_ndx_data, indices, indices_buf_.size);

    stage_buf.Unmap();

    type_ = eMeshType::Simple;
    flags_ = {};
    ready_ = true;

    CommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

    attribs_buf1_.sub = vertex_buf1->AllocSubRegion(attribs_buf1_.size, 16, name_, &stage_buf, cmd_buf);
    attribs_buf1_.buf = vertex_buf1;

    // allocate empty data in buffer 2 (for index matching)
    attribs_buf2_.sub = vertex_buf2->AllocSubRegion(attribs_buf2_.size, 16, name_, nullptr);
    attribs_buf2_.buf = vertex_buf2;

    assert(attribs_buf1_.sub.offset == attribs_buf2_.sub.offset && "Offsets do not match!");

    indices_buf_.sub = index_buf->AllocSubRegion(indices_buf_.size, 4, name_, &stage_buf, cmd_buf, attribs_buf1_.size);
    indices_buf_.buf = index_buf;

    api_ctx->EndSingleTimeCommands(cmd_buf);
    stage_buf.FreeImmediate();

    (*load_status) = eMeshLoadStatus::CreatedFromData;
}

void Ren::Mesh::Init(std::istream *data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                     BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, BufRef &skin_vertex_buf,
                     BufRef &delta_buf, eMeshLoadStatus *load_status, ILog *log) {

    if (data) {
        char mesh_type_str[12];
        std::streampos pos = data->tellg();
        data->read(mesh_type_str, 12);
        data->seekg(pos, std::ios::beg);

        if (strcmp(mesh_type_str, "STATIC_MESH\0") == 0) {
            InitMeshSimple(*data, on_mat_load, api_ctx, vertex_buf1, vertex_buf2, index_buf, log);
        } else if (strcmp(mesh_type_str, "COLORE_MESH\0") == 0) {
            InitMeshColored(*data, on_mat_load, api_ctx, vertex_buf1, vertex_buf2, index_buf, log);
        } else if (strcmp(mesh_type_str, "SKELET_MESH\0") == 0 || strcmp(mesh_type_str, "SKECOL_MESH\0") == 0) {
            InitMeshSkeletal(*data, on_mat_load, api_ctx, skin_vertex_buf, delta_buf, index_buf, log);
        }

        (*load_status) = eMeshLoadStatus::CreatedFromData;
    } else {
        (*load_status) = eMeshLoadStatus::Error;
    }
}

void Ren::Mesh::InitMeshSimple(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                               BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "STATIC_MESH\0") == 0);

    type_ = eMeshType::Simple;

    struct Header {
        int num_chunks;
        MeshChunkPos p[5];
    } file_header = {};
    data.read((char *)&file_header, sizeof(file_header));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_min_ = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_max_ = MakeVec3(temp_f);

    const auto attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);

    attribs_.resize(attribs_size / sizeof(float));
    data.read((char *)attribs_.data(), attribs_size);

    const auto index_data_size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    indices_.resize(index_data_size / sizeof(uint32_t));
    data.read((char *)indices_.data(), index_data_size);

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &name = material_names.emplace_back();
        data.read(name.data(), 64);
    }

    flags_ = {};

    const int tri_groups_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_groups_count == materials_count);
    for (int i = 0; i < tri_groups_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        auto &grp = groups_.emplace_back();
        grp.byte_offset = int(index * sizeof(uint32_t));
        grp.num_indices = num_indices;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            flags_ |= eMeshFlags::HasAlpha;
        }

        std::tie(grp.front_mat, grp.back_mat) = on_mat_load(&material_names[i][0]);
    }

    InitBufferData(api_ctx, vertex_buf1, vertex_buf2, index_buf);

    ready_ = true;
}

void Ren::Mesh::InitMeshColored(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                                BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "COLORE_MESH\0") == 0);

    type_ = eMeshType::Colored;

    struct Header {
        int num_chunks;
        MeshChunkPos p[5];
    } file_header = {};
    data.read((char *)&file_header, sizeof(file_header));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_min_ = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    bbox_max_ = MakeVec3(temp_f);

    const auto attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);

    attribs_.resize(attribs_size / sizeof(float));
    data.read((char *)attribs_.data(), attribs_size);

    const auto index_data_size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    indices_.resize(index_data_size / sizeof(uint32_t));
    data.read((char *)indices_.data(), index_data_size);

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &name = material_names.emplace_back();
        data.read(name.data(), 64);
    }

    flags_ = {};

    const int tri_strips_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_strips_count == materials_count);
    for (int i = 0; i < tri_strips_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        auto &grp = groups_.emplace_back();
        grp.byte_offset = int(index * sizeof(uint32_t));
        grp.num_indices = num_indices;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            flags_ |= eMeshFlags::HasAlpha;
        }

        std::tie(grp.front_mat, grp.back_mat) = on_mat_load(&material_names[i][0]);
    }

    assert(attribs_size % sizeof(orig_vertex_colored_t) == 0);
    const uint32_t vertex_count = attribs_size / sizeof(orig_vertex_colored_t);

    attribs_buf1_.size = vertex_count * sizeof(packed_vertex_data1_t);
    attribs_buf2_.size = vertex_count * sizeof(packed_vertex_data2_t);
    indices_buf_.size = index_data_size;

    const uint32_t total_mem_required = attribs_buf1_.size + attribs_buf2_.size + indices_buf_.size;
    auto stage_buf = Buffer{"Temp Stage Buf", api_ctx, eBufType::Upload, total_mem_required};

    { // Update staging buffer
        auto *vertices_data1 = reinterpret_cast<packed_vertex_data1_t *>(stage_buf.Map());
        auto *vertices_data2 = reinterpret_cast<packed_vertex_data2_t *>(vertices_data1 + vertex_count);
        assert(uintptr_t(vertices_data2) % alignof(packed_vertex_data2_t) == 0);
        auto *index_data = reinterpret_cast<uint32_t *>(vertices_data2 + vertex_count);
        assert(uintptr_t(index_data) % alignof(uint32_t) == 0);

        const auto *orig_vertices = reinterpret_cast<const orig_vertex_colored_t *>(attribs_.data());

        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
            pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
        }
        memcpy(index_data, indices_.data(), indices_buf_.size);
        stage_buf.Unmap();
    }

    CommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

    attribs_buf1_.sub = vertex_buf1->AllocSubRegion(attribs_buf1_.size, 16, name_, &stage_buf, cmd_buf, 0 /* offset */);
    attribs_buf1_.buf = vertex_buf1;

    attribs_buf2_.sub =
        vertex_buf2->AllocSubRegion(attribs_buf2_.size, 16, name_, &stage_buf, cmd_buf, attribs_buf1_.size);
    attribs_buf2_.buf = vertex_buf2;

    indices_buf_.sub = index_buf->AllocSubRegion(indices_buf_.size, 4, name_, &stage_buf, cmd_buf,
                                                 attribs_buf1_.size + attribs_buf2_.size);
    indices_buf_.buf = index_buf;
    assert(attribs_buf1_.sub.offset == attribs_buf2_.sub.offset && "Offsets do not match!");

    api_ctx->EndSingleTimeCommands(cmd_buf);
    stage_buf.FreeImmediate();

    ready_ = true;
}

void Ren::Mesh::InitMeshSkeletal(std::istream &data, const material_load_callback &on_mat_load, ApiContext *api_ctx,
                                 BufRef &skin_vertex_buf, BufRef &delta_buf, BufRef &index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    assert(strcmp(mesh_type_str, "SKELET_MESH\0") == 0 || strcmp(mesh_type_str, "SKECOL_MESH\0") == 0);

    const bool vtx_color_present = strcmp(mesh_type_str, "SKECOL_MESH\0") == 0;

    type_ = eMeshType::Skeletal;

    struct Header {
        int num_chunks;
        MeshChunkPos p[7];
    } file_header = {};
    data.read((char *)&file_header.num_chunks, sizeof(int));
    data.read((char *)&file_header.p[0], std::streamsize(file_header.num_chunks * sizeof(MeshChunkPos)));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    { // Read bounding box
        float temp_f[3];
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        bbox_min_ = MakeVec3(temp_f);
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        bbox_max_ = MakeVec3(temp_f);
    }

    const uint32_t sk_attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);
    attribs_.resize(sk_attribs_size / sizeof(float));
    data.read((char *)attribs_.data(), sk_attribs_size);

    indices_buf_.size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    indices_.resize(indices_buf_.size / sizeof(uint32_t));
    data.read((char *)indices_.data(), indices_buf_.size);

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &name = material_names.emplace_back();
        data.read(name.data(), 64);
    }

    flags_ = {};

    const int tri_groups_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_groups_count == materials_count);
    for (int i = 0; i < tri_groups_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        auto &grp = groups_.emplace_back();
        grp.byte_offset = int(index * sizeof(uint32_t));
        grp.num_indices = num_indices;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            flags_ |= eMeshFlags::HasAlpha;
        }

        std::tie(grp.front_mat, grp.back_mat) = on_mat_load(&material_names[i][0]);
    }

    const int bones_count = file_header.p[int(eMeshFileChunk::Bones)].length / (64 + 8 + 12 + 16);
    skel_.bones.resize(bones_count);

    for (int i = 0; i < int(skel_.bones.size()); i++) {
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
                skel_.bones[skel_.bones[i].parent_id].inv_bind_matrix * skel_.bones[i].bind_matrix;
            const Vec4f pos = skel_.bones[skel_.bones[i].parent_id].inv_bind_matrix * skel_.bones[i].bind_matrix[3];
            skel_.bones[i].head_pos = MakeVec3(&pos[0]);
        } else {
            skel_.bones[i].cur_matrix = skel_.bones[i].bind_matrix;
            skel_.bones[i].head_pos = MakeVec3(&skel_.bones[i].bind_matrix[3][0]);
        }
        skel_.bones[i].cur_comb_matrix = skel_.bones[i].cur_matrix;
        skel_.bones[i].dirty = true;
    }

    const uint32_t vertex_count =
        sk_attribs_size / (vtx_color_present ? sizeof(orig_vertex_skinned_colored_t) : sizeof(orig_vertex_skinned_t));
    sk_attribs_buf_.size = vertex_count * sizeof(packed_vertex_skinned_t);

    // TODO: This is incorrect!
    const uint32_t total_mem_required =
        attribs_buf1_.size + attribs_buf2_.size + indices_buf_.size + sk_attribs_buf_.size;
    auto stage_buf = Buffer{"Temp Stage Buf", api_ctx, eBufType::Upload, total_mem_required};

    uint8_t *stage_buf_ptr = stage_buf.Map();
    uint32_t stage_buf_off = 0;

    uint32_t delta_buf_off = 0;

    const bool shape_data_present = (file_header.num_chunks > 6);
    if (shape_data_present) {
        uint32_t shape_keyed_vertices_start, shape_keyed_vertices_count;
        data.read((char *)&shape_keyed_vertices_start, sizeof(uint32_t));
        data.read((char *)&shape_keyed_vertices_count, sizeof(uint32_t));

        const int shapes_count =
            int(file_header.p[int(eMeshFileChunk::ShapeKeys)].length - 2 * sizeof(uint32_t)) /
            (64 + shape_keyed_vertices_count * (3 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float)));

        deltas_.resize(shapes_count * shape_keyed_vertices_count);
        skel_.shapes.resize(shapes_count);

        for (int i = 0; i < shapes_count; i++) {
            ShapeKey &sh_key = skel_.shapes[i];

            data.read(sh_key.name, 64);
            sh_key.delta_offset = shape_keyed_vertices_count * i;
            sh_key.delta_count = shape_keyed_vertices_count;
            sh_key.cur_weight_packed = 0;

            data.read((char *)&deltas_[sh_key.delta_offset], std::streamsize(sh_key.delta_count * sizeof(VtxDelta)));
        }

        sk_deltas_buf_.size = uint32_t(shapes_count * shape_keyed_vertices_count * sizeof(packed_vertex_delta_t));

        assert(stage_buf.size() - stage_buf_off >= sk_deltas_buf_.size);
        auto *packed_deltas = reinterpret_cast<packed_vertex_delta_t *>(stage_buf_ptr + stage_buf_off);
        delta_buf_off = stage_buf_off;
        stage_buf_off += sk_deltas_buf_.size;

        for (uint32_t i = 0; i < shapes_count * shape_keyed_vertices_count; i++) {
            pack_vertex_delta(deltas_[i], packed_deltas[i]);
        }
    }

    assert(stage_buf.size() - stage_buf_off >= sk_attribs_buf_.size);
    auto *vertices = reinterpret_cast<packed_vertex_skinned_t *>(stage_buf_ptr + stage_buf_off);
    const uint32_t vertices_off = stage_buf_off;
    stage_buf_off += sk_attribs_buf_.size;

    if (vtx_color_present) {
        const auto *orig_vertices = reinterpret_cast<const orig_vertex_skinned_colored_t *>(attribs_.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    } else {
        const auto *orig_vertices = reinterpret_cast<const orig_vertex_skinned_t *>(attribs_.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    }

    assert(stage_buf.size() - stage_buf_off >= indices_buf_.size);
    auto *index_data = reinterpret_cast<uint32_t *>(stage_buf_ptr + stage_buf_off);
    const uint32_t indices_off = stage_buf_off;
    stage_buf_off += indices_buf_.size;
    memcpy(index_data, indices_.data(), indices_buf_.size);

    stage_buf.Unmap();

    CommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

    if (shape_data_present) {
        sk_deltas_buf_.sub =
            delta_buf->AllocSubRegion(sk_deltas_buf_.size, 16, name_, &stage_buf, cmd_buf, delta_buf_off);
        sk_deltas_buf_.buf = delta_buf;
    }

    // allocate untransformed vertices
    sk_attribs_buf_.sub =
        skin_vertex_buf->AllocSubRegion(sk_attribs_buf_.size, 16, name_, &stage_buf, cmd_buf, vertices_off);
    sk_attribs_buf_.buf = skin_vertex_buf;

    indices_buf_.sub = index_buf->AllocSubRegion(indices_buf_.size, 4, name_, &stage_buf, cmd_buf, indices_off);
    indices_buf_.buf = index_buf;

    api_ctx->EndSingleTimeCommands(cmd_buf);
    stage_buf.FreeImmediate();

    ready_ = true;
}

void Ren::Mesh::InitBufferData(ApiContext *api_ctx, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf) {
    const uint32_t vertex_count = uint32_t(attribs_.size() * sizeof(float)) / sizeof(orig_vertex_t);

    attribs_buf1_.size = vertex_count * sizeof(packed_vertex_data1_t);
    attribs_buf2_.size = vertex_count * sizeof(packed_vertex_data2_t);
    indices_buf_.size = uint32_t(indices_.size() * sizeof(uint32_t));

    const uint32_t total_mem_required = attribs_buf1_.size + attribs_buf2_.size + indices_buf_.size;
    auto stage_buf = Buffer{"Temp Stage Buf", api_ctx, eBufType::Upload, total_mem_required};

    { // Update staging buffer
        auto *vertices_data1 = reinterpret_cast<packed_vertex_data1_t *>(stage_buf.Map());
        auto *vertices_data2 = reinterpret_cast<packed_vertex_data2_t *>(vertices_data1 + vertex_count);
        assert(uintptr_t(vertices_data2) % alignof(packed_vertex_data2_t) == 0);
        auto *index_data = reinterpret_cast<uint32_t *>(vertices_data2 + vertex_count);
        assert(uintptr_t(index_data) % alignof(uint32_t) == 0);

        const auto *orig_vertices = reinterpret_cast<const orig_vertex_t *>(attribs_.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
            pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
        }
        memcpy(index_data, indices_.data(), indices_buf_.size);
        stage_buf.Unmap();
    }

    { // Copy buffer data
        CommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

        attribs_buf1_.sub =
            vertex_buf1->AllocSubRegion(attribs_buf1_.size, 16, name_, &stage_buf, cmd_buf, 0 /* offset */);
        attribs_buf1_.buf = vertex_buf1;

        attribs_buf2_.sub =
            vertex_buf2->AllocSubRegion(attribs_buf2_.size, 16, name_, &stage_buf, cmd_buf, attribs_buf1_.size);
        attribs_buf2_.buf = vertex_buf2;

        indices_buf_.sub = index_buf->AllocSubRegion(indices_buf_.size, 4, name_, &stage_buf, cmd_buf,
                                                     attribs_buf1_.size + attribs_buf2_.size);
        indices_buf_.buf = index_buf;
        assert(attribs_buf1_.sub.offset == attribs_buf2_.sub.offset && "Offsets do not match!");

        api_ctx->EndSingleTimeCommands(cmd_buf);
        stage_buf.FreeImmediate();
    }
}

void Ren::Mesh::ReleaseBufferData() {
    attribs_buf1_ = {};
    attribs_buf2_ = {};
    indices_buf_ = {};
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif