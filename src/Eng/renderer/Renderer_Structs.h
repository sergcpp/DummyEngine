#pragma once

#include <cstddef>
#include <cstdint>

#include <Ren/MMat.h>
#include <Ren/MQuat.h>
#include <Ren/SmallVector.h>

#include <Ren/Buffer.h>
#if defined(USE_VK_RENDER)
#include <Ren/VK.h>
#endif

namespace Ren {
class IAccStructure;
}

#include "shaders/Types.h"

namespace Eng {
#include "shaders/Constants.inl"

using namespace Types;
static_assert(sizeof(Types::LightItem) == 96, "!");

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
    uint32_t vis_mask;
    float _pad0[3];
    float prev_model_matrix[3][4];
    float _pad1[4];
};
static_assert(sizeof(InstanceData) == 192, "!");

struct BasicDrawBatch {                          // NOLINT
    static const uint64_t TypeSimple = 0b00ull;  // simple
    static const uint64_t TypeVege = 0b01ull;    // vegetation
    static const uint64_t TypeSkinned = 0b10ull; // skeletal
    static const uint64_t TypeUnused = 0b11ull;

    static const uint64_t BitsSimple = (TypeSimple << 31u);
    static const uint64_t BitsVege = (TypeVege << 31u);
    static const uint64_t BitsSkinned = (TypeSkinned << 31u);
    static const uint64_t BitAlphaTest = (1ull << 30u);
    static const uint64_t BitMoving = (1ull << 29u);
    static const uint64_t BitTwoSided = (1ull << 28u);
    static const uint64_t BitBackSided = (1ull << 27u);
    static const uint64_t BitAlphaBlend = (1ull << 33u);
    static const uint64_t BitEmissive = (1ull << 34u);
    static const uint64_t FlagBits = (0b11111111ull << 27u);

    union {
        struct {
            uint64_t indices_offset : 27;
            uint64_t back_sided_bit : 1;
            uint64_t two_sided_bit : 1;
            uint64_t moving_bit : 1; // object uses two transforms
            uint64_t alpha_test_bit : 1;
            uint64_t type_bits : 2;
            uint64_t alpha_blend_bit : 1;
            uint64_t emissive_bit : 1;
            uint64_t _unused : 27;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int32_t instance_index, material_index;
    uint32_t instance_start;
    uint32_t instance_count;
};
static_assert(offsetof(BasicDrawBatch, indices_count) == 8, "!");

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

enum class ePixelFilter : uint8_t { Box, Gaussian, BlackmanHarris };

enum class eReflectionsQuality : uint8_t { Off, High, Raytraced_Normal, Raytraced_High };

enum class eShadowsQuality : uint8_t { Off, High, Raytraced };

enum class eTonemapMode : uint8_t { Off, Standard, LUT };

enum class eTAAMode : uint8_t { Off, Dynamic, Static };

enum class eGIQuality : uint8_t { Off, Medium, High, Ultra };

enum class eGICacheUpdateMode : uint8_t { Off, Partial, Full };

enum class eSkyQuality : uint8_t { Medium, High, Ultra };

enum class eDebugRT : uint8_t { Off, Main, Shadow };

enum class eDebugDenoise : uint8_t { Off, Reflection, GI, Shadow };

enum class eDebugFrame : uint8_t { Off, Simple, Full };

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
            bool enable_motion_blur : 1;
            bool enable_dof : 1;
            bool enable_bloom : 1;
            bool enable_timers : 1;
            bool enable_shadow_jitter : 1;
            bool enable_aberration : 1;
            bool enable_purkinje : 1;
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
            bool debug_bvh : 1;
            bool debug_ellipsoids : 1;
            bool debug_freeze : 1;
            bool debug_motion : 1;
        };
        uint32_t debug_flags = 0;
    };

    eRenderMode render_mode = eRenderMode::Deferred;
    ePixelFilter pixel_filter = ePixelFilter::BlackmanHarris;
    float pixel_filter_width = 1.5f;
    eReflectionsQuality reflections_quality = eReflectionsQuality::Raytraced_Normal;
    eShadowsQuality shadows_quality = eShadowsQuality::High;
    eTonemapMode tonemap_mode = eTonemapMode::Standard;
    eTAAMode taa_mode = eTAAMode::Dynamic;
    eGIQuality gi_quality = eGIQuality::High;
    eGICacheUpdateMode gi_cache_update_mode = eGICacheUpdateMode::Partial;
    eSkyQuality sky_quality = eSkyQuality::High;

    eDebugRT debug_rt = eDebugRT::Off;
    eDebugDenoise debug_denoise = eDebugDenoise::Off;
#if !defined(NDEBUG)
    eDebugFrame debug_frame = eDebugFrame::Simple;
#else
    eDebugFrame debug_frame = eDebugFrame::Off;
#endif
    int8_t debug_probes = -1;
    int8_t debug_oit_layer = -1;

    render_settings_t() { enable_shadow_jitter = false; }

    bool operator==(const render_settings_t &rhs) const {
        return flags == rhs.flags && debug_flags == rhs.debug_flags && reflections_quality == rhs.reflections_quality &&
               shadows_quality == rhs.shadows_quality && tonemap_mode == rhs.tonemap_mode && taa_mode == rhs.taa_mode &&
               gi_quality == rhs.gi_quality && gi_cache_update_mode == rhs.gi_cache_update_mode &&
               sky_quality == rhs.sky_quality && debug_rt == rhs.debug_rt && debug_denoise == rhs.debug_denoise &&
               debug_probes == rhs.debug_probes && debug_oit_layer == rhs.debug_oit_layer;
    }
    bool operator!=(const render_settings_t &rhs) const { return !operator==(rhs); }

    render_settings_t &operator&=(const render_settings_t &rhs) {
        flags &= rhs.flags;
        debug_flags &= rhs.debug_flags;

        render_mode = eRenderMode(std::min(uint8_t(render_mode), uint8_t(rhs.render_mode)));
        reflections_quality =
            eReflectionsQuality(std::min(uint8_t(reflections_quality), uint8_t(rhs.reflections_quality)));
        shadows_quality = eShadowsQuality(std::min(uint8_t(shadows_quality), uint8_t(rhs.shadows_quality)));
        tonemap_mode = eTonemapMode(std::min(uint8_t(tonemap_mode), uint8_t(rhs.tonemap_mode)));
        taa_mode = eTAAMode(std::min(uint8_t(taa_mode), uint8_t(rhs.taa_mode)));
        gi_quality = eGIQuality(std::min(uint8_t(gi_quality), uint8_t(rhs.gi_quality)));
        gi_cache_update_mode =
            eGICacheUpdateMode(std::min(uint8_t(gi_cache_update_mode), uint8_t(rhs.gi_cache_update_mode)));

        return (*this);
    }
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0, end_timepoint_us = 0;
    uint32_t occluders_time_us = 0, main_gather_time_us = 0, shadow_gather_time_us = 0, drawables_sort_time_us = 0,
             items_assignment_time_us = 0;
};

struct pass_info_t {
    std::string name;
    uint64_t duration_us;
    Ren::SmallVector<std::string, 16> input;
    Ren::SmallVector<std::string, 16> output;
};

struct resource_info_t {
    std::string name;
    uint16_t lifetime[2][2];
    uint32_t heap, offset, size;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0, cpu_end_timepoint_us = 0;
    uint64_t gpu_total_duration = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    Ren::SmallVector<pass_info_t, 256> passes_info;
    Ren::SmallVector<resource_info_t, 256> resources_info;

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
    float pixel_spread_angle;
    int frame_index, volume_to_update, stochastic_lights_count, stochastic_lights_count_cache;
    Ren::Vec3f prev_cam_pos, prev_sun_dir;
    Ren::Mat4f clip_from_world, view_from_world, prev_view_from_world, prev_clip_from_world, down_buf_view_from_world,
        prev_clip_from_view;
    mutable Ren::Vec4f clip_info, frustum_info, rand_rotators[2];
    Ren::Quatf probe_ray_rotator;
    uint32_t probe_ray_hash = 0;
    uint32_t env_generation = 0xffffffff;
};

struct SharedDataBlock {
    Ren::Mat4f view_from_world, clip_from_view, clip_from_world, prev_view_from_world, prev_clip_from_world;
    Ren::Mat4f world_from_view, view_from_clip, world_from_clip, delta_matrix;
    Ren::Mat4f rt_clip_from_world;
    ShadowMapRegion shadowmap_regions[MAX_SHADOWMAPS_TOTAL];
    Ren::Vec4f sun_dir, sun_col, sun_col_point, sun_col_point_sh, env_col, taa_info, frustum_info;
    Ren::Vec4f clip_info, rt_clip_info, cam_pos_and_exp, prev_cam_pos;
    Ren::Vec4f res_and_fres, transp_params_and_time;
    Ren::Vec4i ires_and_ifres;
    Ren::Vec4f wind_scroll, wind_scroll_prev;
    Ren::Vec4u item_counts;
    Ren::Vec4f ambient_hack;
    Types::ProbeVolume probe_volumes[PROBE_VOLUMES_COUNT];
    uint32_t portals[MAX_PORTALS_TOTAL] = {0xffffffff};
    ProbeItem probes[MAX_PROBES_TOTAL] = {};
    EllipsItem ellipsoids[MAX_ELLIPSES_TOTAL] = {};
    Types::AtmosphereParams atmosphere;
};
static_assert(sizeof(SharedDataBlock) ==
                  7888 + 2560 + 64 + 16 + 16 + 4 * 64 + 64 + 192 + 16 + 16 + 16 + 16 + PROBE_VOLUMES_COUNT * 64,
              "!");

const int MAX_MATERIAL_PARAMS = 4;

struct MaterialData {
    uint32_t texture_indices[MAX_TEX_PER_MATERIAL];
    uint32_t _pad[2];
    Ren::Vec4f params[MAX_MATERIAL_PARAMS];
};
static_assert(sizeof(MaterialData) == 96, "!");

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
    Ren::Span<const VkDescriptorSet> textures_descr_sets;
    VkDescriptorSet rt_textures_descr_set, rt_inline_textures_descr_set;
#elif defined(USE_GL_RENDER)
    Ren::WeakBufferRef textures_buf;
#endif
};

enum class eTLASIndex { Main, Shadow, _Count };

struct AccelerationStructureData {
    Ren::WeakBufferRef rt_tlas_buf, rt_sh_tlas_buf;
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

const uint32_t BVH2_PRIM_COUNT_BITS = (7u << 29); // 0b111u
const uint32_t BVH2_PRIM_INDEX_BITS = ~BVH2_PRIM_COUNT_BITS;

struct gpu_bvh_node_t { // NOLINT
    Ren::Vec3f bbox_min;
    union {
        uint32_t prim_index; // First bit is used to identify leaf node
        uint32_t left_child;
    };
    Ren::Vec3f bbox_max;
    union {
        uint32_t prim_count; // First two bits are used for separation axis (00 - x, 01 - y, 11 - z)
        uint32_t right_child;
    };
};
static_assert(sizeof(gpu_bvh_node_t) == 32, "!");

struct gpu_bvh2_node_t {
    Ren::Vec4f ch_data0;  // [ ch0.min.x, ch0.max.x, ch0.min.y, ch0.max.y ]
    Ren::Vec4f ch_data1;  // [ ch1.min.x, ch1.max.x, ch1.min.y, ch1.max.y ]
    Ren::Vec4f ch_data2;  // [ ch0.min.z, ch0.max.z, ch1.min.z, ch1.max.z ]
    uint32_t left_child;  // First three bits identify primitive count in leaf nodes
    uint32_t right_child; // First three bits identify primitive count in leaf nodes
    uint32_t _unused0, _unused1;
};
static_assert(sizeof(gpu_bvh2_node_t) == 64, "!");
static_assert(sizeof(gpu_bvh2_node_t) == BVH2_NODE_BUF_STRIDE * 4 * sizeof(float), "!");

struct alignas(32) gpu_wbvh_node_t {
    float bbox_min[3][8];
    float bbox_max[3][8];
    uint32_t child[8];
};
static_assert(sizeof(gpu_wbvh_node_t) == 224, "!");

struct gpu_light_bvh_node_t : public gpu_bvh_node_t {
    float flux;
    Ren::Vec3f axis;
    float omega_n; // cone angle enclosing light normals
    float omega_e; // emission angle around each normal
};
static_assert(sizeof(gpu_light_bvh_node_t) == 56, "!");

struct gpu_light_wbvh_node_t : public gpu_wbvh_node_t {
    float flux[8];
    uint32_t axis[8];
    uint32_t cos_omega_ne[8];
};
static_assert(sizeof(gpu_light_wbvh_node_t) == 320, "!");

struct gpu_cwbvh_node_t {
    Ren::Vec3f bbox_min;
    float _unused0;
    Ren::Vec3f bbox_max;
    float _unused1;
    uint8_t ch_bbox_min[3][8];
    uint8_t ch_bbox_max[3][8];
    uint32_t child[8];
};
static_assert(sizeof(gpu_cwbvh_node_t) == 112, "!");

struct gpu_light_cwbvh_node_t : public gpu_cwbvh_node_t {
    float flux[8];
    uint32_t axis[8];
    uint32_t cos_omega_ne[8];
};
static_assert(sizeof(gpu_light_cwbvh_node_t) == 208, "!");

struct gpu_mesh_instance_t {
    uint32_t visibility;
    uint32_t node_index;
    uint32_t vert_index;
    uint32_t geo_index_count;
    // TODO: this can be optimized
    Ren::Mat3x4f inv_transform;
    Ren::Mat3x4f transform;
};
static_assert(sizeof(gpu_mesh_instance_t) == 112, "!");
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
const size_t RTGeoInstancesBufChunkSize = sizeof(RTGeoInstance) * MAX_RT_GEO_INSTANCES;
const size_t HWRTObjInstancesBufChunkSize = sizeof_VkAccelerationStructureInstanceKHR * MAX_RT_OBJ_INSTANCES;
const size_t SWRTObjInstancesBufChunkSize = sizeof(gpu_mesh_instance_t) * MAX_RT_OBJ_INSTANCES;
const size_t SWRTTLASNodesBufChunkSize = sizeof(gpu_bvh2_node_t) * MAX_RT_TLAS_NODES;
const size_t SharedDataBlockSize = 12 * 1024;

static_assert(sizeof(SharedDataBlock) <= SharedDataBlockSize, "!");

} // namespace Eng