#pragma once

#include <cstddef>
#include <cstdint>

#include <Ren/MMat.h>
#include <Ren/SmallVector.h>

#if defined(USE_VK_RENDER)
#include <Ren/Buffer.h>
#include <Ren/Fwd.h>
#include <Ren/VK.h>
#elif defined(USE_GL_RENDER)
#include <Ren/Buffer.h>
#endif

namespace Ren {
class IAccStructure;
}

namespace Eng {
#include "shaders/Constants.inl"

struct LightItem {
    float col[3];
    int type;
    float pos[3], radius;
    float dir[3], spot;
    float u[3];
    int shadowreg_index;
    float v[3], _unused1;
};
static_assert(sizeof(LightItem) == 80, "!");

struct DecalItem {
    float mat[3][4];
    // TODO: use 16-bit unorms
    float mask[4], diff[4], norm[4], spec[4];
};
static_assert(sizeof(DecalItem) == DECALS_BUF_STRIDE * 4 * sizeof(float), "!");

struct ProbeItem {
    float position[3], radius;
    float _unused[3], layer;
    float sh_coeffs[3][4];
};
static_assert(sizeof(ProbeItem) == 20 * sizeof(float), "!");

struct EllipsItem {
    float position[3], radius;
    float axis[3];
    uint32_t perp;
};
static_assert(sizeof(EllipsItem) == 8 * sizeof(float), "!");

struct CellData {
    uint32_t item_offset : 24;
    uint32_t light_count : 8;
    uint32_t decal_count : 8;
    uint32_t probe_count : 8;
    uint32_t ellips_count : 8;
    uint32_t _unused : 8;
};
static_assert(sizeof(CellData) == 8, "!");

struct ItemData {
    uint32_t light_index : 12;
    uint32_t decal_index : 12;
    uint32_t probe_index : 8;
    uint32_t ellips_index : 8;
    uint32_t _unused : 24;
};
static_assert(sizeof(ItemData) == 8, "!");

struct InstanceData {
    float model_matrix[3][4];
    union {
        float lmap_transform[4];
        struct {
            // float 0
            uint8_t movement_scale;
            uint8_t tree_mode;
            uint8_t bend_scale;
            uint8_t stretch;
            // float 1, 2
            uint16_t wind_dir_ls[3];
            uint16_t wind_turb;
        };
    };
    float normal_matrix[3][4];
    float _pad0[4];
    float prev_model_matrix[3][4];
    float _pad1[4];
};
static_assert(sizeof(InstanceData) == 192, "!");

struct BasicDrawBatch {                        // NOLINT
    static const uint32_t TypeSimple = 0b00u;  // simple
    static const uint32_t TypeVege = 0b01u;    // vegetation
    static const uint32_t TypeSkinned = 0b10u; // skeletal
    static const uint32_t TypeUnused = 0b11u;

    static const uint32_t BitsSimple = (TypeSimple << 30u);
    static const uint32_t BitsVege = (TypeVege << 30u);
    static const uint32_t BitsSkinned = (TypeSkinned << 30u);
    static const uint32_t BitsUnused = (TypeUnused << 30u);
    static const uint32_t BitAlphaTest = (1u << 29u);
    static const uint32_t BitMoving = (1u << 28u);
    static const uint32_t BitTwoSided = (1u << 27u);
    static const uint32_t BitCustomShaded = (1u << 26u);
    static const uint32_t FlagBits = (0b111111u << 26u);

    union {
        struct {
            uint32_t indices_offset : 26;
            uint32_t custom_shaded : 1;
            uint32_t two_sided_bit : 1;
            uint32_t moving_bit : 1; // object uses two transforms
            uint32_t alpha_test_bit : 1;
            uint32_t type_bits : 2;
        };
        uint32_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int32_t instance_index, material_index;
    uint32_t instance_start;
    uint32_t instance_count;
};
static_assert(offsetof(BasicDrawBatch, indices_count) == 4, "!");

enum class eFwdPipeline { FrontfaceDraw, BackfaceDraw, Wireframe, _Count };

// Draw batch that allows to specify program for forward rendering
struct CustomDrawBatch { // NOLINT
    // TODO: change order of alpha-test and two-sided
    static const uint64_t BitAlphaBlend = (1ull << 63u);
    static const uint64_t BitAlphaTest = (1ull << 62u);
    static const uint64_t BitDepthWrite = (1ull << 61u);
    static const uint64_t BitTwoSided = (1ull << 60u);
    static const uint64_t BitsProgId = (0b11111111ull << 54u);
    static const uint64_t BitsMatId = (0b11111111111111ull << 40u);
    static const uint64_t BitsCamDist = (0b11111111ull << 32u);
    static const uint64_t FlagBits = (0b1111ull << 60u);

    union {
        struct {
            uint64_t _pad1 : 3;
            uint64_t indices_offset : 27;
            uint64_t cam_dist : 8;
            uint64_t mat_id : 14;
            uint64_t pipe_id : 8;
            uint64_t two_sided_bit : 1;
            uint64_t depth_write_bit : 1;
            uint64_t alpha_test_bit : 1;
            uint64_t alpha_blend_bit : 1;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int32_t instance_index, material_index;
    uint32_t instance_start;
    uint32_t instance_count;
};
static_assert(offsetof(CustomDrawBatch, indices_count) == 8, "!");

struct ShadowList { // NOLINT
    int shadow_map_pos[2], shadow_map_size[2];
    int scissor_test_pos[2], scissor_test_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;
    float bias[2];

    // for debugging
    float cam_near, cam_far;
    Ren::Vec2f view_frustum_outline[12];
    int view_frustum_outline_count;
};

struct ShadowMapRegion {
    Ren::Vec4f transform;
    Ren::Mat4f clip_from_world;
};
static_assert(sizeof(ShadowMapRegion) == 80, "!");

struct SortSpan32 {
    uint32_t key;
    uint32_t base;
    uint32_t count;
};
static_assert(sizeof(SortSpan32) == 12, "!");

struct SortSpan64 {
    uint64_t key;
    uint32_t base;
    uint32_t count;
};
static_assert(sizeof(SortSpan64) == 16, "!");

struct SkinTransform {
    float matr[3][4];
};
static_assert(sizeof(SkinTransform) == 48, "!");

struct SkinRegion {
    uint32_t in_vtx_offset, out_vtx_offset;
    uint32_t xform_offset, delta_offset;
    uint32_t shape_key_offset_curr, shape_key_count_curr;
    uint32_t shape_key_offset_prev, shape_key_count_prev;
    uint32_t vertex_count, shape_keyed_vertex_count;
};

struct ShapeKeyData {
    uint16_t shape_index;
    uint16_t shape_weight;
};

enum class eRenderMode : uint8_t { Forward, Deferred };

enum class eReflectionsQuality : uint8_t { Off, Low, High, Raytraced };

enum class eShadowsQuality : uint8_t { Off, High, Raytraced };

enum class eTonemapMode : uint8_t { Off, Standard, LUT };

enum class eTAAMode : uint8_t { Off, Dynamic, Static };

enum class eHDRQuality : uint8_t { Medium, High };

enum class eGIQuality : uint8_t { Off, High };

enum class eDebugRT : uint8_t { Off, Main, Shadow };

enum class eDebugDenoise : uint8_t { Off, Reflection, GI, Shadow };

struct render_settings_t {
    union {
        struct {
            bool enable_zfill : 1;
            bool enable_culling : 1;
            bool enable_ssao : 1;
            bool enable_lightmap : 1;
            bool enable_lights : 1;
            bool enable_decals : 1;
            bool enable_probes : 1;
            bool enable_dof : 1;
            bool enable_bloom : 1;
            bool enable_timers : 1;
        };
        uint32_t flags = 0xffffffff;
    };

    union {
        struct {
            bool debug_culling : 1;
            bool debug_wireframe : 1;
            bool debug_ssao : 1;
            bool debug_shadows : 1;
            bool debug_lights : 1;
            bool debug_decals : 1;
            bool debug_deferred : 1;
            bool debug_blur : 1;
            bool debug_timings : 1;
            bool debug_bvh : 1;
            bool debug_probes : 1;
            bool debug_ellipsoids : 1;
            bool debug_freeze : 1;
            bool debug_motion : 1;
        };
        uint32_t debug_flags = 0;
    };

    eRenderMode render_mode = eRenderMode::Deferred;
    eReflectionsQuality reflections_quality = eReflectionsQuality::Raytraced;
    eShadowsQuality shadows_quality = eShadowsQuality::High;
    eTonemapMode tonemap_mode = eTonemapMode::Standard;
    eTAAMode taa_mode = eTAAMode::Dynamic;
    eHDRQuality hdr_quality = eHDRQuality::High;
    eGIQuality gi_quality = eGIQuality::High;

    eDebugRT debug_rt = eDebugRT::Off;
    eDebugDenoise debug_denoise = eDebugDenoise::Off;

    bool operator==(const render_settings_t &rhs) {
        return flags == rhs.flags && debug_flags == rhs.debug_flags && reflections_quality == rhs.reflections_quality &&
               shadows_quality == rhs.shadows_quality && tonemap_mode == rhs.tonemap_mode && taa_mode == rhs.taa_mode &&
               hdr_quality == rhs.hdr_quality && gi_quality == rhs.gi_quality && debug_rt == rhs.debug_rt &&
               debug_denoise == rhs.debug_denoise;
    }
    bool operator!=(const render_settings_t &rhs) { return !operator==(rhs); }

    render_settings_t &operator&=(const render_settings_t &rhs) {
        flags &= rhs.flags;
        debug_flags &= rhs.debug_flags;

        render_mode = eRenderMode(std::min(uint8_t(render_mode), uint8_t(rhs.render_mode)));
        reflections_quality =
            eReflectionsQuality(std::min(uint8_t(reflections_quality), uint8_t(rhs.reflections_quality)));
        shadows_quality = eShadowsQuality(std::min(uint8_t(shadows_quality), uint8_t(rhs.shadows_quality)));
        tonemap_mode = eTonemapMode(std::min(uint8_t(tonemap_mode), uint8_t(rhs.tonemap_mode)));
        taa_mode = eTAAMode(std::min(uint8_t(taa_mode), uint8_t(rhs.taa_mode)));
        hdr_quality = eHDRQuality(std::min(uint8_t(hdr_quality), uint8_t(rhs.hdr_quality)));
        gi_quality = eGIQuality(std::min(uint8_t(gi_quality), uint8_t(rhs.gi_quality)));

        return (*this);
    }
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0, end_timepoint_us = 0;
    uint32_t occluders_time_us = 0, main_gather_time_us = 0, shadow_gather_time_us = 0, drawables_sort_time_us = 0,
             items_assignment_time_us = 0;
};

struct PassTiming {
    char name[64];
    uint64_t duration;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0, cpu_end_timepoint_us = 0;
    uint64_t gpu_total_duration = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    Ren::SmallVector<PassTiming, 256> pass_timings;

    uint32_t shadow_draw_calls_count = 0, depth_fill_draw_calls_count = 0, opaque_draw_calls_count = 0;

    uint32_t tris_rendered = 0;
};

struct ItemsInfo {
    uint32_t lights_count = 0, decals_count = 0, probes_count = 0;
    uint32_t items_total = 0;
};

struct ViewState {
    Ren::Vec2i act_res, scr_res;
    float vertical_fov;
    int frame_index;
    Ren::Vec3f prev_cam_pos;
    Ren::Mat4f prev_clip_from_world_no_translation, down_buf_view_from_world, prev_clip_from_view;
    mutable Ren::Vec4f clip_info, frustum_info, rand_rotators[3];
    bool is_multisampled = false;
};

struct SharedDataBlock {
    Ren::Mat4f view_from_world, clip_from_view, clip_from_world_no_translation, prev_clip_from_world_no_translation;
    Ren::Mat4f world_from_view, view_from_clip, world_from_clip_no_translation, delta_matrix;
    Ren::Mat4f rt_clip_from_world;
    ShadowMapRegion shadowmap_regions[MAX_SHADOWMAPS_TOTAL];
    Ren::Vec4f sun_dir, sun_col, taa_info, frustum_info;
    Ren::Vec4f clip_info, rt_clip_info, cam_pos_and_gamma, prev_cam_pos;
    Ren::Vec4f res_and_fres, transp_params_and_time;
    Ren::Vec4f wind_scroll, wind_scroll_prev;
    Ren::Vec4u item_counts;
    Ren::Vec4f ambient_hack;
    ProbeItem probes[MAX_PROBES_TOTAL] = {};
    EllipsItem ellipsoids[MAX_ELLIPSES_TOTAL] = {};
};
static_assert(sizeof(SharedDataBlock) == 7888 + 2560 + 64 + 16, "!");

const int MAX_MATERIAL_PARAMS = 3;

struct MaterialData {
    uint32_t texture_indices[MAX_TEX_PER_MATERIAL];
    uint32_t _pad[2];
    Ren::Vec4f params[MAX_MATERIAL_PARAMS];
};
static_assert(sizeof(MaterialData) == 80, "!");

const uint32_t RTGeoProbeBits = 0xff;
const uint32_t RTGeoLightmappedBit = (1u << 8u);

struct RTGeoInstance {
    uint32_t indices_start;
    uint32_t vertices_start;
    uint32_t material_index;
    uint32_t flags;
    float lmap_transform[4];
};
static_assert(sizeof(RTGeoInstance) == 32, "!");

struct RTObjInstance {
    float xform[3][4];
    float bbox_min_ws[3];
    uint32_t geo_index : 24;
    uint32_t mask : 8;
    float bbox_max_ws[3];
    uint32_t geo_count;
    const Ren::IAccStructure *blas_ref;
};
static_assert(sizeof(RTObjInstance) == 64 + 24, "!");

struct BindlessTextureData {
#if defined(USE_VK_RENDER)
    const Ren::SmallVectorImpl<VkDescriptorSet> *textures_descr_sets;
    VkDescriptorSet rt_textures_descr_set, rt_inline_textures_descr_set;
#elif defined(USE_GL_RENDER)
    Ren::WeakBufferRef textures_buf;
#endif
};

enum class eTLASIndex { Main, Shadow, _Count };

struct AccelerationStructureData {
    Ren::WeakBufferRef rt_instance_buf, rt_geo_data_buf, rt_tlas_buf, rt_sh_tlas_buf;
    struct {
        uint32_t rt_tlas_build_scratch_size = 0;
    } hwrt;

    Ren::IAccStructure *rt_tlases[int(eTLASIndex::_Count)] = {};
};

//
// SWRT stuff
//

const uint32_t LEAF_NODE_BIT = (1u << 31);
const uint32_t PRIM_INDEX_BITS = ~LEAF_NODE_BIT;
const uint32_t LEFT_CHILD_BITS = ~LEAF_NODE_BIT;

const uint32_t SEP_AXIS_BITS = (0b11u << 30);
const uint32_t PRIM_COUNT_BITS = ~SEP_AXIS_BITS;
const uint32_t RIGHT_CHILD_BITS = ~SEP_AXIS_BITS;

struct gpu_bvh_node_t { // NOLINT
    Ren::Vec3f bbox_min;
    union {
        uint32_t prim_index; // First bit is used to identify leaf node
        uint32_t left_child;
    };
    Ren::Vec3f bbox_max;
    union {
        uint32_t prim_count; // First two bits are used for separation axis (0, 1 or 2 - x, y or z)
        uint32_t right_child;
    };
};
static_assert(sizeof(gpu_bvh_node_t) == 32, "!");

struct gpu_mesh_t {
    uint32_t node_index, node_count;
    uint32_t tris_index, tris_count;
    uint32_t vert_index, geo_count;
};
static_assert(sizeof(gpu_mesh_t) == 24, "!");
static_assert(sizeof(gpu_mesh_t) == MESH_BUF_STRIDE * 2 * sizeof(float), "!");

struct gpu_mesh_instance_t {
    Ren::Vec3f bbox_min;
    uint32_t geo_index;
    Ren::Vec3f bbox_max;
    uint32_t mesh_index;
    // TODO: this can be optimized
    Ren::Mat3x4f inv_transform;
    Ren::Mat3x4f transform;
};
static_assert(sizeof(gpu_mesh_instance_t) == 32 + 48 + 48, "!");
static_assert(sizeof(gpu_mesh_instance_t) == MESH_INSTANCE_BUF_STRIDE * 4 * sizeof(float), "!");

const size_t sizeof_VkAccelerationStructureInstanceKHR = 64;

// Constant that controls buffers orphaning
const size_t SkinTransformsBufChunkSize = sizeof(SkinTransform) * MAX_SKIN_XFORMS_TOTAL;
const size_t ShapeKeysBufChunkSize = sizeof(ShapeKeyData) * MAX_SHAPE_KEYS_TOTAL;
const size_t SkinRegionsBufChunkSize = sizeof(SkinRegion) * MAX_SKIN_REGIONS_TOTAL;
const size_t InstanceDataBufChunkSize = sizeof(InstanceData) * MAX_INSTANCES_TOTAL;
const size_t InstanceIndicesBufChunkSize = sizeof(Ren::Vec2i) * MAX_INSTANCES_TOTAL;
const size_t LightsBufChunkSize = sizeof(LightItem) * MAX_LIGHTS_TOTAL;
const size_t DecalsBufChunkSize = sizeof(DecalItem) * MAX_DECALS_TOTAL;
const size_t CellsBufChunkSize = sizeof(CellData) * ITEM_CELLS_COUNT;
const size_t ItemsBufChunkSize = sizeof(ItemData) * MAX_ITEMS_TOTAL;
const size_t HWRTObjInstancesBufChunkSize = sizeof_VkAccelerationStructureInstanceKHR * MAX_RT_OBJ_INSTANCES;
const size_t SWRTObjInstancesBufChunkSize = sizeof(gpu_mesh_instance_t) * MAX_RT_OBJ_INSTANCES;
const size_t SWRTTLASNodesBufChunkSize = sizeof(gpu_bvh_node_t) * MAX_RT_TLAS_NODES;
const size_t SharedDataBlockSize = 12 * 1024;

static_assert(sizeof(SharedDataBlock) <= SharedDataBlockSize, "!");

} // namespace Eng