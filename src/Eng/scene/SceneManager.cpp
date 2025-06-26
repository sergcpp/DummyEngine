#include "SceneManager.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <map>

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Snd/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "TexUpdateFileBuf.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

#ifdef _MSC_VER
#include <intrin.h>

#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_bittestandcomplement)
#endif

#include "../utils/Load.h"
#include "../utils/ShaderLoader.h"

namespace SceneManagerConstants {
const float NEAR_CLIP = 0.05f;
const float FAR_CLIP = 10000.0f;

const int SCROLLING_DISTANCE = 64;

extern const int DECALS_ATLAS_RESX = 4096 / 4, DECALS_ATLAS_RESY = 2048 / 4;
extern const int LIGHTMAP_ATLAS_RESX = 2048, LIGHTMAP_ATLAS_RESY = 1024;

const float DefaultSunShadowBias[2] = {4.0f, 8.0f};

// const int PROBE_RES = 512;
// const int PROBE_COUNT = 1;

__itt_string_handle *itt_load_scene_str = __itt_string_handle_create("SceneManager::LoadScene");
__itt_string_handle *itt_serve_str = __itt_string_handle_create("SceneManager::Serve");
__itt_string_handle *itt_on_loaded_str = __itt_string_handle_create("SceneManager::OnTextureDataLoaded");
} // namespace SceneManagerConstants

namespace SceneManagerInternal {
template <typename T> class DefaultCompStorage : public Eng::CompStorage {
    Ren::SparseArray<T> data_;

  public:
    [[nodiscard]] std::string_view name() const override { return T::name(); }

    uint32_t Create() override { return data_.emplace(); }
    void Delete(const uint32_t i) override { data_.erase(i); }

    [[nodiscard]] const void *Get(uint32_t i) const override { return data_.GetOrNull(i); }
    [[nodiscard]] void *Get(uint32_t i) override { return data_.GetOrNull(i); }

    [[nodiscard]] uint32_t First() const override { return data_.empty() ? 0xffffffff : data_.cbegin().index(); }

    [[nodiscard]] uint32_t Next(uint32_t i) const override {
        auto it = data_.citer_at(i);
        ++it;
        return (it == data_.cend()) ? 0xffffffff : it.index();
    }

    [[nodiscard]] int Count() const override { return int(data_.size()); }

    void ReadFromJs(const Sys::JsObjectP &js_obj, void *comp) override { T::Read(js_obj, *(T *)comp); }
    void WriteToJs(const void *comp, Sys::JsObjectP &js_obj) const override { T::Write(*(T *)comp, js_obj); }

    [[nodiscard]] const void *SequentialData() const override { return data_.data(); }
    [[nodiscard]] void *SequentialData() override { return data_.data(); }
};

// bit scan forward
long GetFirstBit(long mask) {
#ifdef _MSC_VER
    unsigned long ret;
    _BitScanForward(&ret, (unsigned long)mask);
    return long(ret);
#else
    return long(__builtin_ffsl(mask) - 1);
#endif
}

// bit test and complement
long ClearBit(long mask, long index) {
#ifdef _MSC_VER
    _bittestandcomplement(&mask, index);
    return mask;
#else
    return (mask & ~(1 << index));
#endif
}

int16_t f32_to_s16(const float value) { return int16_t(value * 32767); }
uint16_t f32_to_u16(const float value) { return uint16_t(value * 65535); }
uint8_t f32_to_u8(const float value) { return uint8_t(value * 255); }

void __init_wind_params(const Eng::VegState &vs, const Eng::Environment &env, const Ren::Mat4f &object_from_world,
                        Eng::instance_data_t &instance) {
    instance.movement_scale = f32_to_u8(vs.movement_scale);
    instance.tree_mode = f32_to_u8(vs.tree_mode);
    instance.bend_scale = f32_to_u8(vs.bend_scale);
    instance.stretch = f32_to_u8(vs.stretch);

    const auto wind_vec_ws = Ren::Vec4f{env.wind_vec[0], env.wind_vec[1], env.wind_vec[2], 0.0f};
    const Ren::Vec4f wind_vec_ls = object_from_world * wind_vec_ws;

    instance.wind_dir_ls[0] = Ren::f32_to_f16(wind_vec_ls[0]);
    instance.wind_dir_ls[1] = Ren::f32_to_f16(wind_vec_ls[1]);
    instance.wind_dir_ls[2] = Ren::f32_to_f16(wind_vec_ls[2]);
    instance.wind_turb = Ren::f32_to_f16(env.wind_turbulence);
}

#include "precomputed/__cam_rig.inl"
} // namespace SceneManagerInternal

Eng::SceneManager::SceneManager(Ren::Context &ren_ctx, Eng::ShaderLoader &sh, Snd::Context *snd_ctx,
                                Sys::ThreadPool &threads, const path_config_t &paths)
    : ren_ctx_(ren_ctx), sh_(sh), snd_ctx_(snd_ctx), threads_(threads), paths_(paths),
      cam_(Ren::Vec3f{0.0f, 0.0f, 1.0f}, Ren::Vec3f{0.0f, 0.0f, 0.0f}, Ren::Vec3f{0.0f, 1.0f, 0.0f}) {
    using namespace SceneManagerConstants;
    using namespace SceneManagerInternal;

    { // Alloc texture for decals atlas
        const Ren::eTexFormat formats[] = {Ren::DefaultCompressedRGBA, Ren::eTexFormat::Undefined};
        const Ren::Bitmask<Ren::eTexFlags> flags[] = {{}};
        scene_data_.decals_atlas =
            Ren::TextureAtlas{ren_ctx.api_ctx(),          DECALS_ATLAS_RESX, DECALS_ATLAS_RESY, 64, 1, formats, flags,
                              Ren::eTexFilter::Trilinear, ren_ctx_.log()};
    }

    { // Create splitter for lightmap atlas
        scene_data_.lm_splitter = Ren::TextureSplitter(SceneManagerConstants::LIGHTMAP_ATLAS_RESX,
                                                       SceneManagerConstants::LIGHTMAP_ATLAS_RESY);
    }

    /*{ // Allocate cubemap array
        const bool res =
            scene_data_.probe_storage.Resize(ren_ctx.api_ctx(), ren_ctx.default_mem_allocs(),
                                             Ren::DefaultCompressedRGBA, PROBE_RES, PROBE_COUNT, ren_ctx_.log());
        assert(res);
    }*/

    { // Register default components
        using namespace std::placeholders;

        default_comp_storage_[CompTransform] = std::make_unique<DefaultCompStorage<Transform>>();
        RegisterComponent(CompTransform, default_comp_storage_[CompTransform].get(), nullptr);

        default_comp_storage_[CompDrawable] = std::make_unique<DefaultCompStorage<Drawable>>();
        RegisterComponent(CompDrawable, default_comp_storage_[CompDrawable].get(),
                          std::bind(&SceneManager::PostloadDrawable, this, _1, _2, _3));

        default_comp_storage_[CompOccluder] = std::make_unique<DefaultCompStorage<Occluder>>();
        RegisterComponent(CompOccluder, default_comp_storage_[CompOccluder].get(),
                          std::bind(&SceneManager::PostloadOccluder, this, _1, _2, _3));

        default_comp_storage_[CompLightmap] = std::make_unique<DefaultCompStorage<Lightmap>>();
        RegisterComponent(CompLightmap, default_comp_storage_[CompLightmap].get(),
                          std::bind(&SceneManager::PostloadLightmap, this, _1, _2, _3));

        default_comp_storage_[CompLightSource] = std::make_unique<DefaultCompStorage<LightSource>>();
        RegisterComponent(CompLightSource, default_comp_storage_[CompLightSource].get(),
                          std::bind(&SceneManager::PostloadLightSource, this, _1, _2, _3));

        default_comp_storage_[CompDecal] = std::make_unique<DefaultCompStorage<Decal>>();
        RegisterComponent(CompDecal, default_comp_storage_[CompDecal].get(),
                          std::bind(&SceneManager::PostloadDecal, this, _1, _2, _3));

        default_comp_storage_[CompProbe] = std::make_unique<DefaultCompStorage<LightProbe>>();
        RegisterComponent(CompProbe, default_comp_storage_[CompProbe].get(),
                          std::bind(&SceneManager::PostloadLightProbe, this, _1, _2, _3));

        default_comp_storage_[CompAnimState] = std::make_unique<DefaultCompStorage<AnimState>>();
        RegisterComponent(CompAnimState, default_comp_storage_[CompAnimState].get(), nullptr);

        default_comp_storage_[CompVegState] = std::make_unique<DefaultCompStorage<VegState>>();
        RegisterComponent(CompVegState, default_comp_storage_[CompVegState].get(), nullptr);

        default_comp_storage_[CompSoundSource] = std::make_unique<DefaultCompStorage<SoundSource>>();
        RegisterComponent(CompSoundSource, default_comp_storage_[CompSoundSource].get(),
                          std::bind(&SceneManager::PostloadSoundSource, this, _1, _2, _3));

        default_comp_storage_[CompPhysics] = std::make_unique<DefaultCompStorage<Physics>>();
        RegisterComponent(CompPhysics, default_comp_storage_[CompPhysics].get(), nullptr);

        default_comp_storage_[CompAccStructure] = std::make_unique<DefaultCompStorage<AccStructure>>();
        RegisterComponent(CompAccStructure, default_comp_storage_[CompAccStructure].get(),
                          std::bind(&SceneManager::PostloadAccStructure, this, _1, _2, _3));
    }

    { // Load cam rig
        Sys::MemBuf buf{__cam_rig_mesh, size_t(__cam_rig_mesh_size)};
        std::istream in_mesh(&buf);

        Ren::eMeshLoadStatus status;
        cam_rig_ = ren_ctx.LoadMesh(
            "__cam_rig", &in_mesh,
            [this](std::string_view name) -> std::array<Ren::MaterialRef, 3> {
                Ren::eMatLoadStatus status;
                Ren::MaterialRef mat = ren_ctx_.LoadMaterial(name, {}, &status, nullptr, nullptr, nullptr);
                return std::array<Ren::MaterialRef, 3>{mat, mat, {}};
            },
            &status);
        assert(status == Ren::eMeshLoadStatus::CreatedFromData);
    }

    Ren::ILog *log = ren_ctx_.log();

    { // create white texture
        Ren::TexParams p;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.format = Ren::eTexFormat::RGBA8;
        p.w = p.h = 1;

        static const uint8_t data[4] = {255, 255, 255, 255};

        Ren::eTexLoadStatus status;
        white_tex_ = ren_ctx_.LoadTexture("White Tex", data, p, ren_ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // load error texture
        std::string name_buf = paths_.textures_path;
        name_buf += "internal/error.dds";

        Ren::TexParams p;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;

        const std::vector<uint8_t> data = LoadDDS(name_buf, &p);
        if (!data.empty()) {
            Ren::eTexLoadStatus status;
            error_tex_ = ren_ctx_.LoadTexture(name_buf, data, p, ren_ctx_.default_mem_allocs(), &status);
            assert(status == Ren::eTexLoadStatus::CreatedFromData);
        } else {
            log->Error("SceneManager: Failed to load error.dds!");
        }
    }

    requested_textures_.reserve(16384);
    finished_textures_.reserve(16384);

    if (snd_ctx_) {
        const float pos[] = {0.0f, 0.0f, 0.0f};
        amb_sound_.Init(1.0f, pos);
    }

    for (auto &range : scene_data_.mat_update_ranges) {
        range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);
    }

    // AllocMeshBuffers();
    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    scene_data_.persistent_data.vertex_buf1 =
        scene_data_.buffers.Insert("VtxBuf1", api_ctx, Ren::eBufType::VertexAttribs, 128, 16);
    scene_data_.persistent_data.vertex_buf1->AddBufferView(Ren::eTexFormat::RGBA32F);
    scene_data_.persistent_data.vertex_buf2 =
        scene_data_.buffers.Insert("VtxBuf2", api_ctx, Ren::eBufType::VertexAttribs, 128, 16);
    scene_data_.persistent_data.vertex_buf2->AddBufferView(Ren::eTexFormat::RGBA32UI);
    scene_data_.persistent_data.skin_vertex_buf =
        scene_data_.buffers.Insert("SkinVtxBuf", api_ctx, Ren::eBufType::VertexAttribs, 128, 16);
    scene_data_.persistent_data.delta_buf =
        scene_data_.buffers.Insert("DeltaBuf", api_ctx, Ren::eBufType::VertexAttribs, 128, 16);
    scene_data_.persistent_data.indices_buf =
        scene_data_.buffers.Insert("NdxBuf", api_ctx, Ren::eBufType::VertexIndices, 128, 4);
    scene_data_.persistent_data.indices_buf->AddBufferView(Ren::eTexFormat::R32UI);

    StartTextureLoaderThread();
}

Eng::SceneManager::~SceneManager() {
    StopTextureLoaderThread();
    ClearScene();
}

void Eng::SceneManager::RegisterComponent(const uint32_t index, CompStorage *storage,
                                          const std::function<PostLoadFunc> &post_init) {
    scene_data_.comp_store[index] = storage;
    component_post_load_[index] = post_init;
}

void Eng::SceneManager::LoadScene(const Sys::JsObjectP &js_scene, const Ren::Bitmask<eSceneLoadFlags> load_flags) {
    using namespace SceneManagerConstants;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_load_scene_str);

    Ren::ILog *log = ren_ctx_.log();
    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    log->Info("SceneManager: Loading scene!");
    {
        StopTextureLoaderThread();
        ClearScene();
        scene_data_.persistent_data.mem_allocs =
            std::make_unique<Ren::MemAllocators>("Scene Mem Allocs", api_ctx, 16 * 1024 * 1024 /* initial_block_size */,
                                                 1.5f /* growth_factor */, 128 * 1024 * 1024 /* max_pool_size */);
        // Temp. solution (prevent reallocation)
        scene_data_.textures.reserve(16384);
        if (load_flags & eSceneLoadFlags::Textures) {
            StartTextureLoaderThread();
        }
    }

    AllocMeshBuffers();

    std::map<std::string, Ren::Vec4f> decals_textures;

    if (js_scene.Has("name")) {
        const Sys::JsStringP &js_name = js_scene.at("name").as_str();
        scene_data_.name = Ren::String{js_name.val.c_str()};
    } else {
        throw std::runtime_error("Level has no name!");
    }

    scene_data_.load_flags = load_flags;

    /*{ // load lightmaps
        std::string lm_base_tex_name = "lightmaps/";
        lm_base_tex_name += scene_data_.name.c_str();

        const char tex_ext[] =
#if !defined(__ANDROID__)
            ".dds";
#else
            ".ktx";
#endif

        std::string lm_direct_tex_name = lm_base_tex_name;
        lm_direct_tex_name += "_lm_direct";
        lm_direct_tex_name += tex_ext;

        std::string lm_indir_tex_name = lm_base_tex_name;
        lm_indir_tex_name += "_lm_indirect";
        lm_indir_tex_name += tex_ext;

        const uint8_t default_l0_color[] = {255, 255, 255, 255};
        scene_data_.env.lm_direct = OnLoadTexture(lm_direct_tex_name, default_l0_color, Ren::eTexFlags{});
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            std::string lm_indir_sh_tex_name = lm_base_tex_name;
            lm_indir_sh_tex_name += "_lm_sh_";
            lm_indir_sh_tex_name += std::to_string(sh_l);
            lm_indir_sh_tex_name += tex_ext;

            const uint8_t default_l1_color[] = {0, 0, 0, 0};
            scene_data_.env.lm_indir_sh[sh_l] =
                OnLoadTexture(lm_indir_sh_tex_name, default_l1_color, Ren::eTexFlagBits::NoRepeat);
        }
    }*/

    const Sys::JsArrayP &js_objects = js_scene.at("objects").as_arr();
    for (const Sys::JsElementP &js_elem : js_objects.elements) {
        const Sys::JsObjectP &js_obj = js_elem.as_obj();

        SceneObject &obj = scene_data_.objects.emplace_back();

        Ren::Vec3f obj_bbox[2] = {Ren::Vec3f{std::numeric_limits<float>::max()},
                                  Ren::Vec3f{std::numeric_limits<float>::lowest()}};

        for (const auto &js_comp : js_obj.elements) {
            if (js_comp.second.type() != Sys::JsType::Object) {
                continue;
            }
            const Sys::JsObjectP &js_comp_obj = js_comp.second.as_obj();
            const auto &js_comp_name = js_comp.first;

            for (unsigned i = 0; i < MAX_COMPONENT_TYPES; i++) {
                CompStorage *store = scene_data_.comp_store[i];
                if (!store) {
                    continue;
                }

                if (js_comp_name == store->name()) {
                    const uint32_t index = store->Create();

                    void *new_component = store->Get(index);
                    store->ReadFromJs(js_comp_obj, new_component);

                    obj.components[i] = index;
                    obj.comp_mask |= (1u << i);

                    if (component_post_load_[i]) {
                        component_post_load_[i](js_comp_obj, new_component, obj_bbox);
                    }

                    break;
                }
            }
        }

        auto *tr = (Transform *)scene_data_.comp_store[CompTransform]->Get(obj.components[CompTransform]);
        tr->bbox_min = obj_bbox[0];
        tr->bbox_max = obj_bbox[1];
        tr->UpdateBBox();

        if (js_obj.Has("name")) {
            const Sys::JsStringP &js_name = js_obj.at("name").as_str();
            obj.name = Ren::String{js_name.val.c_str()};
            scene_data_.name_to_object[obj.name] = uint32_t(scene_data_.objects.size() - 1);
        }

        uint32_t *count = scene_data_.object_counts.Find(obj.comp_mask);
        if (count) {
            ++(*count);
        } else {
            scene_data_.object_counts.Insert(obj.comp_mask, 1);
        }
    }

    if (js_scene.Has("environment")) {
        const Sys::JsObjectP &js_env = js_scene.at("environment").as_obj();
        if (js_env.Has("sun_dir")) {
            const Sys::JsArrayP &js_dir = js_env.at("sun_dir").as_arr();

            const double x = js_dir.at(0).as_num().val, y = js_dir.at(1).as_num().val, z = js_dir.at(2).as_num().val;

            scene_data_.env.sun_dir = Ren::Vec3f{float(x), float(y), float(z)};
            scene_data_.env.sun_dir = Normalize(scene_data_.env.sun_dir);
        } else if (js_env.Has("sun_rot")) {
            const Sys::JsArrayP &js_rot = js_env.at("sun_rot").as_arr();

            const float rot[3] = {float(js_rot.at(0).as_num().val) * Ren::Pi<float>() / 180.0f,
                                  float(js_rot.at(1).as_num().val) * Ren::Pi<float>() / 180.0f,
                                  float(js_rot.at(2).as_num().val) * Ren::Pi<float>() / 180.0f};

            Ren::Mat4f transform;
            transform = Rotate(transform, rot[2], Ren::Vec3f{0.0f, 0.0f, 1.0f});
            transform = Rotate(transform, rot[0], Ren::Vec3f{1.0f, 0.0f, 0.0f});
            transform = Rotate(transform, rot[1], Ren::Vec3f{0.0f, 1.0f, 0.0f});

            const Ren::Vec4f sun_dir = Normalize(transform * Ren::Vec4f{0.0f, -1.0f, 0.0f, 0.0f});
            scene_data_.env.sun_dir[0] = sun_dir[0];
            scene_data_.env.sun_dir[1] = sun_dir[1];
            scene_data_.env.sun_dir[2] = sun_dir[2];
        }
        if (js_env.Has("sun_col")) {
            const Sys::JsArrayP &js_col = js_env.at("sun_col").as_arr();

            const double r = js_col.at(0).as_num().val, g = js_col.at(1).as_num().val, b = js_col.at(2).as_num().val;

            scene_data_.env.sun_col = Ren::Vec3f{float(r), float(g), float(b)};
        }
        if (js_env.Has("sun_angle")) {
            const Sys::JsNumber &js_sun_softness = js_env.at("sun_angle").as_num();
            scene_data_.env.sun_angle = float(js_sun_softness.val);
        }
        if (js_env.Has("env_col")) {
            const Sys::JsArrayP &js_col = js_env.at("env_col").as_arr();

            const double r = js_col.at(0).as_num().val, g = js_col.at(1).as_num().val, b = js_col.at(2).as_num().val;

            scene_data_.env.env_col = Ren::Vec3f{float(r), float(g), float(b)};
        }
        if (js_env.Has("env_map")) {
            const Sys::JsStringP &js_env_map = js_env.at("env_map").as_str();
            scene_data_.env.env_map_name = Ren::String{js_env_map.val.c_str()};
        }
        if (js_env.Has("env_map_rot")) {
            const Sys::JsNumber &js_env_map_rot = js_env.at("env_map_rot").as_num();
            scene_data_.env.env_map_rot = float(js_env_map_rot.val) * Ren::Pi<float>() / 180.0f;
        }

        if (js_env.Has("sun_shadow_bias")) {
            const Sys::JsArrayP &js_sun_shadow_bias = js_env.at("sun_shadow_bias").as_arr();
            scene_data_.env.sun_shadow_bias[0] = float(js_sun_shadow_bias.at(0).as_num().val);
            scene_data_.env.sun_shadow_bias[1] = float(js_sun_shadow_bias.at(1).as_num().val);
        } else {
            scene_data_.env.sun_shadow_bias[0] = DefaultSunShadowBias[0];
            scene_data_.env.sun_shadow_bias[1] = DefaultSunShadowBias[1];
        }

        if (js_env.Has("clouds_density")) {
            scene_data_.env.atmosphere.clouds_density = float(js_env.at("clouds_density").as_num().val);
        } else {
            scene_data_.env.atmosphere.clouds_density = 0.5f;
        }

        scene_data_.env.fog = {};
        if (js_env.Has("fog")) {
            const Sys::JsObjectP &js_fog = js_env.at("fog").as_obj();
            if (js_fog.Has("scatter_color")) {
                const Sys::JsArrayP &js_col = js_fog.at("scatter_color").as_arr();
                scene_data_.env.fog.scatter_color[0] = float(js_col[0].as_num().val);
                scene_data_.env.fog.scatter_color[1] = float(js_col[1].as_num().val);
                scene_data_.env.fog.scatter_color[2] = float(js_col[2].as_num().val);
            }
            if (js_fog.Has("density")) {
                scene_data_.env.fog.density = float(js_fog.at("density").as_num().val);
            }
            if (js_fog.Has("anisotropy")) {
                scene_data_.env.fog.anisotropy = float(js_fog.at("anisotropy").as_num().val);
            }
            if (js_fog.Has("absorption")) {
                scene_data_.env.fog.absorption = float(js_fog.at("absorption").as_num().val);
            }
            if (js_fog.Has("emission_color")) {
                const Sys::JsArrayP &js_col = js_fog.at("emission_color").as_arr();
                scene_data_.env.fog.emission_color[0] = float(js_col[0].as_num().val);
                scene_data_.env.fog.emission_color[1] = float(js_col[1].as_num().val);
                scene_data_.env.fog.emission_color[2] = float(js_col[2].as_num().val);
            }
            if (js_fog.Has("bbox_min")) {
                const Sys::JsArrayP &js_bbox_min = js_fog.at("bbox_min").as_arr();
                scene_data_.env.fog.bbox_min[0] = js_bbox_min[0].as_num().val;
                scene_data_.env.fog.bbox_min[1] = js_bbox_min[1].as_num().val;
                scene_data_.env.fog.bbox_min[2] = js_bbox_min[2].as_num().val;
            }
            if (js_fog.Has("bbox_max")) {
                const Sys::JsArrayP &js_bbox_max = js_fog.at("bbox_max").as_arr();
                scene_data_.env.fog.bbox_max[0] = js_bbox_max[0].as_num().val;
                scene_data_.env.fog.bbox_max[1] = js_bbox_max[1].as_num().val;
                scene_data_.env.fog.bbox_max[2] = js_bbox_max[2].as_num().val;
            }
        }
    } else {
        scene_data_.env = {};
    }

    LoadEnvMap();

    // scene_data_.probe_storage.Finalize();
    // LoadProbeCache();

    RebuildSceneBVH();
    RebuildMaterialTextureGraph();
    if (load_flags & eSceneLoadFlags::LightTree) {
        RebuildLightTree();
    }

    AllocMaterialsBuffer();
    AllocInstanceBuffer();
    Alloc_TLAS();
    AllocGICache();

    __itt_task_end(__g_itt_domain);
}

void Eng::SceneManager::SaveScene(Sys::JsObjectP &js_scene) {
    using namespace SceneManagerConstants;

    auto alloc = js_scene.get_allocator();
    // write name
    js_scene.Insert("name", Sys::JsStringP(scene_data_.name, alloc));

    { // write environment
        Sys::JsObjectP js_env(alloc);

        { // write sun direction
            Sys::JsArrayP js_sun_dir(alloc);
            js_sun_dir.Push(Sys::JsNumber(scene_data_.env.sun_dir[0]));
            js_sun_dir.Push(Sys::JsNumber(scene_data_.env.sun_dir[1]));
            js_sun_dir.Push(Sys::JsNumber(scene_data_.env.sun_dir[2]));

            js_env.Insert("sun_dir", std::move(js_sun_dir));
        }

        { // write sun color
            Sys::JsArrayP js_sun_col(alloc);
            js_sun_col.Push(Sys::JsNumber(scene_data_.env.sun_col[0]));
            js_sun_col.Push(Sys::JsNumber(scene_data_.env.sun_col[1]));
            js_sun_col.Push(Sys::JsNumber(scene_data_.env.sun_col[2]));

            js_env.Insert("sun_col", std::move(js_sun_col));
        }

        { // write sun softness
            js_env.Insert("sun_angle", Sys::JsNumber(scene_data_.env.sun_angle));
        }
        if (Length2(scene_data_.env.env_col) > FLT_EPSILON) {
            Sys::JsArrayP js_env_col(alloc);
            js_env_col.Push(Sys::JsNumber(scene_data_.env.env_col[0]));
            js_env_col.Push(Sys::JsNumber(scene_data_.env.env_col[1]));
            js_env_col.Push(Sys::JsNumber(scene_data_.env.env_col[2]));

            js_env.Insert("env_col", std::move(js_env_col));
        }
        if (scene_data_.env.env_map->name() != "dummy_white_cube") {
            js_env.Insert("env_map", Sys::JsStringP{scene_data_.env.env_map_name, alloc});
        }
        if (scene_data_.env.env_map_rot != 0.0f) {
            js_env.Insert("env_map_rot", Sys::JsNumber(scene_data_.env.env_map_rot * 180.0f / Ren::Pi<float>()));
        }

        if (scene_data_.env.sun_shadow_bias[0] != DefaultSunShadowBias[0] ||
            scene_data_.env.sun_shadow_bias[1] != DefaultSunShadowBias[1]) { // write sun shadow bias
            Sys::JsArrayP js_sun_shadow_bias(alloc);
            js_sun_shadow_bias.Push(Sys::JsNumber(scene_data_.env.sun_shadow_bias[0]));
            js_sun_shadow_bias.Push(Sys::JsNumber(scene_data_.env.sun_shadow_bias[1]));

            js_env.Insert("sun_shadow_bias", std::move(js_sun_shadow_bias));
        }

        if (scene_data_.env.fog != volume_params_t{}) {
            Sys::JsObjectP js_fog(alloc);

            /*if (scene_data_.env.fog.scatter_color != volume_params_t{}.scatter_color)*/ {
                Sys::JsArrayP js_col(alloc);
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.scatter_color[0]));
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.scatter_color[1]));
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.scatter_color[2]));
                js_fog.Insert("scatter_color", js_col);
            }

            if (scene_data_.env.fog.density != volume_params_t{}.density) {
                js_fog.Insert("density", Sys::JsNumber{scene_data_.env.fog.density});
            }
            if (scene_data_.env.fog.anisotropy != volume_params_t{}.anisotropy) {
                js_fog.Insert("anisotropy", Sys::JsNumber{scene_data_.env.fog.anisotropy});
            }

            if (scene_data_.env.fog.absorption != volume_params_t{}.absorption) {
                js_fog.Insert("absorption", Sys::JsNumber{scene_data_.env.fog.absorption});
            }

            if (scene_data_.env.fog.emission_color != volume_params_t{}.emission_color) {
                Sys::JsArrayP js_col(alloc);
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.emission_color[0]));
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.emission_color[1]));
                js_col.Push(Sys::JsNumber(scene_data_.env.fog.emission_color[2]));
                js_fog.Insert("emission_color", js_col);
            }

            if (scene_data_.env.fog.bbox_min != volume_params_t{}.bbox_min) {
                Sys::JsArrayP js_bbox_min(alloc);
                js_bbox_min.Push(Sys::JsNumber(scene_data_.env.fog.bbox_min[0]));
                js_bbox_min.Push(Sys::JsNumber(scene_data_.env.fog.bbox_min[1]));
                js_bbox_min.Push(Sys::JsNumber(scene_data_.env.fog.bbox_min[2]));
                js_fog.Insert("bbox_min", js_bbox_min);
            }

            if (scene_data_.env.fog.bbox_max != volume_params_t{}.bbox_max) {
                Sys::JsArrayP js_bbox_max(alloc);
                js_bbox_max.Push(Sys::JsNumber(scene_data_.env.fog.bbox_max[0]));
                js_bbox_max.Push(Sys::JsNumber(scene_data_.env.fog.bbox_max[1]));
                js_bbox_max.Push(Sys::JsNumber(scene_data_.env.fog.bbox_max[2]));
                js_fog.Insert("bbox_max", js_bbox_max);
            }

            js_env.Insert("fog", js_fog);
        }

        js_scene.Insert("environment", std::move(js_env));
    }

    { // write objects
        Sys::JsArrayP js_objects(alloc);

        const CompStorage *const *comp_storage = scene_data_.comp_store;

        for (const SceneObject &obj : scene_data_.objects) {
            Sys::JsObjectP js_obj(alloc);

            for (unsigned i = 0; i < MAX_COMPONENT_TYPES; i++) {
                if (obj.comp_mask & (1u << i)) {
                    const uint32_t comp_id = obj.components[i];
                    const void *p_comp = comp_storage[i]->Get(comp_id);

                    Sys::JsObjectP js_comp(alloc);
                    comp_storage[i]->WriteToJs(p_comp, js_comp);

                    js_obj.Insert(comp_storage[i]->name(), std::move(js_comp));
                }
            }

            js_objects.Push(std::move(js_obj));
        }

        js_scene.Insert("objects", std::move(js_objects));
    }
}

void Eng::SceneManager::ClearScene() {
    using namespace SceneManagerInternal;

    scene_data_.name = {};

    for (auto &obj : scene_data_.objects) {
        while (obj.comp_mask) {
            const long i = GetFirstBit(obj.comp_mask);
            obj.comp_mask = ClearBit(obj.comp_mask, i);
            scene_data_.comp_store[i]->Delete(obj.components[i]);
        }
    }

    for (int i = 0; i < MAX_COMPONENT_TYPES; i++) {
        if (scene_data_.comp_store[i]) {
            assert(scene_data_.comp_store[i]->Count() == 0);
        }
    }

    scene_data_.env = {};
    scene_data_.persistent_data.Release();

    assert(scene_data_.meshes.empty());
    assert(scene_data_.materials.empty());
    assert(scene_data_.textures.empty());
    assert(scene_data_.buffers.size() == 5);

    scene_data_.objects.clear();
    scene_data_.name_to_object.clear();
    scene_data_.lm_splitter.Clear();
    // scene_data_.probe_storage.Clear();
    scene_data_.nodes.clear();
    scene_data_.free_nodes.clear();
    scene_data_.update_counter = 0;

    for (auto &range : scene_data_.mat_update_ranges) {
        range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);
    }

    changed_objects_.clear();
    last_changed_objects_.clear();
}

void Eng::SceneManager::LoadEnvMap() {
    if (scene_data_.env.env_map_name == "physical_sky") {
        std::vector<uint8_t> black_cube(8 * 1023 * 1023, 0); // 1023 because of mips
        Ren::Span<const uint8_t> _black_cube[6];
        for (int i = 0; i < 6; ++i) {
            _black_cube[i] = black_cube;
        }

        Ren::TexParams p;
        p.w = p.h = 512;
        p.format = Ren::eTexFormat::RGBA16F;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage |
                  Ren::eTexUsage::RenderTarget;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        scene_data_.env.env_map = ren_ctx_.LoadTextureCube("Sky Envmap", _black_cube, p,
                                                           scene_data_.persistent_data.mem_allocs.get(), &status);

        for (int j = 0; j < scene_data_.env.env_map->params.mip_count; ++j) {
            for (int i = 0; i < 6; ++i) {
                const int view_index = scene_data_.env.env_map->AddImageView(Ren::eTexFormat::RGBA16F, j, 1, i, 1);
                assert(view_index <= 1 + j * 6 + i);
            }
        }
    } else if (!scene_data_.env.env_map_name.empty()) {
        Sys::AssetFile in_file(std::string(paths_.textures_path) + scene_data_.env.env_map_name.c_str());
        [[maybe_unused]] const size_t in_file_size = in_file.size();

        Ren::DDSHeader header = {};
        in_file.Read((char *)&header, sizeof(Ren::DDSHeader));

        Ren::DDS_HEADER_DXT10 dx10_header = {};
        in_file.Read((char *)&dx10_header, sizeof(Ren::DDS_HEADER_DXT10));

        const int w = int(header.dwWidth), h = int(header.dwHeight);
        assert(w == h);

        const int size_per_face = int(header.dwPitchOrLinearSize) / 6;

        std::vector<uint8_t> tex_data[6];
        Ren::Span<const uint8_t> data[6];

        for (int i = 0; i < 6; i++) {
            tex_data[i].resize(size_per_face);
            in_file.Read((char *)tex_data[i].data(), size_per_face);
            data[i] = tex_data[i];
        }

        Ren::TexParams p;
        p.w = w;
        p.h = h;
        p.mip_count = int(header.dwMipMapCount);
        p.format = Ren::eTexFormat::RGB9_E5;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus load_status;
        scene_data_.env.env_map =
            ren_ctx_.LoadTextureCube("EnvCubemap", data, p, scene_data_.persistent_data.mem_allocs.get(), &load_status);
    } else {
        static const uint8_t white_cube[6][4] = {{255, 255, 255, 128}, {255, 255, 255, 128}, {255, 255, 255, 128},
                                                 {255, 255, 255, 128}, {255, 255, 255, 128}, {255, 255, 255, 128}};

        Ren::Span<const uint8_t> _white_cube[6];
        for (int i = 0; i < 6; ++i) {
            _white_cube[i] = white_cube[i];
        }

        Ren::TexParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RGBA8;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        scene_data_.env.env_map = ren_ctx_.LoadTextureCube("dummy_white_cube", _white_cube, p,
                                                           scene_data_.persistent_data.mem_allocs.get(), &status);
    }
}

void Eng::SceneManager::ReleaseEnvMap(const bool immediate) {
    if (immediate) {
        scene_data_.env.env_map->FreeImmediate();
    }
    scene_data_.env.env_map = {};
}

void Eng::SceneManager::AllocGICache() {
    float probe_volume_spacing = 0.5f;
    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        probe_volume_t &volume = scene_data_.persistent_data.probe_volumes.emplace_back();
        volume.origin = Ren::Vec3f{0.0f};
        volume.spacing = Ren::Vec3f{probe_volume_spacing};
        probe_volume_spacing *= 3.0f;
    }

    { // ~47.8mb
        Ren::TexParams p;
        p.w = PROBE_VOLUME_RES_X * PROBE_IRRADIANCE_RES;
        p.h = PROBE_VOLUME_RES_Z * PROBE_IRRADIANCE_RES;
        p.d = 2 * PROBE_VOLUME_RES_Y * PROBE_VOLUMES_COUNT;
        p.format = Ren::eTexFormat::RGBA16F;
        p.flags = Ren::eTexFlags::Array;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Storage) | Ren::eTexUsage::Sampled | Ren::eTexUsage::Transfer;
        p.sampling.filter = Ren::eTexFilter::Bilinear;

        Ren::eTexLoadStatus status;
        scene_data_.persistent_data.probe_irradiance =
            ren_ctx_.LoadTexture("Probe Volume Irradiance", p, ren_ctx_.default_mem_allocs(), &status);
        assert(status != Ren::eTexLoadStatus::Error);
    }
    { // ~84.9mb
        Ren::TexParams p;
        p.w = PROBE_VOLUME_RES_X * PROBE_DISTANCE_RES;
        p.h = PROBE_VOLUME_RES_Z * PROBE_DISTANCE_RES;
        p.d = PROBE_VOLUME_RES_Y * PROBE_VOLUMES_COUNT;
        p.format = Ren::eTexFormat::RG16F;
        p.flags = Ren::eTexFlags::Array;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Storage) | Ren::eTexUsage::Sampled | Ren::eTexUsage::Transfer;
        p.sampling.filter = Ren::eTexFilter::Bilinear;

        Ren::eTexLoadStatus status;
        scene_data_.persistent_data.probe_distance =
            ren_ctx_.LoadTexture("Probe Volume Distance", p, ren_ctx_.default_mem_allocs(), &status);
        assert(status != Ren::eTexLoadStatus::Error);
    }
    { // ~0.7mb
        Ren::TexParams p;
        p.w = PROBE_VOLUME_RES_X;
        p.h = PROBE_VOLUME_RES_Z;
        p.d = PROBE_VOLUME_RES_Y * PROBE_VOLUMES_COUNT;
        p.format = Ren::eTexFormat::RGBA16F;
        p.flags = Ren::eTexFlags::Array;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Storage) | Ren::eTexUsage::Sampled | Ren::eTexUsage::Transfer;
        p.sampling.filter = Ren::eTexFilter::Bilinear;

        Ren::eTexLoadStatus status;
        scene_data_.persistent_data.probe_offset =
            ren_ctx_.LoadTexture("Probe Volume Offset", p, ren_ctx_.default_mem_allocs(), &status);
        assert(status != Ren::eTexLoadStatus::Error);
    }

    ClearGICache();
}

void Eng::SceneManager::ReleaseGICache(const bool immediate) {
    if (immediate) {
        scene_data_.persistent_data.probe_irradiance->FreeImmediate();
        scene_data_.persistent_data.probe_distance->FreeImmediate();
        scene_data_.persistent_data.probe_offset->FreeImmediate();
    }
    scene_data_.persistent_data.probe_irradiance = {};
    scene_data_.persistent_data.probe_distance = {};
    scene_data_.persistent_data.probe_offset = {};
    scene_data_.persistent_data.probe_volumes.clear();
}

void Eng::SceneManager::Alloc_TLAS() {
    if (ren_ctx_.capabilities.hwrt) {
        Alloc_HWRT_TLAS();
    } else {
        Alloc_SWRT_TLAS();
    }
}

void Eng::SceneManager::Release_TLAS(const bool immediate) {
    if (immediate) {
        for (auto &buf : scene_data_.persistent_data.rt_tlas_buf) {
            buf->FreeImmediate();
        }
        for (auto &tlas : scene_data_.persistent_data.rt_tlas) {
            tlas->FreeImmediate();
        }
    }
    std::fill(std::begin(scene_data_.persistent_data.rt_tlas_buf), std::end(scene_data_.persistent_data.rt_tlas_buf),
              Ren::BufRef{});
    std::fill(std::begin(scene_data_.persistent_data.rt_tlas), std::end(scene_data_.persistent_data.rt_tlas), nullptr);
}

void Eng::SceneManager::AllocMeshBuffers() {
    bool recreate_views = !*scene_data_.persistent_data.vertex_buf1;
    scene_data_.persistent_data.vertex_buf1->Resize(16 * 1024 * 1024);
    scene_data_.persistent_data.vertex_buf2->Resize(16 * 1024 * 1024);
    scene_data_.persistent_data.skin_vertex_buf->Resize(16 * 1024 * 1024);
    scene_data_.persistent_data.delta_buf->Resize(16 * 1024 * 1024);
    scene_data_.persistent_data.indices_buf->Resize(16 * 1024 * 1024);
    if (recreate_views) {
        scene_data_.persistent_data.vertex_buf1->AddBufferView(Ren::eTexFormat::RGBA32F);
        scene_data_.persistent_data.vertex_buf2->AddBufferView(Ren::eTexFormat::RGBA32UI);
        scene_data_.persistent_data.indices_buf->AddBufferView(Ren::eTexFormat::R32UI);
    }
}

void Eng::SceneManager::LoadMeshBuffers() {
    AllocMeshBuffers();

    auto *drawables = (Eng::Drawable *)scene_data_.comp_store[Eng::CompDrawable]->SequentialData();
    auto *acc_structs = (Eng::AccStructure *)scene_data_.comp_store[Eng::CompAccStructure]->SequentialData();

    for (const Eng::SceneObject &obj : scene_data_.objects) {
        if (bool(obj.comp_mask & Eng::CompDrawableBit)) {
            Eng::Drawable &dr = drawables[obj.components[Eng::CompDrawable]];
            Ren::Mesh *mesh = dr.mesh.get();
            assert(mesh->type() == Ren::eMeshType::Simple);
            mesh->InitBufferData(ren_ctx_.api_ctx(), scene_data_.persistent_data.vertex_buf1,
                                 scene_data_.persistent_data.vertex_buf2, scene_data_.persistent_data.indices_buf);
        }
        if (bool(obj.comp_mask & Eng::CompAccStructureBit)) {
            Eng::AccStructure &acc = acc_structs[obj.components[Eng::CompAccStructure]];
            Ren::Mesh *mesh = acc.mesh.get();
            assert(mesh->type() == Ren::eMeshType::Simple);
            mesh->InitBufferData(ren_ctx_.api_ctx(), scene_data_.persistent_data.vertex_buf1,
                                 scene_data_.persistent_data.vertex_buf2, scene_data_.persistent_data.indices_buf);
            if (ren_ctx_.capabilities.hwrt) {
                mesh->blas = Build_HWRT_BLAS(acc);
            } else {
                mesh->blas = Build_SWRT_BLAS(acc);
            }
        }
    }
}

void Eng::SceneManager::ReleaseMeshBuffers(const bool immediate) {
    auto *drawables = (Eng::Drawable *)scene_data_.comp_store[Eng::CompDrawable]->SequentialData();
    auto *acc_structs = (Eng::AccStructure *)scene_data_.comp_store[Eng::CompAccStructure]->SequentialData();

    for (const Eng::SceneObject &obj : scene_data_.objects) {
        if (bool(obj.comp_mask & Eng::CompDrawableBit)) {
            Eng::Drawable &dr = drawables[obj.components[Eng::CompDrawable]];
            Ren::Mesh *mesh = dr.mesh.get();
            assert(mesh->type() == Ren::eMeshType::Simple);
            mesh->ReleaseBufferData();
        }
        if (bool(obj.comp_mask & Eng::CompAccStructureBit)) {
            Eng::AccStructure &acc = acc_structs[obj.components[Eng::CompAccStructure]];
            Ren::Mesh *mesh = acc.mesh.get();
            assert(mesh->type() == Ren::eMeshType::Simple);
            mesh->ReleaseBufferData();
            if (immediate && mesh->blas) {
                mesh->blas->FreeImmediate();
            }
            mesh->blas = {};
        }
    }

    if (immediate) {
        for (Ren::BufRef &b : scene_data_.persistent_data.hwrt.rt_blas_buffers) {
            b->FreeImmediate();
        }
        if (scene_data_.persistent_data.swrt.rt_prim_indices_buf) {
            scene_data_.persistent_data.swrt.rt_prim_indices_buf->FreeImmediate();
        }
        if (scene_data_.persistent_data.swrt.rt_blas_buf) {
            scene_data_.persistent_data.swrt.rt_blas_buf->FreeImmediate();
        }
        scene_data_.persistent_data.vertex_buf1->FreeImmediate();
        scene_data_.persistent_data.vertex_buf2->FreeImmediate();
        scene_data_.persistent_data.indices_buf->FreeImmediate();
        scene_data_.persistent_data.skin_vertex_buf->FreeImmediate();
        scene_data_.persistent_data.delta_buf->FreeImmediate();
    }
    scene_data_.persistent_data.hwrt = {};
    scene_data_.persistent_data.swrt = {};

    assert(scene_data_.persistent_data.vertex_buf1.strong_refs() == 1);
    assert(scene_data_.persistent_data.vertex_buf2.strong_refs() == 1);
    assert(scene_data_.persistent_data.indices_buf.strong_refs() == 1);

    // scene_data_.persistent_data.vertex_buf1 = scene_data_.persistent_data.vertex_buf2 =
    //     scene_data_.persistent_data.skin_vertex_buf = scene_data_.persistent_data.delta_buf =
    //         scene_data_.persistent_data.indices_buf = {};
}

void Eng::SceneManager::AllocInstanceBuffer() {
    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();
    scene_data_.persistent_data.instance_buf = scene_data_.buffers.Insert(
        "Instance Buf", api_ctx, Ren::eBufType::Texture, uint32_t(sizeof(instance_data_t) * MAX_INSTANCES_TOTAL));
    scene_data_.persistent_data.instance_buf->AddBufferView(Ren::eTexFormat::RGBA32F);
    for (uint32_t i = 0; i < scene_data_.objects.size(); ++i) {
        instance_data_to_update_.push_back(i);
    }
}

void Eng::SceneManager::ReleaseInstanceBuffer(const bool immediate) {
    if (immediate) {
        scene_data_.persistent_data.instance_buf->FreeImmediate();
    }
    scene_data_.persistent_data.instance_buf = {};
}

void Eng::SceneManager::AllocMaterialsBuffer() {
    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();
    scene_data_.persistent_data.materials_buf = scene_data_.buffers.Insert(
        "Materials Buffer", api_ctx, Ren::eBufType::Storage, uint32_t(8 * sizeof(material_data_t)));
    for (auto it = scene_data_.materials.begin(); it != scene_data_.materials.end(); ++it) {
        scene_data_.material_changes.push_back(it.index());
    }
}

void Eng::SceneManager::ReleaseMaterialsBuffer(const bool immediate) {
    if (immediate) {
        scene_data_.persistent_data.materials_buf->FreeImmediate();
    }
    scene_data_.persistent_data.materials_buf = {};
}

void Eng::SceneManager::LoadProbeCache() {
#if 0
    const int res = scene_data_.probe_storage.res(), capacity = scene_data_.probe_storage.capacity();

    if (scene_data_.probe_storage.format() != Ren::DefaultCompressedRGBA) {
        // init in compressed texture format
        scene_data_.probe_storage.Resize(ren_ctx_.api_ctx(), ren_ctx_.default_mem_allocs(), Ren::DefaultCompressedRGBA,
                                         res, capacity, ren_ctx_.log());
    }

    CompStorage *probe_storage = scene_data_.comp_store[CompProbe];

    uint32_t probe_id = probe_storage->First();
    while (probe_id != 0xffffffff) {
        auto *lprobe = (LightProbe *)probe_storage->Get(probe_id);
        assert(lprobe);

        std::string file_path;

        for (int face_index = 0; face_index < 6; face_index++) {
            file_path.clear();

#if !defined(__ANDROID__)
            file_path += "assets_pc/textures/probes_cache/";
#else
            file_path += "assets/textures/probes_cache/";
#endif
            file_path += scene_data_.name.c_str();
            file_path += std::to_string(lprobe->layer_index);
            file_path += "_";
            file_path += std::to_string(face_index);
#if !defined(__ANDROID__)
            file_path += ".dds";
#else
            file_path += ".ktx";
#endif

            Sys::AssetFile in_file(file_path);
            if (in_file) {
                size_t in_file_size = in_file.size();

                std::vector<uint8_t> file_data(in_file_size);
                in_file.Read((char *)&file_data[0], in_file_size);

                Ren::ILog *log = ren_ctx_.log();

                const int res = scene_data_.probe_storage.res();
                CompStorage *probe_storage = scene_data_.comp_store[CompProbe];

                auto *lprobe = (LightProbe *)probe_storage->Get(probe_id);
                assert(lprobe);

#if !defined(__ANDROID__)
                const uint8_t *p_data = file_data.data() + sizeof(Ren::DDSHeader);
                int data_len = int(in_file_size - sizeof(Ren::DDSHeader));

                int _res = res;
                int level = 0;

                while (_res >= 16) {
                    const int len = ((_res + 3) / 4) * ((_res + 3) / 4) * 16;

                    if (len > data_len || !scene_data_.probe_storage.SetPixelData(
                                              level, lprobe->layer_index, face_index, Ren::DefaultCompressedRGBA,
                                              p_data, len, ren_ctx_.log())) {
                        log->Error("Failed to load probe texture!");
                    }

                    p_data += len;
                    data_len -= len;

                    _res /= 2;
                    level++;
                }
#else
                const uint8_t *p_data = file_data.data();
                int data_offset = sizeof(Ren::KTXHeader);
                int data_len = in_file_size - int(sizeof(Ren::KTXHeader));

                int _res = res;
                int level = 0;

                while (_res >= 16) {
                    uint32_t len;
                    memcpy(&len, &p_data[data_offset], sizeof(uint32_t));
                    data_offset += sizeof(uint32_t);
                    data_len -= sizeof(uint32_t);

                    if (int(len) > data_len || !self->scene_data_.probe_storage.SetPixelData(
                                                   level, lprobe->layer_index, face_index, Ren::eTexFormat::Compressed,
                                                   &p_data[data_offset], len, self->ren_ctx_.log())) {
                        log->Error("Failed to load probe texture!");
                    }

                    data_offset += len;
                    data_len -= len;

                    const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
                    data_offset += pad;

                    _res = _res / 2;
                    level++;
                }
#endif
            }
        }

        probe_id = probe_storage->Next(probe_id);
    }

    scene_data_.probe_storage.Finalize();
#endif
}

void Eng::SceneManager::SetupView(const Ren::Vec3d &origin, const Ren::Vec3d &target, const Ren::Vec3f &up,
                                  const float fov, const Ren::Vec2f sensor_shift, const float gamma,
                                  const float min_exposure, const float max_exposure) {
    using namespace SceneManagerConstants;

    const int cur_scr_w = ren_ctx_.w(), cur_scr_h = ren_ctx_.h();
    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    int origin_dist = int(std::abs(origin[0] - scene_data_.origin[0]));
    origin_dist = std::max(origin_dist, int(std::abs(origin[1] - scene_data_.origin[1])));
    origin_dist = std::max(origin_dist, int(std::abs(origin[2] - scene_data_.origin[2])));

    if (origin_dist > SCROLLING_DISTANCE) {
        const Ren::Vec3i cam_sector = Ren::Vec3i(origin / double(SCROLLING_DISTANCE / 2));
        const Ren::Vec3d new_origin = Ren::Vec3d(cam_sector) * double(SCROLLING_DISTANCE / 2);

        UpdateWorldScrolling(new_origin);
    }

    const Ren::Vec3f rel_origin = Ren::Vec3f(origin - scene_data_.origin);
    const Ren::Vec3f rel_target = Ren::Vec3f(target - scene_data_.origin);

    cam_.SetupView(rel_origin, rel_target, up);
    cam_.Perspective(Ren::eZRange::OneToZero, fov, float(cur_scr_w) / float(cur_scr_h), NEAR_CLIP, FAR_CLIP,
                     sensor_shift);
    cam_.UpdatePlanes();

    cam_.set_render_mask(Ren::Bitmask<Drawable::eVisibility>{Drawable::eVisibility::Camera});

    static const float ExtendedFrustumOffset = 100.0f;
    static const float ExtendedFrustumFrontOffset = 200.0f;

    ext_cam_.SetupView(rel_origin - ExtendedFrustumOffset * cam_.fwd(), rel_origin, cam_.up());
    ext_cam_.Perspective(Ren::eZRange::OneToZero, cam_.angle(), cam_.aspect(), 1.0f,
                         ExtendedFrustumOffset + ExtendedFrustumFrontOffset);
    ext_cam_.UpdatePlanes();

    cam_.gamma = gamma;
    cam_.min_exposure = min_exposure;
    cam_.max_exposure = max_exposure;

    const double cur_time_s = Sys::GetTimeS();
    const Ren::Vec3f velocity = Ren::Vec3f(origin - last_cam_pos_) / float(cur_time_s - last_cam_time_s_);
    last_cam_pos_ = origin;
    last_cam_time_s_ = cur_time_s;

    if (snd_ctx_) {
        const Ren::Vec3f fwd_up[2] = {cam_.fwd(), cam_.up()};
        snd_ctx_->SetupListener(ValuePtr(rel_origin), ValuePtr(velocity), ValuePtr(fwd_up[0]));
    }
}

void Eng::SceneManager::PostloadDrawable(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    using namespace SceneManagerConstants;

    auto *dr = (Drawable *)comp;

    if (js_comp_obj.Has("mesh_file")) {
        const Sys::JsStringP &js_mesh_file_name = js_comp_obj.at("mesh_file").as_str();

        Ren::eMeshLoadStatus status;
        dr->mesh = LoadMesh(js_mesh_file_name.val, nullptr, nullptr, &status);

        if (status != Ren::eMeshLoadStatus::Found) {
            const std::string mesh_path = std::string(paths_.models_path) + js_mesh_file_name.val.c_str();

#if defined(__ANDROID__)
            Sys::AssetFile in_file(mesh_path.c_str());
            if (!in_file) {
                ren_ctx_.log()->Error("Failed to open %s", mesh_path.c_str());
                return;
            }
            size_t in_file_size = in_file.size();

            std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
            in_file.Read((char *)&in_file_data[0], in_file_size);

            Sys::MemBuf mem = {&in_file_data[0], in_file_size};
            std::istream in_file_stream(&mem);
#else
            std::ifstream in_file_stream(mesh_path.c_str(), std::ios::binary);
#endif

            using namespace std::placeholders;
            dr->mesh = LoadMesh(js_mesh_file_name.val, &in_file_stream,
                                std::bind(&SceneManager::OnLoadMaterial, this, _1), &status);
            assert(status == Ren::eMeshLoadStatus::CreatedFromData);
        }
    } else {
        assert(false && "Not supported anymore, update scene file!");
    }

    if (js_comp_obj.Has("material_override")) {
        const Sys::JsArrayP &js_materials = js_comp_obj.at("material_override").as_arr();
        for (const Sys::JsElementP &js_mat_el : js_materials.elements) {
            auto front_back_mats = OnLoadMaterial(js_mat_el.as_str().val);
            dr->material_override.push_back(std::move(front_back_mats));
        }
    }

    if (js_comp_obj.Has("anims")) {
        const Sys::JsArrayP &js_anims = js_comp_obj.at("anims").as_arr();

        assert(dr->mesh->type() == Ren::eMeshType::Skeletal);
        Ren::Skeleton *skel = dr->mesh->skel();

        for (const auto &js_anim : js_anims.elements) {
            const Sys::JsStringP &js_anim_name = js_anim.as_str();
            const std::string anim_path = std::string(paths_.models_path) + js_anim_name.val.c_str();

            Sys::AssetFile in_file(anim_path.c_str());
            size_t in_file_size = in_file.size();

            std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
            in_file.Read((char *)&in_file_data[0], in_file_size);

            Sys::MemBuf mem = {&in_file_data[0], in_file_size};
            std::istream in_file_stream(&mem);

            Ren::AnimSeqRef anim_ref = ren_ctx_.LoadAnimSequence(js_anim_name.val, in_file_stream);
            skel->AddAnimSequence(anim_ref);
        }
    }

    /*if (dr->mesh->type() == Ren::eMeshType::Skeletal) {
        const Ren::Skeleton *skel = dr->mesh->skel();

        // Attach ellipsoids to bones
        for (int i = 0; i < dr->ellipsoids_count; i++) {
            Drawable::Ellipsoid &e = dr->ellipsoids[i];
            if (e.bone_name.empty()) {
                e.bone_index = -1;
                continue;
            }

            for (int j = 0; j < skel->bones_count; j++) {
                if (e.bone_name == skel->bones[j].name) {
                    e.bone_index = j;
                    break;
                }
            }
        }
    }*/

    obj_bbox[0] = Min(obj_bbox[0], dr->mesh->bbox_min());
    obj_bbox[1] = Max(obj_bbox[1], dr->mesh->bbox_max());
}

void Eng::SceneManager::PostloadOccluder(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    using namespace SceneManagerConstants;

    auto *occ = (Occluder *)comp;

    const Sys::JsStringP &js_mesh_file_name = js_comp_obj.at("mesh_file").as_str();

    Ren::eMeshLoadStatus status;
    occ->mesh = LoadMesh(js_mesh_file_name.val, nullptr, nullptr, &status);

    if (status != Ren::eMeshLoadStatus::Found) {
        const std::string mesh_path = std::string(paths_.models_path) + js_mesh_file_name.val.c_str();

        Sys::AssetFile in_file(mesh_path);
        size_t in_file_size = in_file.size();

        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
        in_file.Read((char *)&in_file_data[0], in_file_size);

        Sys::MemBuf mem = {&in_file_data[0], in_file_size};
        std::istream in_file_stream(&mem);

        using namespace std::placeholders;
        occ->mesh = LoadMesh(js_mesh_file_name.val, &in_file_stream, std::bind(&SceneManager::OnLoadMaterial, this, _1),
                             &status);
        assert(status == Ren::eMeshLoadStatus::CreatedFromData);
    }

    obj_bbox[0] = Min(obj_bbox[0], occ->mesh->bbox_min());
    obj_bbox[1] = Max(obj_bbox[1], occ->mesh->bbox_max());
}

void Eng::SceneManager::PostloadLightmap(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    using namespace SceneManagerConstants;

    auto *lm = (Lightmap *)comp;

    const int node_id = scene_data_.lm_splitter.Allocate(lm->size, lm->pos);
    if (node_id == -1) {
        throw std::runtime_error("Cannot allocate lightmap region!");
    }

    lm->xform = Ren::Vec4f{
        float(lm->pos[0]) / LIGHTMAP_ATLAS_RESX,
        float(lm->pos[1]) / LIGHTMAP_ATLAS_RESY,
        float(lm->size[0]) / LIGHTMAP_ATLAS_RESX,
        float(lm->size[1]) / LIGHTMAP_ATLAS_RESY,
    };
}

void Eng::SceneManager::PostloadLightSource(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    auto *ls = (LightSource *)comp;

    // Compute bounding box of light source
    [[maybe_unused]] const auto pos = Ren::Vec4f{ls->offset[0], ls->offset[1], ls->offset[2], 1.0f},
                                dir = Ren::Vec4f{ls->dir[0], ls->dir[1], ls->dir[2], 0.0f};

    Ren::Vec3f bbox_min, bbox_max;

    const auto _dir = Ren::Vec3f{dir[0], dir[1], dir[2]};
    const Ren::Vec3f p1 = _dir * ls->cull_radius;

    bbox_min = Min(bbox_min, p1);
    bbox_max = Max(bbox_max, p1);

    const Ren::Vec3f p2 = _dir * ls->spot_cos * ls->cull_radius;

    const float d = std::sqrt(1.0f - ls->spot_cos * ls->spot_cos) * ls->cull_radius;

    bbox_min = Min(bbox_min, p2 - Ren::Vec3f{d, 0.0f, d});
    bbox_max = Max(bbox_max, p2 + Ren::Vec3f{d, 0.0f, d});

    if (ls->spot_cos < 0.0f) {
        bbox_min = Min(bbox_min, p1 - Ren::Vec3f{ls->cull_radius, 0.0f, ls->cull_radius});
        bbox_max = Max(bbox_max, p1 + Ren::Vec3f{ls->cull_radius, 0.0f, ls->cull_radius});
    }

    auto up = Ren::Vec3f{1.0f, 0.0f, 0.0f};
    if (std::abs(_dir[1]) < std::abs(_dir[2]) && std::abs(_dir[1]) < std::abs(_dir[0])) {
        up = Ren::Vec3f{0.0f, 1.0f, 0.0f};
    } else if (std::abs(_dir[2]) < std::abs(_dir[0]) && std::abs(_dir[2]) < std::abs(_dir[1])) {
        up = Ren::Vec3f{0.0f, 0.0f, 1.0f};
    }

    const Ren::Vec3f side = Cross(_dir, up);

    Transform ls_transform;
    ls_transform.world_from_object = Ren::Mat4f{
        Ren::Vec4f{side[0], -_dir[0], up[0], 0.0f}, Ren::Vec4f{side[1], -_dir[1], up[1], 0.0f},
        Ren::Vec4f{side[2], -_dir[2], up[2], 0.0f}, Ren::Vec4f{ls->offset[0], ls->offset[1], ls->offset[2], 1.0f}};

    ls_transform.bbox_min = bbox_min;
    ls_transform.bbox_max = bbox_max;
    ls_transform.UpdateBBox();

    // Combine light's bounding box with object's
    obj_bbox[0] = Min(obj_bbox[0], ls_transform.bbox_min_ws);
    obj_bbox[1] = Max(obj_bbox[1], ls_transform.bbox_max_ws);
}

void Eng::SceneManager::PostloadDecal(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    auto *de = (Decal *)comp;

    if (js_comp_obj.Has("mask")) {
        const Sys::JsStringP &js_mask = js_comp_obj.at("mask").as_str();

        const Ren::Vec4f *mask_tr = scene_data_.decals_textures.Find(js_mask.val.c_str());
        if (!mask_tr) {
            de->mask = LoadDecalTexture(js_mask.val);
            scene_data_.decals_textures.Insert(Ren::String{js_mask.val.c_str()}, de->mask);
        } else {
            de->mask = *mask_tr;
        }
    }

    if (js_comp_obj.Has("diff")) {
        const Sys::JsStringP &js_diff = js_comp_obj.at("diff").as_str();

        const Ren::Vec4f *diff_tr = scene_data_.decals_textures.Find(js_diff.val.c_str());
        if (!diff_tr) {
            de->diff = LoadDecalTexture(js_diff.val);
            scene_data_.decals_textures.Insert(Ren::String{js_diff.val.c_str()}, de->diff);
        } else {
            de->diff = *diff_tr;
        }
    }

    if (js_comp_obj.Has("norm")) {
        const Sys::JsStringP &js_norm = js_comp_obj.at("norm").as_str();

        const Ren::Vec4f *norm_tr = scene_data_.decals_textures.Find(js_norm.val.c_str());
        if (!norm_tr) {
            de->norm = LoadDecalTexture(js_norm.val);
            scene_data_.decals_textures.Insert(Ren::String{js_norm.val.c_str()}, de->norm);
        } else {
            de->norm = *norm_tr;
        }
    }

    if (js_comp_obj.Has("spec")) {
        const Sys::JsStringP &js_spec = js_comp_obj.at("spec").as_str();

        const Ren::Vec4f *spec_tr = scene_data_.decals_textures.Find(js_spec.val.c_str());
        if (!spec_tr) {
            de->spec = LoadDecalTexture(js_spec.val);
            scene_data_.decals_textures.Insert(Ren::String{js_spec.val.c_str()}, de->spec);
        } else {
            de->spec = *spec_tr;
        }
    }

    const Ren::Mat4f world_from_clip = Inverse(de->proj * de->view);

    Ren::Vec4f points[] = {Ren::Vec4f{-1.0f, -1.0f, -1.0f, 1.0f}, Ren::Vec4f{-1.0f, 1.0f, -1.0f, 1.0f},
                           Ren::Vec4f{1.0f, 1.0f, -1.0f, 1.0f},   Ren::Vec4f{1.0f, -1.0f, -1.0f, 1.0f},

                           Ren::Vec4f{-1.0f, -1.0f, 1.0f, 1.0f},  Ren::Vec4f{-1.0f, 1.0f, 1.0f, 1.0f},
                           Ren::Vec4f{1.0f, 1.0f, 1.0f, 1.0f},    Ren::Vec4f{1.0f, -1.0f, 1.0f, 1.0f}};

    for (Ren::Vec4f &point : points) {
        point = world_from_clip * point;
        point /= point[3];

        // Combine decals's bounding box with object's
        obj_bbox[0] = Min(obj_bbox[0], Ren::Vec3f{point});
        obj_bbox[1] = Max(obj_bbox[1], Ren::Vec3f{point});
    }
}

void Eng::SceneManager::PostloadLightProbe(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    auto *pr = (LightProbe *)comp;

    // pr->layer_index = scene_data_.probe_storage.Allocate();

    // Combine probe's bounding box with object's
    obj_bbox[0] = Min(obj_bbox[0], pr->offset - Ren::Vec3f{pr->radius});
    obj_bbox[1] = Max(obj_bbox[1], pr->offset + Ren::Vec3f{pr->radius});
}

void Eng::SceneManager::PostloadSoundSource(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    auto *snd = (SoundSource *)comp;

    const Ren::Vec3f center = 0.5f * (obj_bbox[0] + obj_bbox[1]);
    snd->snd_src.Init(1.0f, ValuePtr(center));
}

void Eng::SceneManager::PostloadAccStructure(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]) {
    using namespace SceneManagerConstants;

    auto *acc = (AccStructure *)comp;

    const Sys::JsStringP &js_mesh_file_name = js_comp_obj.at("mesh_file").as_str();

    Ren::eMeshLoadStatus status;
    acc->mesh = LoadMesh(js_mesh_file_name.val, nullptr, nullptr, &status);

    if (status != Ren::eMeshLoadStatus::Found) {
        const std::string mesh_path = std::string(paths_.models_path) + js_mesh_file_name.val.c_str();

        Sys::AssetFile in_file(mesh_path);
        size_t in_file_size = in_file.size();

        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
        in_file.Read((char *)&in_file_data[0], in_file_size);

        Sys::MemBuf mem = {&in_file_data[0], in_file_size};
        std::istream in_file_stream(&mem);

        using namespace std::placeholders;
        acc->mesh = LoadMesh(js_mesh_file_name.val, &in_file_stream, std::bind(&SceneManager::OnLoadMaterial, this, _1),
                             &status);
        assert(status == Ren::eMeshLoadStatus::CreatedFromData);
    }

    if (js_comp_obj.Has("material_override")) {
        const Sys::JsArrayP &js_materials = js_comp_obj.at("material_override").as_arr();
        for (const Sys::JsElementP &js_mat_el : js_materials.elements) {
            auto front_back_mats = OnLoadMaterial(js_mat_el.as_str().val);
            acc->material_override.push_back(std::move(front_back_mats));
        }
    }

    // TODO: use better surface area estimation
    const Ren::Vec3f e = acc->mesh->bbox_max() - acc->mesh->bbox_min();
    acc->surf_area = 2.0f * (e[0] + e[1] + e[2]);

    obj_bbox[0] = Min(obj_bbox[0], acc->mesh->bbox_min());
    obj_bbox[1] = Max(obj_bbox[1], acc->mesh->bbox_max());

    if (!acc->mesh->blas) {
        if (ren_ctx_.capabilities.hwrt) {
            acc->mesh->blas = Build_HWRT_BLAS(*acc);
        } else {
            acc->mesh->blas = Build_SWRT_BLAS(*acc);
        }
    }
}

std::array<Ren::MaterialRef, 3> Eng::SceneManager::OnLoadMaterial(std::string_view name) {
    using namespace SceneManagerConstants;

    std::array<Ren::MaterialRef, 3> ret;

    const std::string backside_name = std::string(name) + "@back";
    const std::string volumetric_name = std::string(name) + "@vol";

    /*Ren::eMatLoadStatus status;
    ret[0] = LoadMaterial(name, {}, &status, nullptr, nullptr, nullptr);
    if (ret[0]->ready()) {
        ret[1] = scene_data_.materials.FindByName(backside_name);
        if (!ret[1]) {
            ret[1] = ret[0];
        }
    }*/

    ret[0] = scene_data_.materials.FindByName(name);
    ret[1] = scene_data_.materials.FindByName(backside_name);
    if (!ret[1]) {
        ret[1] = ret[0];
    }
    ret[2] = scene_data_.materials.FindByName(volumetric_name);

    if (!ret[0] && !ret[1] && !ret[2]) {
        Sys::AssetFile in_file(std::string(paths_.materials_path).append(name));
        if (!in_file) {
            ren_ctx_.log()->Error("Error loading material %s", name.data());
            return ret;
        }

        const size_t file_size = in_file.size();

        std::string mat_src;
        mat_src.resize(file_size);
        in_file.Read((char *)mat_src.data(), file_size);

        using namespace std::placeholders;

        Ren::eMatLoadStatus status;
        if (mat_src[0] == '-' && mat_src[1] == '-' && mat_src[2] == '-') {
            status = Ren::eMatLoadStatus::CreatedFromData_NeedsMore;
        } else {
            ret[0] = ret[1] = LoadMaterial(name, mat_src, &status,
                                           std::bind(&SceneManager::OnLoadPipelines, this, _1, _2, _3, _4, _5, _6),
                                           std::bind(&SceneManager::OnLoadTexture, this, _1, _2, _3),
                                           std::bind(&SceneManager::OnLoadSampler, this, _1));
        }
        if (status == Ren::eMatLoadStatus::CreatedFromData_NeedsMore) {
            const size_t n1 = mat_src.find("---");
            mat_src = mat_src.substr(n1 + 4);
            if (mat_src[0] == '-' && mat_src[1] == '-' && mat_src[2] == '-') {
                status = Ren::eMatLoadStatus::CreatedFromData_NeedsMore;
            } else {
                ret[1] = LoadMaterial(backside_name, mat_src, &status,
                                      std::bind(&SceneManager::OnLoadPipelines, this, _1, _2, _3, _4, _5, _6),
                                      std::bind(&SceneManager::OnLoadTexture, this, _1, _2, _3),
                                      std::bind(&SceneManager::OnLoadSampler, this, _1));
            }
            if (status == Ren::eMatLoadStatus::CreatedFromData_NeedsMore) {
                const size_t n2 = mat_src.find("---");
                mat_src = mat_src.substr(n2 + 4);
                ret[2] = LoadMaterial(volumetric_name, mat_src, &status,
                                      std::bind(&SceneManager::OnLoadPipelines, this, _1, _2, _3, _4, _5, _6),
                                      std::bind(&SceneManager::OnLoadTexture, this, _1, _2, _3),
                                      std::bind(&SceneManager::OnLoadSampler, this, _1));
            }
        }
        assert(status == Ren::eMatLoadStatus::CreatedFromData);
    }
    scene_data_.material_changes.push_back(ret[0].index());
    if (ret[1] != ret[0]) {
        scene_data_.material_changes.push_back(ret[1].index());
    }
    if (ret[2]) {
        scene_data_.material_changes.push_back(ret[2].index());
    }
    return ret;
}

void Eng::SceneManager::OnLoadPipelines(Ren::Bitmask<Ren::eMatFlags> flags, std::string_view v_shader,
                                        std::string_view f_shader, std::string_view tc_shader,
                                        std::string_view te_shader,
                                        Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) {
    using namespace SceneManagerConstants;

    const Ren::ProgramRef ret = sh_.LoadProgram(v_shader, f_shader, tc_shader, te_shader);
    init_pipelines_(ret, flags, scene_data_.persistent_data.pipelines, out_pipelines);
}

Ren::TexRef Eng::SceneManager::OnLoadTexture(const std::string_view name, const uint8_t color[4],
                                             const Ren::Bitmask<Ren::eTexFlags> flags) {
    using namespace SceneManagerConstants;

    Ren::TexParams p;
    p.w = p.h = 1;
    p.format = Ren::eTexFormat::RGBA8;
    p.sampling.filter = Ren::eTexFilter::Trilinear;
    p.sampling.wrap = Ren::eTexWrap::Repeat;
    p.flags = Ren::eTexFlags::Stub;
    p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
    p.sampling.lod_bias.from_float(-1.0f); // TAA compensation

    Ren::eTexLoadStatus status;
    Ren::TexRef ret = LoadTexture(name, Ren::Span{color, color + 4}, p, &status);

    if (status == Ren::eTexLoadStatus::CreatedFromData) {
        TextureRequest new_req;
        new_req.ref = ret;

        if (ret->name().StartsWith("lightmaps/")) {
            // set max initial priority for lightmaps
            new_req.sort_key = 0;
        }

        std::lock_guard<std::mutex> _(tex_requests_lock_);
        requested_textures_.push_back(std::move(new_req));
        tex_loader_cnd_.notify_one();
    }

    return ret;
}

Ren::SamplerRef Eng::SceneManager::OnLoadSampler(Ren::SamplingParams params) {
    Ren::eSamplerLoadStatus status;
    return ren_ctx_.LoadSampler(params, &status);
}

Ren::MeshRef Eng::SceneManager::LoadMesh(std::string_view name, std::istream *data,
                                         const Ren::material_load_callback &on_mat_load,
                                         Ren::eMeshLoadStatus *load_status) {
    Ren::MeshRef ref = scene_data_.meshes.FindByName(name);
    if (!ref) {
        ref = scene_data_.meshes.Insert(
            name, data, on_mat_load, ren_ctx_.api_ctx(), scene_data_.persistent_data.vertex_buf1,
            scene_data_.persistent_data.vertex_buf2, scene_data_.persistent_data.indices_buf,
            scene_data_.persistent_data.skin_vertex_buf, scene_data_.persistent_data.delta_buf, load_status,
            ren_ctx_.log());
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = Ren::eMeshLoadStatus::Found;
            }
        } else if (data) {
            ref->Init(data, on_mat_load, ren_ctx_.api_ctx(), scene_data_.persistent_data.vertex_buf1,
                      scene_data_.persistent_data.vertex_buf2, scene_data_.persistent_data.indices_buf,
                      scene_data_.persistent_data.skin_vertex_buf, scene_data_.persistent_data.delta_buf, load_status,
                      ren_ctx_.log());
        }
    }

    return ref;
}

Ren::MaterialRef Eng::SceneManager::LoadMaterial(std::string_view name, std::string_view mat_src,
                                                 Ren::eMatLoadStatus *status,
                                                 const Ren::pipelines_load_callback &on_pipes_load,
                                                 const Ren::texture_load_callback &on_tex_load,
                                                 const Ren::sampler_load_callback &on_sampler_load) {
    Ren::MaterialRef ref = scene_data_.materials.FindByName(name);
    if (!ref) {
        ref = scene_data_.materials.Insert(name, mat_src, status, on_pipes_load, on_tex_load, on_sampler_load,
                                           ren_ctx_.log());
    } else {
        if (ref->ready()) {
            if (status) {
                (*status) = Ren::eMatLoadStatus::Found;
            }
        } else if (!ref->ready() && !mat_src.empty()) {
            ref->Init(mat_src, status, on_pipes_load, on_tex_load, on_sampler_load, ren_ctx_.log());
        }
    }
    return ref;
}

Ren::TexRef Eng::SceneManager::LoadTexture(std::string_view name, Ren::Span<const uint8_t> data,
                                           const Ren::TexParams &p, Ren::eTexLoadStatus *load_status) {
    Ren::TexRef ref = scene_data_.textures.FindByName(name);
    if (!ref) {
        ref = scene_data_.textures.Insert(name, ren_ctx_.api_ctx(), data, p,
                                          scene_data_.persistent_data.mem_allocs.get(), load_status, ren_ctx_.log());
    } else {
        if (load_status) {
            (*load_status) = Ren::eTexLoadStatus::Found;
        }
        if ((Ren::Bitmask<Ren::eTexFlags>{ref->params.flags} & Ren::eTexFlags::Stub) &&
            !(p.flags & Ren::eTexFlags::Stub) && !data.empty()) {
            ref->Init(data, p, scene_data_.persistent_data.mem_allocs.get(), load_status, ren_ctx_.log());
        }
    }

    return ref;
}

Ren::Vec4f Eng::SceneManager::LoadDecalTexture(std::string_view name) {
    using namespace SceneManagerConstants;

    const std::string file_name = paths_.textures_path + std::string(name);

    Sys::AssetFile in_file(file_name);
    size_t in_file_size = in_file.size();

    std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
    in_file.Read((char *)&in_file_data[0], in_file_size);

    int res[2];
#if !defined(__ANDROID__)
    Ren::DDSHeader header = {};
    memcpy(&header, in_file_data.get(), sizeof(Ren::DDSHeader));

    const int px_format = int(header.sPixelFormat.dwFourCC >> 24u) - '0';
    assert(px_format == 5);

    res[0] = int(header.dwWidth);
    res[1] = int(header.dwHeight);

    const uint8_t *p_data = in_file_data.get() + sizeof(Ren::DDSHeader);
    int data_len = int(in_file_size) - int(sizeof(Ren::DDSHeader));

    auto stage_buf = Ren::Buffer{"Temp Stage Buf", ren_ctx_.api_ctx(), Ren::eBufType::Upload, uint32_t(data_len)};
    { // Initialize stage buffer
        uint8_t *stage_data = stage_buf.Map();
        if (!stage_data) {
            ren_ctx_.log()->Error("Failed to map buffer!");
            return Ren::Vec4f{};
        }
        memcpy(stage_data, p_data, data_len);
        stage_buf.Unmap();
    }

    int pos[2];
    const int rc = scene_data_.decals_atlas.AllocateRegion(res, pos);
    if (rc == -1) {
        ren_ctx_.log()->Error("Failed to allocate decal texture!");
        return Ren::Vec4f{};
    }

    int _pos[2] = {pos[0], pos[1]};
    int _res[2] = {res[0], res[1]};
    int level = 0;

    Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();

    int data_off = 0;
    while (_res[0] >= 16 && _res[1] >= 16) {
        const int len = ((_res[0] + 3) / 4) * ((_res[1] + 3) / 4) * 16;

        if (len > data_len) {
            ren_ctx_.log()->Error("Invalid data count!");
            break;
        }

        scene_data_.decals_atlas.InitRegion(stage_buf, data_off, len, cmd_buf, Ren::DefaultCompressedRGBA, {}, 0, level,
                                            _pos, _res, ren_ctx_.log());

        data_off += len;
        data_len -= len;

        _pos[0] = _pos[0] / 2;
        _pos[1] = _pos[1] / 2;
        _res[0] = _res[0] / 2;
        _res[1] = _res[1] / 2;
        level++;
    }

    ren_ctx_.EndTempSingleTimeCommands(cmd_buf);
    stage_buf.FreeImmediate();
#else
    Ren::KTXHeader header;
    memcpy(&header, in_file_data.get(), sizeof(Ren::KTXHeader));

    assert(header.gl_internal_format == 0x93B0 /* GL_COMPRESSED_RGBA_ASTC_4x4_KHR */);

    res[0] = int(header.pixel_width);
    res[1] = int(header.pixel_height);

    int pos[2];
    const int rc = scene_data_.decals_atlas.AllocateRegion(res, pos);
    if (rc == -1) {
        ren_ctx_.log()->Error("Failed to allocate decal texture!");
        return Ren::Vec4f{};
    }

    const uint8_t *p_data = (uint8_t *)in_file_data.get();
    int data_offset = sizeof(Ren::KTXHeader);
    int data_len = int(in_file_size) - int(sizeof(Ren::KTXHeader));

    int _pos[2] = {pos[0], pos[1]};
    int _res[2] = {res[0], res[0]};
    int level = 0;

    while (_res[0] >= 16 && _res[1] >= 16) {
        uint32_t len;
        memcpy(&len, &p_data[data_offset], sizeof(uint32_t));
        data_offset += sizeof(uint32_t);
        data_len -= sizeof(uint32_t);

        if (int(len) > data_len) {
            ren_ctx_.log()->Error("Invalid data count!");
            break;
        }

        scene_data_.decals_atlas.InitRegion(p_data, len, Ren::eTexFormat::Compressed, 0, 0, level, _pos, _res,
                                            ren_ctx_.log());

        data_offset += len;
        data_len -= len;

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;

        _pos[0] = _pos[0] / 2;
        _pos[1] = _pos[1] / 2;
        _res[0] = _res[0] / 2;
        _res[1] = _res[1] / 2;
        level++;
    }
#endif

    return Ren::Vec4f{float(pos[0]) / DECALS_ATLAS_RESX, float(pos[1]) / DECALS_ATLAS_RESY,
                      float(res[0]) / DECALS_ATLAS_RESX, float(res[1]) / DECALS_ATLAS_RESY};
}

bool Eng::SceneManager::Serve(const int texture_budget) {
    using namespace SceneManagerConstants;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_serve_str);

    scene_data_.decals_atlas.Finalize(ren_ctx_.current_cmd_buf());

    EstimateTextureMemory(texture_budget);
    bool finished = true;

    if (scene_data_.load_flags & eSceneLoadFlags::Textures) {
        finished &= ProcessPendingTextures(texture_budget);
    }
    if (scene_data_.persistent_data.materials_buf) {
        finished &= UpdateMaterialsBuffer();
    }
    if (scene_data_.persistent_data.instance_buf) {
        finished &= UpdateInstanceBuffer();
    }

    __itt_task_end(__g_itt_domain);

    return finished;
}

bool Eng::SceneManager::UpdateInstanceBuffer() {
    if (instance_data_to_update_.empty()) {
        return true;
    }

    sort(begin(instance_data_to_update_), end(instance_data_to_update_));
    instance_data_to_update_.erase(unique(begin(instance_data_to_update_), end(instance_data_to_update_)),
                                   end(instance_data_to_update_));

    if (instance_data_to_update_.size() > 1) {
        uint32_t range_start = instance_data_to_update_[0];
        for (int i = 1; i < int(instance_data_to_update_.size()); ++i) {
            if (instance_data_to_update_[i] != instance_data_to_update_[i - 1] + 1) {
                UpdateInstanceBufferRange(range_start, instance_data_to_update_[i - 1]);
                range_start = instance_data_to_update_[i];
            }
        }
        UpdateInstanceBufferRange(range_start, instance_data_to_update_[instance_data_to_update_.size() - 1]);
    } else {
        UpdateInstanceBufferRange(instance_data_to_update_[0], instance_data_to_update_[0]);
    }

    instance_data_to_update_.clear();

    return false;
}

void Eng::SceneManager::UpdateInstanceBufferRange(const uint32_t obj_beg, const uint32_t obj_end) {
    using namespace SceneManagerInternal;

    const auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();
    const auto *drawables = (Drawable *)scene_data_.comp_store[CompDrawable]->SequentialData();
    const auto *lightmaps = (Lightmap *)scene_data_.comp_store[CompLightmap]->SequentialData();
    const auto *vegs = (VegState *)scene_data_.comp_store[CompVegState]->SequentialData();

    const uint32_t total_data_to_update = sizeof(instance_data_t) * (obj_end - obj_beg + 1);
    Ren::BufRef temp_stage_buf =
        ren_ctx_.LoadBuffer("Instance Update Stage Buf", Ren::eBufType::Upload, total_data_to_update);
    auto *instance_stage = (instance_data_t *)temp_stage_buf->Map();

    for (uint32_t i = obj_beg; i <= obj_end; ++i) {
        SceneObject &obj = scene_data_.objects[i];
        assert(obj.comp_mask & CompTransformBit);

        const Transform &tr = transforms[obj.components[CompTransform]];
        const Ren::Mat4f world_from_object_trans = Transpose(tr.world_from_object);
        const Ren::Mat4f prev_world_from_object_trans = Transpose(tr.world_from_object_prev);

        instance_data_t &instance = instance_stage[i - obj_beg];
        memcpy(&instance._model_matrix[0], ValuePtr(world_from_object_trans), 12 * sizeof(float));
        memcpy(&instance._prev_model_matrix[0], ValuePtr(prev_world_from_object_trans), 12 * sizeof(float));

        if (obj.comp_mask & CompDrawableBit) {
            const Drawable &dr = drawables[obj.components[CompDrawable]];
            instance.vis_mask = uint8_t(dr.vis_mask);
        }

        if (obj.comp_mask & CompLightmapBit) {
            const Lightmap &lm = lightmaps[obj.components[CompLightmap]];
            memcpy(&instance.lmap_transform[0], ValuePtr(lm.xform), 4 * sizeof(float));
        } else if (obj.comp_mask & CompVegStateBit) {
            const VegState &vs = vegs[obj.components[CompVegState]];
            __init_wind_params(vs, scene_data_.env, tr.object_from_world, instance);
        }
    }

    temp_stage_buf->Unmap();

    scene_data_.persistent_data.instance_buf->UpdateSubRegion(obj_beg * sizeof(instance_data_t), total_data_to_update,
                                                              *temp_stage_buf, 0, ren_ctx_.current_cmd_buf());
}

void Eng::SceneManager::ClearGICache(Ren::CommandBuffer _cmd_buf) {
    if (!scene_data_.persistent_data.probe_irradiance || !scene_data_.persistent_data.probe_distance ||
        !scene_data_.persistent_data.probe_offset) {
        return;
    }

    Ren::CommandBuffer cmd_buf = _cmd_buf;
    if (!cmd_buf) {
        cmd_buf = ren_ctx_.BegTempSingleTimeCommands();
    }

    const Ren::TransitionInfo transitions[] = {
        {scene_data_.persistent_data.probe_irradiance.get(), Ren::eResState::CopyDst},
        {scene_data_.persistent_data.probe_distance.get(), Ren::eResState::CopyDst},
        {scene_data_.persistent_data.probe_offset.get(), Ren::eResState::CopyDst}};
    TransitionResourceStates(ren_ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);

    static const float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Ren::ClearImage(*scene_data_.persistent_data.probe_irradiance, rgba, cmd_buf);
    Ren::ClearImage(*scene_data_.persistent_data.probe_distance, rgba, cmd_buf);
    Ren::ClearImage(*scene_data_.persistent_data.probe_offset, rgba, cmd_buf);

    if (!_cmd_buf) {
        ren_ctx_.EndTempSingleTimeCommands(cmd_buf);
    }

    for (probe_volume_t &volume : scene_data_.persistent_data.probe_volumes) {
        volume.updates_count = 0;
    }
}