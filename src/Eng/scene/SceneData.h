#pragma once

#include <cstdint>

#include <atomic>

#include <Ren/HashMap32.h>
#include <Ren/MMat.h>
#include <Ren/Mesh.h>
#include <Ren/Pipeline.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Storage.h>
#include <Ren/TextureArray.h>
#include <Ren/TextureAtlas.h>

#include "Atmosphere.h"
#include "components/AccStructure.h"
#include "components/AnimState.h"
#include "components/Decal.h"
#include "components/Drawable.h"
#include "components/LightProbe.h"
#include "components/LightSource.h"
#include "components/Lightmap.h"
#include "components/Occluder.h"
#include "components/Physics.h"
#include "components/SoundSource.h"
#include "components/Transform.h"
#include "components/VegState.h"

namespace Eng {
enum eObjectComp : uint32_t {
    CompTransform = 0,
    CompDrawable = 1,
    CompOccluder = 2,
    CompLightmap = 3,
    CompLightSource = 4,
    CompDecal = 5,
    CompProbe = 6,
    CompAnimState = 7,
    CompVegState = 8,
    CompSoundSource = 9,
    CompPhysics = 10,
    CompAccStructure = 11,
};

enum eObjectCompBit : uint32_t {
    CompTransformBit = (1u << CompTransform),
    CompDrawableBit = (1u << CompDrawable),
    CompOccluderBit = (1u << CompOccluder),
    CompLightmapBit = (1u << CompLightmap),
    CompLightSourceBit = (1u << CompLightSource),
    CompDecalBit = (1u << CompDecal),
    CompProbeBit = (1u << CompProbe),
    CompAnimStateBit = (1u << CompAnimState),
    CompVegStateBit = (1u << CompVegState),
    CompSoundSourceBit = (1u << CompSoundSource),
    CompPhysicsBit = (1u << CompPhysics),
    CompAccStructureBit = (1u << CompAccStructure)
};

const int MAX_COMPONENT_TYPES = 32;

const float LIGHT_ATTEN_CUTOFF = 0.004f;

struct SceneObject {
    uint32_t comp_mask, change_mask, last_change_mask;
    uint32_t components[MAX_COMPONENT_TYPES];
    Ren::String name;

    SceneObject() : comp_mask(0), change_mask(0), last_change_mask(0) {} // NOLINT
    SceneObject(const SceneObject &rhs) = delete;
    SceneObject(SceneObject &&rhs) noexcept = default;

    SceneObject &operator=(const SceneObject &rhs) = delete;
    SceneObject &operator=(SceneObject &&rhs) = default;
};
// static_assert(sizeof(SceneObject) == 156 + 4, "!");

struct bvh_node_t { // NOLINT
    Ren::Vec3f bbox_min;
    union {
        struct {
            uint32_t leaf_node : 1;
            uint32_t prim_index : 31;
        };
        struct {
            uint32_t _leaf_node : 1;
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
            uint32_t _space_axis : 2;
            uint32_t right_child : 30;
        };
    };
    uint32_t parent;
};
static_assert(sizeof(bvh_node_t) == 36, "!");

const int MAX_STACK_SIZE = 64;

struct Environment {
    Ren::Vec3f sun_dir, sun_col;
    float sun_angle = 0.0f;
    Ren::Vec3f wind_vec;
    float wind_turbulence = 0.0f;
    Ren::Vec2f prev_wind_scroll_lf, prev_wind_scroll_hf;
    Ren::Vec2f curr_wind_scroll_lf, curr_wind_scroll_hf;
    Ren::Vec3f env_col;
    float env_map_rot = 0.0f;
    Ren::Tex2DRef env_map;
    Ren::Tex2DRef lm_direct, lm_indir, lm_indir_sh[4];
    float sun_shadow_bias[2] = {4.0f, 8.0f};
    uint32_t generation = 0;

    Ren::String env_map_name;
    AtmosphereParams atmosphere;
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
    [[nodiscard]] virtual std::string_view name() const = 0;

    [[nodiscard]] virtual uint32_t Create() = 0;
    virtual void Delete(uint32_t i) = 0;
    [[nodiscard]] virtual void *Get(uint32_t i) = 0;
    [[nodiscard]] virtual const void *Get(uint32_t i) const = 0;

    [[nodiscard]] virtual uint32_t First() const = 0;
    [[nodiscard]] virtual uint32_t Next(uint32_t i) const = 0;

    [[nodiscard]] virtual int Count() const = 0;

    virtual void ReadFromJs(const JsObjectP &js_obj, void *comp) = 0;
    virtual void WriteToJs(const void *comp, JsObjectP &js_obj) const = 0;

    // returns contiguous array of data or null if storage does not support it
    [[nodiscard]] virtual const void *SequentialData() const { return nullptr; }
    [[nodiscard]] virtual void *SequentialData() { return nullptr; }
};
} // namespace Eng

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#endif

namespace Eng {
struct ProbeVolume {
    mutable Ren::Vec3f origin, spacing;
    mutable Ren::Vec3i scroll, scroll_diff;
    mutable int updates_count = 0;
};

struct LightItem {
    float col[3];
    uint32_t type_and_flags;
    float pos[3], radius;
    float dir[3], spot;
    float u[3];
    int shadowreg_index;
    float v[3], blend;
    float shadow_pos[3];
    uint32_t tri_index;
};
static_assert(sizeof(LightItem) == 96, "!");

static const uint32_t RtBLASChunkSize = 16 * 1024 * 1024;

struct PersistentGpuData {
    Ren::BufferRef instance_buf;
    Ren::Tex1DRef instance_buf_tbo;
    Ren::BufferRef materials_buf;
    Ren::BufferRef stoch_lights_buf, stoch_lights_nodes_buf;
#if defined(USE_VK_RENDER)
    std::unique_ptr<Ren::DescrPool> textures_descr_pool;
    VkDescriptorSetLayout textures_descr_layout = VK_NULL_HANDLE;
    std::unique_ptr<Ren::DescrPool> rt_textures_descr_pool, rt_inline_textures_descr_pool;
    VkDescriptorSetLayout rt_textures_descr_layout = VK_NULL_HANDLE, rt_inline_textures_descr_layout = VK_NULL_HANDLE;
    Ren::SmallVector<VkDescriptorSet, 1024> textures_descr_sets[4];
    VkDescriptorSet rt_textures_descr_sets[4], rt_inline_textures_descr_sets[4];
#elif defined(USE_GL_RENDER)
    Ren::BufferRef textures_buf;
#endif
    Ren::PipelineStorage pipelines;

    struct {
        Ren::FreelistAlloc rt_blas_mem_alloc;
        std::vector<Ren::BufferRef> rt_blas_buffers;
    } hwrt;

    Ren::BufferRef rt_tlas_buf, rt_sh_tlas_buf;

    struct {
        Ren::BufferRef rt_prim_indices_buf;
        uint32_t rt_root_node = 0;
        Ren::BufferRef rt_meshes_buf;
        Ren::BufferRef rt_blas_buf;
    } swrt;
    uint32_t rt_tlas_build_scratch_size = 0;
    std::unique_ptr<Ren::IAccStructure> rt_tlas, rt_sh_tlas;

    std::unique_ptr<Ren::Texture2DArray> probe_ray_data;
    std::unique_ptr<Ren::Texture2DArray> probe_irradiance;
    std::unique_ptr<Ren::Texture2DArray> probe_distance;
    std::unique_ptr<Ren::Texture2DArray> probe_offset;
    std::vector<ProbeVolume> probe_volumes;

    mutable bool reset_probe_relocation = true;
    mutable bool reset_probe_classification = true;

    PersistentGpuData();
    ~PersistentGpuData();

    PersistentGpuData(PersistentGpuData &&rhs) noexcept = delete;

    void Clear();
};

struct SceneData {
    Ren::String name;

    Ren::Texture2DStorage textures;
    Ren::MaterialStorage materials;
    std::vector<uint32_t> material_changes;
    PersistentGpuData persistent_data;
    std::pair<uint32_t, uint32_t> mat_update_ranges[4];
    Ren::MeshStorage meshes;

    std::vector<uint32_t> texture_mem_buckets;
    uint32_t tex_mem_bucket_index = 0;
    std::atomic<uint64_t> estimated_texture_mem = {};

    Environment env;

    Ren::HashMap32<Ren::String, Ren::Vec4f> decals_textures;
    Ren::TextureAtlas decals_atlas;
    Ren::TextureSplitter lm_splitter;

    CompStorage *comp_store[MAX_COMPONENT_TYPES] = {};

    std::vector<SceneObject> objects;
    Ren::HashMap32<Ren::String, uint32_t> name_to_object;
    Ren::HashMap32<uint32_t, uint32_t> object_counts;

    std::vector<bvh_node_t> nodes;
    std::vector<uint32_t> free_nodes;
    uint32_t root_node = 0xffffffff;

    uint32_t update_counter = 0;
};
} // namespace Eng
