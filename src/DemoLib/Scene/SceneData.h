#pragma once

#include <cstdint>

#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>
#include <Ren/TextureAtlas.h>

#include "ProbeStorage.h"

#include "Comp/Decal.h"
#include "Comp/Drawable.h"
#include "Comp/Lightmap.h"
#include "Comp/LightProbe.h"
#include "Comp/LightSource.h"
#include "Comp/Occluder.h"
#include "Comp/Transform.h"

enum eObjectComp {
    CompTransform  = 0,
    CompDrawable   = 1,
    CompOccluder   = 2,
    CompLightmap   = 3,
    CompLightSource = 4,
    CompDecal      = 5,
    CompProbe      = 6
};

enum eObjectCompBit {
    CompTransformBit   = (1 << CompTransform),
    CompDrawableBit    = (1 << CompDrawable),
    CompOccluderBit    = (1 << CompOccluder),
    CompLightmapBit    = (1 << CompLightmap),
    CompLightSourceBit = (1 << CompLightSource),
    CompDecalBit       = (1 << CompDecal),
    CompProbeBit       = (1 << CompProbe)
};

const int MAX_COMPONENT_TYPES = 32;

const float LIGHT_ATTEN_CUTOFF = 0.004f;

struct SceneObject {
    uint32_t comp_mask, change_mask, last_change_mask;
    uint32_t components[MAX_COMPONENT_TYPES];
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0,
             end_timepoint_us = 0;
    uint32_t occluders_time_us = 0,
             main_gather_time_us = 0,
             shadow_gather_time_us = 0,
             drawables_sort_time_us = 0,
             items_assignment_time_us = 0;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0,
             cpu_end_timepoint_us = 0;
    uint64_t gpu_start_timepoint_us = 0,
             gpu_end_timepoint_us = 0;
    uint32_t shadow_time_us = 0,
             depth_opaque_pass_time_us = 0,
             ao_pass_time_us = 0,
             opaque_pass_time_us = 0,
             transp_pass_time_us = 0,
             refl_pass_time_us = 0,
             blur_pass_time_us = 0,
             blit_pass_time_us = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    uint32_t shadow_draw_calls_count = 0,
             depth_fill_draw_calls_count = 0,
             opaque_draw_calls_count = 0;
};

struct bvh_node_t {
    uint32_t prim_index, prim_count,
             left_child, right_child;
    Ren::Vec3f bbox_min;
    uint32_t parent;
    Ren::Vec3f bbox_max;
    uint32_t space_axis; // axis with maximal child's centroids distance
};
static_assert(sizeof(bvh_node_t) == 48, "!");

struct Environment {
    Ren::Vec3f  sun_dir, sun_col;
    float       sun_softness = 0.0f;
    Ren::Texture2DRef env_map;
    Ren::Texture2DRef lm_direct, lm_indir,
                      lm_indir_sh[4];
};

struct BBox {
    Ren::Vec3f bmin, bmax;
};

class CompStorage {
public:
    virtual ~CompStorage() {}
    virtual const char *name() const = 0;

    virtual uint32_t Create() = 0;
    virtual void *Get(uint32_t i) = 0;

    virtual void ReadFromJs(const JsObject &js_obj, void *comp) = 0;
    virtual void WriteToJs(const void *comp, JsObject &js_obj) = 0;
};

struct SceneData {
    Environment             env;
    Ren::TextureAtlas       decals_atlas;
    Ren::TextureSplitter    lm_splitter;
    ProbeStorage            probe_storage;

    CompStorage                     *comp_store[MAX_COMPONENT_TYPES] = {};

    std::vector<SceneObject>        objects;

    std::vector<bvh_node_t>         nodes;
    std::vector<uint32_t>           free_nodes;
    uint32_t                        root_node = 0;

    uint32_t                        update_counter = 0;
};