#pragma once

#include <cstdint>

#include <Ren/HashMap32.h>
#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>
#include <Ren/TextureAtlas.h>

#include "ProbeStorage.h"

#include "Comp/AnimState.h"
#include "Comp/Decal.h"
#include "Comp/Drawable.h"
#include "Comp/Lightmap.h"
#include "Comp/LightProbe.h"
#include "Comp/LightSource.h"
#include "Comp/Occluder.h"
#include "Comp/Transform.h"

enum eObjectComp {
    CompTransform   = 0,
    CompDrawable    = 1,
    CompOccluder    = 2,
    CompLightmap    = 3,
    CompLightSource = 4,
    CompDecal       = 5,
    CompProbe       = 6,
    CompAnimState   = 7
};

enum eObjectCompBit {
    CompTransformBit    = (1 << CompTransform),
    CompDrawableBit     = (1 << CompDrawable),
    CompOccluderBit     = (1 << CompOccluder),
    CompLightmapBit     = (1 << CompLightmap),
    CompLightSourceBit  = (1 << CompLightSource),
    CompDecalBit        = (1 << CompDecal),
    CompProbeBit        = (1 << CompProbe),
    CompAnimStateBit    = (1 << CompAnimState)
};

const int MAX_COMPONENT_TYPES = 32;

const float LIGHT_ATTEN_CUTOFF = 0.004f;

struct SceneObject {
    uint32_t    comp_mask, change_mask, last_change_mask;
    uint32_t    components[MAX_COMPONENT_TYPES];
    Ren::String name;

    SceneObject() : comp_mask(0), change_mask(0), last_change_mask(0) {}
    SceneObject(const SceneObject &rhs) = delete;
    SceneObject(SceneObject &&rhs) = default;

    SceneObject &operator=(const SceneObject &rhs) = delete;
    SceneObject &operator=(SceneObject &&rhs) = default;
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

#define MAX_STACK_SIZE 64

struct Environment {
    Ren::Vec3f          sun_dir, sun_col;
    float               sun_softness = 0.0f;
    Ren::Texture2DRef   env_map;
    Ren::Texture2DRef   lm_direct, lm_indir,
                        lm_indir_sh[4];

    Ren::String         env_map_name, env_map_name_pt;
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
    virtual const void *Get(uint32_t i) const = 0;

    virtual uint32_t First() const = 0;
    virtual uint32_t Next(uint32_t i) const = 0;

    virtual int Count() const = 0;

    virtual void ReadFromJs(const JsObject &js_obj, void *comp) = 0;
    virtual void WriteToJs(const void *comp, JsObject &js_obj) const = 0;

    // tells whether it is possible to access storage as if it is contiguous array
    virtual bool IsSequential() const { return false; }
};

struct SceneData {
    Ren::String             name;
    Environment             env;
    Ren::TextureAtlas       decals_atlas;
    Ren::TextureSplitter    lm_splitter;
    ProbeStorage            probe_storage;

    CompStorage                     *comp_store[MAX_COMPONENT_TYPES] = {};

    std::vector<SceneObject>        objects;
    Ren::HashMap32<Ren::String, uint32_t> name_to_object;

    std::vector<bvh_node_t>         nodes;
    std::vector<uint32_t>           free_nodes;
    uint32_t                        root_node = 0xffffffff;

    uint32_t                        update_counter = 0;
};