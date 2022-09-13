#pragma once

#include <cstdint>

#include <atomic>

#include <Ren/HashMap32.h>
#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Pipeline.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Storage.h>
#include <Ren/TextureAtlas.h>

#include "Comp/AccStructure.h"
#include "Comp/AnimState.h"
#include "Comp/Decal.h"
#include "Comp/Drawable.h"
#include "Comp/Lightmap.h"
#include "Comp/LightProbe.h"
#include "Comp/LightSource.h"
#include "Comp/Occluder.h"
#include "Comp/Physics.h"
#include "Comp/SoundSource.h"
#include "Comp/Transform.h"
#include "Comp/VegState.h"

enum eObjectComp : uint32_t {
    CompTransform    = 0,
    CompDrawable     = 1,
    CompOccluder     = 2,
    CompLightmap     = 3,
    CompLightSource  = 4,
    CompDecal        = 5,
    CompProbe        = 6,
    CompAnimState    = 7,
    CompVegState     = 8,
    CompSoundSource  = 9,
    CompPhysics      = 10,
    CompAccStructure = 11,
};

enum eObjectCompBit : uint32_t {
    CompTransformBit    = (1u << CompTransform),
    CompDrawableBit     = (1u << CompDrawable),
    CompOccluderBit     = (1u << CompOccluder),
    CompLightmapBit     = (1u << CompLightmap),
    CompLightSourceBit  = (1u << CompLightSource),
    CompDecalBit        = (1u << CompDecal),
    CompProbeBit        = (1u << CompProbe),
    CompAnimStateBit    = (1u << CompAnimState),
    CompVegStateBit     = (1u << CompVegState),
    CompSoundSourceBit  = (1u << CompSoundSource),
    CompPhysicsBit      = (1u << CompPhysics),
    CompAccStructureBit = (1u << CompAccStructure)
};

const int MAX_COMPONENT_TYPES = 32;

const float LIGHT_ATTEN_CUTOFF = 0.004f;

struct SceneObject {
    uint32_t    comp_mask, change_mask, last_change_mask;
    uint32_t    components[MAX_COMPONENT_TYPES];
    Ren::String name;

    SceneObject() : comp_mask(0), change_mask(0), last_change_mask(0) {}    // NOLINT
    SceneObject(const SceneObject &rhs) = delete;
    SceneObject(SceneObject &&rhs) noexcept = default;

    SceneObject &operator=(const SceneObject &rhs) = delete;
    SceneObject &operator=(SceneObject &&rhs) = default;
};
//static_assert(sizeof(SceneObject) == 156 + 4, "!");

struct bvh_node_t { // NOLINT
    Ren::Vec3f bbox_min;
    union {
        struct {
            uint32_t leaf_node : 1;
            uint32_t prim_index : 31;
        };
        struct {
            uint32_t leaf_node : 1;
            uint32_t left_child : 31;
        };
    };
    Ren::Vec3f bbox_max;
    union {
        struct {
            uint32_t space_axis : 2;
            uint32_t prim_count : 30;
        };
        struct {
            uint32_t space_axis : 2;
            uint32_t right_child : 30;
        };
    };
    uint32_t parent;
};
static_assert(sizeof(bvh_node_t) == 36, "!");

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
    uint32_t vert_index, vert_count;
};
static_assert(sizeof(gpu_mesh_t) == 24, "!");

struct gpu_mesh_instance_t {
    Ren::Vec3f bbox_min;
    uint32_t tr_index;
    Ren::Vec3f bbox_max;
    uint32_t mesh_index;
};
static_assert(sizeof(gpu_mesh_instance_t) == 32, "!");

const int MAX_STACK_SIZE = 64;

struct Environment {
    Ren::Vec3f          sun_dir, sun_col;
    float               sun_softness = 0.0f;
    Ren::Vec3f          wind_vec;
    float               wind_turbulence = 0.0f;
    Ren::Vec2f          prev_wind_scroll_lf, prev_wind_scroll_hf;
    Ren::Vec2f          curr_wind_scroll_lf, curr_wind_scroll_hf;
    Ren::Tex2DRef       env_map;
    Ren::Tex2DRef       lm_direct, lm_indir,
                        lm_indir_sh[4];
    float               sun_shadow_bias[2] = { 4.0f, 8.0f };

    Ren::String         env_map_name, env_map_name_pt;
};

struct BBox {
    Ren::Vec3f bmin, bmax;
};

struct TexEntry {
    uint32_t index;
    union {
        struct {
            uint32_t cam_dist : 16;
            uint32_t prio : 4;
            uint32_t _unused : 12;
        };
        uint32_t sort_key = 0;
    };
};
static_assert(sizeof(TexEntry) == 8, "!");

class CompStorage {
public:
    virtual ~CompStorage() = default;
    virtual const char *name() const = 0;

    virtual uint32_t Create() = 0;
    virtual void Delete(uint32_t i) = 0;
    virtual void *Get(uint32_t i) = 0;
    virtual const void *Get(uint32_t i) const = 0;

    virtual uint32_t First() const = 0;
    virtual uint32_t Next(uint32_t i) const = 0;

    virtual int Count() const = 0;

    virtual void ReadFromJs(const JsObjectP &js_obj, void *comp) = 0;
    virtual void WriteToJs(const void *comp, JsObjectP &js_obj) const = 0;

    // returns contiguous array of data or null if storage does not support it
    virtual const void *SequentialData() const { return nullptr; }
    virtual void *SequentialData() { return nullptr; }
};

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#endif

struct PersistentGpuData {
    Ren::BufferRef                          materials_buf;
#if defined(USE_VK_RENDER)
    std::unique_ptr<Ren::DescrPool>         textures_descr_pool;
    VkDescriptorSetLayout                   textures_descr_layout = VK_NULL_HANDLE;
    std::unique_ptr<Ren::DescrPool>         rt_textures_descr_pool, rt_inline_textures_descr_pool;
    VkDescriptorSetLayout                   rt_textures_descr_layout = VK_NULL_HANDLE,
                                            rt_inline_textures_descr_layout = VK_NULL_HANDLE;
    Ren::SmallVector<VkDescriptorSet, 1024> textures_descr_sets[4];
    VkDescriptorSet                         rt_textures_descr_sets[4], rt_inline_textures_descr_sets[4];
#elif defined(USE_GL_RENDER)
    Ren::BufferRef                          textures_buf;
#endif
    Ren::PipelineStorage                    pipelines;

    Ren::BufferRef                          rt_instance_buf, rt_geo_data_buf, rt_tlas_buf, rt_sh_tlas_buf, rt_blas_buf;
    Ren::BufferRef                          rt_prim_indices_buf; // used for SWRT only
    uint32_t                                rt_tlas_build_scratch_size = 0;
    std::unique_ptr<Ren::IAccStructure>     rt_tlas, rt_sh_tlas;

    PersistentGpuData();
    ~PersistentGpuData();

    PersistentGpuData(PersistentGpuData &&rhs) noexcept = delete;

    void Clear();
};

struct SceneData {
    Ren::String                             name;
    
    Ren::Texture2DStorage                   textures;
    Ren::MaterialStorage                    materials;
    std::vector<uint32_t>                   material_changes;
    PersistentGpuData                       persistent_data = {};
    std::pair<uint32_t, uint32_t>           mat_update_ranges[4];
    Ren::MeshStorage                        meshes;

    std::vector<uint32_t>                   texture_mem_buckets;
    uint32_t                                tex_mem_bucket_index = 0;
    std::atomic<uint64_t>                   estimated_texture_mem = {};

    Environment                             env;

    Ren::HashMap32<Ren::String, Ren::Vec4f> decals_textures;
    Ren::TextureAtlas                       decals_atlas;
    Ren::TextureSplitter                    lm_splitter;
    Ren::ProbeStorage                       probe_storage;

    CompStorage                             *comp_store[MAX_COMPONENT_TYPES] = {};

    std::vector<SceneObject>                objects;
    Ren::HashMap32<Ren::String, uint32_t>   name_to_object;

    std::vector<bvh_node_t>                 nodes;
    std::vector<uint32_t>                   free_nodes;
    uint32_t                                root_node = 0xffffffff;

    uint32_t                                update_counter = 0;
};
