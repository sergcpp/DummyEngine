#include "Mesh.h"

#include <ctime>
#include <istream>

#include "ApiContext.h"
#include "Pipeline.h"
#include "ResizableBuffer.h"
#include "utils/Utils.h"

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
static_assert(sizeof(orig_vertex_t) == 52);

struct orig_vertex_colored_t {
    float p[3];
    float n[3];
    float b[3];
    float t0[2];
    uint8_t c[4];
};
static_assert(sizeof(orig_vertex_colored_t) == 48);

struct orig_vertex_skinned_t {
    orig_vertex_t v;
    int32_t bone_indices[4];
    float bone_weights[4];
};
static_assert(sizeof(orig_vertex_skinned_t) == 84);

struct orig_vertex_skinned_colored_t {
    orig_vertex_colored_t v;
    int32_t bone_indices[4];
    float bone_weights[4];
};
static_assert(sizeof(orig_vertex_skinned_t) == 84);

struct packed_vertex_data1_t {
    float p[3];
    uint16_t t0[2];
};
static_assert(sizeof(packed_vertex_data1_t) == 16);

struct packed_vertex_data2_t {
    int16_t n_and_bx[4];
    int16_t byz[2];
    uint16_t t1[2];
};
static_assert(sizeof(packed_vertex_data2_t) == 16);

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_data1_t, t0) % 4 == 0);
static_assert(offsetof(packed_vertex_data2_t, n_and_bx) % 4 == 0);
static_assert(offsetof(packed_vertex_data2_t, byz) % 4 == 0);
static_assert(offsetof(packed_vertex_data2_t, t1) % 4 == 0);

struct packed_vertex_t {
    float p[3];
    int16_t n_and_bx[4];
    int16_t byz[2];
    uint16_t t0[2];
    uint16_t t1[2];
};
static_assert(sizeof(packed_vertex_t) == 32);

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_t, n_and_bx) % 4 == 0);
static_assert(offsetof(packed_vertex_t, byz) % 4 == 0);
static_assert(offsetof(packed_vertex_t, t0) % 4 == 0);
static_assert(offsetof(packed_vertex_t, t1) % 4 == 0);

struct packed_vertex_skinned_t {
    packed_vertex_t v;
    uint16_t bone_indices[4];
    uint16_t bone_weights[4];
};
static_assert(sizeof(packed_vertex_skinned_t) == 48);

// make sure attributes are aligned to 4-bytes
static_assert(offsetof(packed_vertex_skinned_t, v.n_and_bx) % 4 == 0);
static_assert(offsetof(packed_vertex_skinned_t, v.byz) % 4 == 0);
static_assert(offsetof(packed_vertex_skinned_t, v.t0) % 4 == 0);
static_assert(offsetof(packed_vertex_skinned_t, v.t1) % 4 == 0);
static_assert(offsetof(packed_vertex_skinned_t, bone_indices) % 4 == 0);
static_assert(offsetof(packed_vertex_skinned_t, bone_weights) % 4 == 0);

struct packed_vertex_delta_t {
    float dp[3];
    int16_t dn[3]; // normalized, delta normal is limited but it is fine
    int16_t db[3];
};
static_assert(sizeof(packed_vertex_delta_t) == 24);

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

void pack_vertex_delta(const vtx_delta_t &in_v, packed_vertex_delta_t &out_v) {
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

bool Ren::Mesh_Init(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                    std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                    ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ResizableBuffer &skin_vertex_buf,
                    ResizableBuffer &delta_buf, ILog *log) {
    char mesh_type_str[12];
    std::streampos pos = data.tellg();
    data.read(mesh_type_str, 12);
    if (data.gcount() != 12) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }
    data.seekg(pos, std::ios::beg);

    if (strncmp(mesh_type_str, "STATIC_MESH", 11) == 0) {
        return Mesh_InitSimple(api, mesh_main, mesh_cold, name, data, on_mat_load, vertex_buf1, vertex_buf2, index_buf,
                               log);
    } else if (strncmp(mesh_type_str, "COLORE_MESH", 11) == 0) {
        return Mesh_InitColored(api, mesh_main, mesh_cold, name, data, on_mat_load, vertex_buf1, vertex_buf2, index_buf,
                                log);
    } else if (strncmp(mesh_type_str, "SKELET_MESH", 11) == 0 || strncmp(mesh_type_str, "SKECOL_MESH", 11) == 0) {
        return Mesh_InitSkeletal(api, mesh_main, mesh_cold, name, data, on_mat_load, skin_vertex_buf, delta_buf,
                                 index_buf, log);
    }

    log->Error("Invalid mesh (%s)!", name.c_str());
    return false;
}

bool Ren::Mesh_InitSimple(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                          std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                          ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    if (data.gcount() != 12 || strncmp(mesh_type_str, "STATIC_MESH", 11) != 0) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }

    mesh_main.type = eMeshType::Simple;
    mesh_cold.name = name;

    struct Header {
        int num_chunks;
        mesh_chunk_pos_t p[5];
    } file_header = {};
    data.read((char *)&file_header, sizeof(file_header));
    if (data.gcount() != sizeof(file_header)) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    if (data.gcount() != 3 * sizeof(float)) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }
    mesh_cold.bbox_min = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    if (data.gcount() != 3 * sizeof(float)) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }
    mesh_cold.bbox_max = MakeVec3(temp_f);

    const auto attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);

    mesh_cold.attribs.resize(attribs_size / sizeof(float));
    data.read((char *)mesh_cold.attribs.data(), attribs_size);
    if (data.gcount() != attribs_size) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }

    const auto index_data_size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    mesh_cold.indices.resize(index_data_size / sizeof(uint32_t));
    data.read((char *)mesh_cold.indices.data(), index_data_size);
    if (data.gcount() != index_data_size) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &mat_name = material_names.emplace_back();
        data.read(mat_name.data(), 64);
        if (data.gcount() != 64) {
            log->Error("Error reading mesh data (%s)!", name.c_str());
            return false;
        }
    }

    mesh_main.flags = {};

    const int tri_groups_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_groups_count == materials_count);
    for (int i = 0; i < tri_groups_count; i++) {
        int grp_data[3];
        data.read((char *)&grp_data[0], sizeof(grp_data));
        if (data.gcount() != sizeof(grp_data)) {
            log->Error("Error reading mesh data (%s)!", name.c_str());
            return false;
        }

        const int index_offset = grp_data[0];
        const int index_count = grp_data[1];
        const int alpha = grp_data[2];

        auto &grp = mesh_cold.groups.emplace_back();
        grp.byte_offset = int(index_offset * sizeof(uint32_t));
        grp.num_indices = index_count;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            mesh_main.flags |= eMeshFlags::HasAlpha;
        }

        std::array<MaterialHandle, 3> mats = on_mat_load(&material_names[i][0]);
        grp.front_mat = mats[0];
        grp.back_mat = mats[1];
        grp.vol_mat = mats[2];
    }

    return Mesh_InitBufferData(api, mesh_main, mesh_cold, vertex_buf1, vertex_buf2, index_buf, log);
}

bool Ren::Mesh_InitColored(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                           std::istream &data, const material_load_callback &on_mat_load, ResizableBuffer &vertex_buf1,
                           ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf, ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    if (data.gcount() != 12 || strncmp(mesh_type_str, "COLORE_MESH", 11) != 0) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }

    mesh_main.type = eMeshType::Colored;
    mesh_cold.name = name;

    struct Header {
        int num_chunks;
        mesh_chunk_pos_t p[5];
    } file_header = {};
    data.read((char *)&file_header, sizeof(file_header));
    if (data.gcount() != sizeof(file_header)) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    float temp_f[3];
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    if (data.gcount() != 3 * sizeof(float)) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }
    mesh_cold.bbox_min = MakeVec3(temp_f);
    data.read((char *)&temp_f[0], sizeof(float) * 3);
    if (data.gcount() != 3 * sizeof(float)) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }
    mesh_cold.bbox_max = MakeVec3(temp_f);

    const auto attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);

    mesh_cold.attribs.resize(attribs_size / sizeof(float));
    data.read((char *)mesh_cold.attribs.data(), attribs_size);
    if (data.gcount() != attribs_size) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }

    const auto index_data_size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    mesh_cold.indices.resize(index_data_size / sizeof(uint32_t));
    data.read((char *)mesh_cold.indices.data(), index_data_size);
    if (data.gcount() != index_data_size) {
        log->Error("Error reading mesh data (%s)!", name.c_str());
        return false;
    }

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &mat_name = material_names.emplace_back();
        data.read(mat_name.data(), 64);
    }

    mesh_main.flags = {};

    const int tri_strips_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_strips_count == materials_count);
    for (int i = 0; i < tri_strips_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        auto &grp = mesh_cold.groups.emplace_back();
        grp.byte_offset = int(index * sizeof(uint32_t));
        grp.num_indices = num_indices;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            mesh_main.flags |= eMeshFlags::HasAlpha;
        }

        std::array<MaterialHandle, 3> mats = on_mat_load(&material_names[i][0]);
        grp.front_mat = mats[0];
        grp.back_mat = mats[1];
        grp.vol_mat = mats[2];
    }

    assert(attribs_size % sizeof(orig_vertex_colored_t) == 0);
    const uint32_t vertex_count = attribs_size / sizeof(orig_vertex_colored_t);

    const uint32_t attribs_buf1_size = vertex_count * sizeof(packed_vertex_data1_t);
    const uint32_t attribs_buf2_size = vertex_count * sizeof(packed_vertex_data2_t);
    const uint32_t indices_buf_size = index_data_size;

    const uint32_t total_mem_required = attribs_buf1_size + attribs_buf2_size + indices_buf_size;

    Ren::BufferMain upload_buf_main = {};
    Ren::BufferCold upload_buf_cold = {};
    if (!Buffer_Init(api, upload_buf_main, upload_buf_cold, Ren::String{"Temp Upload Buf"}, eBufType::Upload,
                     total_mem_required, log)) {
        return false;
    }

    { // Update staging buffer
        auto *vertices_data1 =
            reinterpret_cast<packed_vertex_data1_t *>(Buffer_Map(api, upload_buf_main, upload_buf_cold));
        auto *vertices_data2 = reinterpret_cast<packed_vertex_data2_t *>(vertices_data1 + vertex_count);
        assert(uintptr_t(vertices_data2) % alignof(packed_vertex_data2_t) == 0);
        auto *index_data = reinterpret_cast<uint32_t *>(vertices_data2 + vertex_count);
        assert(uintptr_t(index_data) % alignof(uint32_t) == 0);

        const auto *orig_vertices = reinterpret_cast<const orig_vertex_colored_t *>(mesh_cold.attribs.data());

        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
            pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
        }
        memcpy(index_data, mesh_cold.indices.data(), indices_buf_size);

        Buffer_Unmap(api, upload_buf_main, upload_buf_cold);
    }

    { // Copy buffer data
        CommandBuffer cmd_buf = api.BegSingleTimeCommands();

        mesh_main.attribs_buf1.sub = vertex_buf1.AllocSubRegion(attribs_buf1_size, 16, mesh_cold.name, log,
                                                                &upload_buf_main, cmd_buf, 0 /* offset */);
        mesh_main.attribs_buf1.buf = vertex_buf1.handle();

        mesh_main.attribs_buf2.sub = vertex_buf2.AllocSubRegion(attribs_buf2_size, 16, mesh_cold.name, log,
                                                                &upload_buf_main, cmd_buf, attribs_buf1_size);
        mesh_main.attribs_buf2.buf = vertex_buf2.handle();

        mesh_main.indices_buf.sub = index_buf.AllocSubRegion(indices_buf_size, 4, mesh_cold.name, log, &upload_buf_main,
                                                             cmd_buf, attribs_buf1_size + attribs_buf2_size);
        mesh_main.indices_buf.buf = index_buf.handle();
        assert(mesh_main.attribs_buf1.sub.offset == mesh_main.attribs_buf2.sub.offset && "Offsets do not match!");

        api.EndSingleTimeCommands(cmd_buf);
    }

    Buffer_DestroyImmediately(api, upload_buf_main, upload_buf_cold);

    return true;
}

bool Ren::Mesh_InitSkeletal(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold, Ren::String name,
                            std::istream &data, const material_load_callback &on_mat_load,
                            ResizableBuffer &skin_vertex_buf, ResizableBuffer &delta_buf, ResizableBuffer &index_buf,
                            ILog *log) {
    char mesh_type_str[12];
    data.read(mesh_type_str, 12);
    if (strncmp(mesh_type_str, "SKELET_MESH", 11) != 0 && strncmp(mesh_type_str, "SKECOL_MESH", 11) != 0) {
        log->Error("Failed to read mesh header (%s)!", name.c_str());
        return false;
    }

    const bool vtx_color_present = strcmp(mesh_type_str, "SKECOL_MESH\0") == 0;

    mesh_main.type = eMeshType::Skeletal;
    mesh_cold.name = name;

    struct Header {
        int num_chunks;
        mesh_chunk_pos_t p[7];
    } file_header = {};
    data.read((char *)&file_header.num_chunks, sizeof(int));
    data.read((char *)&file_header.p[0], std::streamsize(file_header.num_chunks * sizeof(mesh_chunk_pos_t)));

    // Skip name, cant remember why i put it there
    data.seekg(32, std::ios::cur);

    { // Read bounding box
        float temp_f[3];
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        mesh_cold.bbox_min = MakeVec3(temp_f);
        data.read((char *)&temp_f[0], sizeof(float) * 3);
        mesh_cold.bbox_max = MakeVec3(temp_f);
    }

    const uint32_t sk_attribs_size = uint32_t(file_header.p[int(eMeshFileChunk::VtxAttributes)].length);
    mesh_cold.attribs.resize(sk_attribs_size / sizeof(float));
    data.read((char *)mesh_cold.attribs.data(), sk_attribs_size);

    const uint32_t indices_buf_size = uint32_t(file_header.p[int(eMeshFileChunk::TriIndices)].length);
    mesh_cold.indices.resize(indices_buf_size / sizeof(uint32_t));
    data.read((char *)mesh_cold.indices.data(), indices_buf_size);

    const int materials_count = file_header.p[int(eMeshFileChunk::Materials)].length / 64;
    SmallVector<std::array<char, 64>, 8> material_names;
    for (int i = 0; i < materials_count; i++) {
        auto &mat_name = material_names.emplace_back();
        data.read(mat_name.data(), 64);
    }

    mesh_main.flags = {};

    const int tri_groups_count = file_header.p[int(eMeshFileChunk::TriGroups)].length / 12;
    assert(tri_groups_count == materials_count);
    for (int i = 0; i < tri_groups_count; i++) {
        int index, num_indices, alpha;
        data.read((char *)&index, 4);
        data.read((char *)&num_indices, 4);
        data.read((char *)&alpha, 4);

        auto &grp = mesh_cold.groups.emplace_back();
        grp.byte_offset = int(index * sizeof(uint32_t));
        grp.num_indices = num_indices;

        if (alpha) {
            grp.flags |= eMeshFlags::HasAlpha;
            mesh_main.flags |= eMeshFlags::HasAlpha;
        }

        std::array<MaterialHandle, 3> mats = on_mat_load(&material_names[i][0]);
        grp.front_mat = mats[0];
        grp.back_mat = mats[1];
        grp.vol_mat = mats[2];
    }

    const int bones_count = file_header.p[int(eMeshFileChunk::Bones)].length / (64 + 8 + 12 + 16);
    mesh_cold.skel.bones.resize(bones_count);

    for (int i = 0; i < int(mesh_cold.skel.bones.size()); i++) {
        float temp_f[4];
        Vec3f temp_v;
        Quatf temp_q;
        char temp_name[64];
        data.read(temp_name, 64);
        mesh_cold.skel.bones[i].name = String{temp_name};
        data.read((char *)&mesh_cold.skel.bones[i].id, sizeof(int));
        data.read((char *)&mesh_cold.skel.bones[i].parent_id, sizeof(int));

        data.read((char *)&temp_f[0], sizeof(float) * 3);
        temp_v = MakeVec3(&temp_f[0]);
        mesh_cold.skel.bones[i].bind_matrix = Translate(mesh_cold.skel.bones[i].bind_matrix, temp_v);
        data.read((char *)&temp_f[0], sizeof(float) * 4);
        temp_q = MakeQuat(&temp_f[0]);
        mesh_cold.skel.bones[i].bind_matrix *= ToMat4(temp_q);
        mesh_cold.skel.bones[i].inv_bind_matrix = Inverse(mesh_cold.skel.bones[i].bind_matrix);

        if (mesh_cold.skel.bones[i].parent_id != -1) {
            mesh_cold.skel.bones[i].cur_matrix =
                mesh_cold.skel.bones[mesh_cold.skel.bones[i].parent_id].inv_bind_matrix *
                mesh_cold.skel.bones[i].bind_matrix;
            const Vec4f pos = mesh_cold.skel.bones[mesh_cold.skel.bones[i].parent_id].inv_bind_matrix *
                              mesh_cold.skel.bones[i].bind_matrix[3];
            mesh_cold.skel.bones[i].head_pos = MakeVec3(&pos[0]);
        } else {
            mesh_cold.skel.bones[i].cur_matrix = mesh_cold.skel.bones[i].bind_matrix;
            mesh_cold.skel.bones[i].head_pos = MakeVec3(&mesh_cold.skel.bones[i].bind_matrix[3][0]);
        }
        mesh_cold.skel.bones[i].cur_comb_matrix = mesh_cold.skel.bones[i].cur_matrix;
        mesh_cold.skel.bones[i].dirty = true;
    }

    const uint32_t vertex_count =
        sk_attribs_size / (vtx_color_present ? sizeof(orig_vertex_skinned_colored_t) : sizeof(orig_vertex_skinned_t));
    const uint32_t sk_attribs_buf_size = vertex_count * sizeof(packed_vertex_skinned_t);

    const uint32_t attribs_buf1_size = vertex_count * sizeof(packed_vertex_data1_t);
    const uint32_t attribs_buf2_size = vertex_count * sizeof(packed_vertex_data2_t);

    // TODO: This is incorrect!
    const uint32_t total_mem_required = attribs_buf1_size + attribs_buf2_size + indices_buf_size + sk_attribs_buf_size;

    Ren::BufferMain upload_buf_main = {};
    Ren::BufferCold upload_buf_cold = {};
    if (!Buffer_Init(api, upload_buf_main, upload_buf_cold, Ren::String{"Temp Upload Buf"}, eBufType::Upload,
                     total_mem_required, log)) {
        return false;
    }

    uint8_t *stage_buf_ptr = Buffer_Map(api, upload_buf_main, upload_buf_cold);
    uint32_t stage_buf_off = 0;

    uint32_t delta_buf_off = 0;
    uint32_t sk_deltas_buf_size = 0;

    const bool shape_data_present = (file_header.num_chunks > 6);
    if (shape_data_present) {
        uint32_t shape_keyed_vertices_start, shape_keyed_vertices_count;
        data.read((char *)&shape_keyed_vertices_start, sizeof(uint32_t));
        data.read((char *)&shape_keyed_vertices_count, sizeof(uint32_t));

        const int shapes_count =
            int(file_header.p[int(eMeshFileChunk::ShapeKeys)].length - 2 * sizeof(uint32_t)) /
            (64 + shape_keyed_vertices_count * (3 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float)));

        mesh_cold.deltas.resize(shapes_count * shape_keyed_vertices_count);
        mesh_cold.skel.shapes.resize(shapes_count);

        for (int i = 0; i < shapes_count; i++) {
            ShapeKey &sh_key = mesh_cold.skel.shapes[i];

            char temp_name[64];
            data.read(temp_name, 64);
            sh_key.name = String{temp_name};
            sh_key.delta_offset = shape_keyed_vertices_count * i;
            sh_key.delta_count = shape_keyed_vertices_count;
            sh_key.cur_weight_packed = 0;

            data.read((char *)&mesh_cold.deltas[sh_key.delta_offset],
                      std::streamsize(sh_key.delta_count * sizeof(vtx_delta_t)));
        }

        sk_deltas_buf_size = uint32_t(shapes_count * shape_keyed_vertices_count * sizeof(packed_vertex_delta_t));

        assert(upload_buf_cold.size - stage_buf_off >= sk_deltas_buf_size);
        auto *packed_deltas = reinterpret_cast<packed_vertex_delta_t *>(stage_buf_ptr + stage_buf_off);
        delta_buf_off = stage_buf_off;
        stage_buf_off += sk_deltas_buf_size;

        for (uint32_t i = 0; i < shapes_count * shape_keyed_vertices_count; i++) {
            pack_vertex_delta(mesh_cold.deltas[i], packed_deltas[i]);
        }
    }

    assert(upload_buf_cold.size - stage_buf_off >= sk_attribs_buf_size);
    auto *vertices = reinterpret_cast<packed_vertex_skinned_t *>(stage_buf_ptr + stage_buf_off);
    const uint32_t vertices_off = stage_buf_off;
    stage_buf_off += sk_attribs_buf_size;

    if (vtx_color_present) {
        const auto *orig_vertices = reinterpret_cast<const orig_vertex_skinned_colored_t *>(mesh_cold.attribs.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    } else {
        const auto *orig_vertices = reinterpret_cast<const orig_vertex_skinned_t *>(mesh_cold.attribs.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex(orig_vertices[i], vertices[i]);
        }
    }

    assert(upload_buf_cold.size - stage_buf_off >= indices_buf_size);
    auto *index_data = reinterpret_cast<uint32_t *>(stage_buf_ptr + stage_buf_off);
    const uint32_t indices_off = stage_buf_off;
    stage_buf_off += indices_buf_size;
    memcpy(index_data, mesh_cold.indices.data(), indices_buf_size);

    Buffer_Unmap(api, upload_buf_main, upload_buf_cold);

    { // Copy buffer data
        CommandBuffer cmd_buf = api.BegSingleTimeCommands();

        if (shape_data_present) {
            mesh_main.sk_deltas_buf.sub = delta_buf.AllocSubRegion(sk_deltas_buf_size, 16, mesh_cold.name, log,
                                                                   &upload_buf_main, cmd_buf, delta_buf_off);
            mesh_main.sk_deltas_buf.buf = delta_buf.handle();
        }

        // allocate untransformed vertices
        mesh_main.sk_attribs_buf.sub = skin_vertex_buf.AllocSubRegion(sk_attribs_buf_size, 16, mesh_cold.name, log,
                                                                      &upload_buf_main, cmd_buf, vertices_off);
        mesh_main.sk_attribs_buf.buf = skin_vertex_buf.handle();

        mesh_main.indices_buf.sub =
            index_buf.AllocSubRegion(indices_buf_size, 4, mesh_cold.name, log, &upload_buf_main, cmd_buf, indices_off);
        mesh_main.indices_buf.buf = index_buf.handle();

        api.EndSingleTimeCommands(cmd_buf);
    }

    Buffer_DestroyImmediately(api, upload_buf_main, upload_buf_cold);

    return true;
}

bool Ren::Mesh_InitBufferData(const ApiContext &api, MeshMain &mesh_main, MeshCold &mesh_cold,
                              ResizableBuffer &vertex_buf1, ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf,
                              ILog *log) {
    const uint32_t vertex_count = uint32_t(mesh_cold.attribs.size() * sizeof(float)) / sizeof(orig_vertex_t);

    const uint32_t attribs_buf1_size = vertex_count * sizeof(packed_vertex_data1_t);
    const uint32_t attribs_buf2_size = vertex_count * sizeof(packed_vertex_data2_t);
    const uint32_t indices_buf_size = uint32_t(mesh_cold.indices.size() * sizeof(uint32_t));

    const uint32_t total_mem_required = attribs_buf1_size + attribs_buf2_size + indices_buf_size;

    BufferMain upload_buf_main = {};
    BufferCold upload_buf_cold = {};
    if (!Buffer_Init(api, upload_buf_main, upload_buf_cold, String{"Temp Upload Buf"}, eBufType::Upload,
                     total_mem_required, log)) {
        return false;
    }

    { // Update staging buffer
        auto *vertices_data1 =
            reinterpret_cast<packed_vertex_data1_t *>(Buffer_Map(api, upload_buf_main, upload_buf_cold));
        auto *vertices_data2 = reinterpret_cast<packed_vertex_data2_t *>(vertices_data1 + vertex_count);
        assert(uintptr_t(vertices_data2) % alignof(packed_vertex_data2_t) == 0);
        auto *index_data = reinterpret_cast<uint32_t *>(vertices_data2 + vertex_count);
        assert(uintptr_t(index_data) % alignof(uint32_t) == 0);

        const auto *orig_vertices = reinterpret_cast<const orig_vertex_t *>(mesh_cold.attribs.data());
        for (uint32_t i = 0; i < vertex_count; i++) {
            pack_vertex_data1(orig_vertices[i], vertices_data1[i]);
            pack_vertex_data2(orig_vertices[i], vertices_data2[i]);
        }
        memcpy(index_data, mesh_cold.indices.data(), indices_buf_size);

        Buffer_Unmap(api, upload_buf_main, upload_buf_cold);
    }

    { // Copy buffer data
        CommandBuffer cmd_buf = api.BegSingleTimeCommands();

        mesh_main.attribs_buf1.sub = vertex_buf1.AllocSubRegion(attribs_buf1_size, 16, mesh_cold.name, log,
                                                                &upload_buf_main, cmd_buf, 0 /* offset */);
        mesh_main.attribs_buf1.buf = vertex_buf1.handle();

        mesh_main.attribs_buf2.sub = vertex_buf2.AllocSubRegion(attribs_buf2_size, 16, mesh_cold.name, log,
                                                                &upload_buf_main, cmd_buf, attribs_buf1_size);
        mesh_main.attribs_buf2.buf = vertex_buf2.handle();

        mesh_main.indices_buf.sub = index_buf.AllocSubRegion(indices_buf_size, 4, mesh_cold.name, log, &upload_buf_main,
                                                             cmd_buf, attribs_buf1_size + attribs_buf2_size);
        mesh_main.indices_buf.buf = index_buf.handle();
        assert(mesh_main.attribs_buf1.sub.offset == mesh_main.attribs_buf2.sub.offset && "Offsets do not match!");

        api.EndSingleTimeCommands(cmd_buf);
    }

    Buffer_DestroyImmediately(api, upload_buf_main, upload_buf_cold);

    return true;
}

void Ren::Mesh_Destroy(SparseDualStorage<BufferMain, BufferCold> &buffers, MeshMain &mesh_main, MeshCold &mesh_cold) {
    auto free_range = [&](BufferRange &range) {
        if (range.buf && range.sub) {
            Buffer_FreeSubRegion(buffers[range.buf].second, range.sub);
            range = {};
        }
    };
    free_range(mesh_main.attribs_buf1);
    free_range(mesh_main.attribs_buf2);
    free_range(mesh_main.indices_buf);
    free_range(mesh_main.sk_attribs_buf);
    free_range(mesh_main.sk_deltas_buf);

    mesh_main = {};
    mesh_cold = {};
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif