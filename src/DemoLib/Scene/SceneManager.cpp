#include "SceneManager.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <map>

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Ren/SOIL2/SOIL2.h>
#include <Sys/AssetFile.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Sys/MemBuf.h>

extern "C" {
#include <Ren/SOIL2/image_DXT.h>
#include <Ren/SOIL2/stb_image.h>
}

#include "../Utils/Load.h"

namespace SceneManagerConstants {
const float NEAR_CLIP = 0.1f;
const float FAR_CLIP = 10000.0f;

#if defined(__ANDROID__)
const char *MODELS_PATH = "./assets/models/";
const char *TEXTURES_PATH = "./assets/textures/";
const char *MATERIALS_PATH = "./assets/materials/";
const char *SHADERS_PATH = "./assets/shaders/";
#else
const char *MODELS_PATH = "./assets_pc/models/";
const char *TEXTURES_PATH = "./assets_pc/textures/";
const char *MATERIALS_PATH = "./assets_pc/materials/";
const char *SHADERS_PATH = "./assets_pc/shaders/";
#endif

const int DECALS_ATLAS_RESX = 2048,
          DECALS_ATLAS_RESY = 1024;

const int LIGHTMAP_ATLAS_RESX = 2048,
          LIGHTMAP_ATLAS_RESY = 1024;

const int PROBE_RES = 256;
const int PROBE_COUNT = 16;
}

namespace SceneManagerInternal {
    std::unique_ptr<uint8_t[]> Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width, int &height);

    template <typename T>
    class DefaultCompStorage : public CompStorage {
        Ren::SparseArray<T> data_;
    public:
        const char *name() const override { return T::name(); }

        uint32_t Create() override {
            return (uint32_t)data_.Add();
        }

        void *Get(uint32_t i) override {
            return (void *)data_.Get(i);
        }

        void ReadFromJs(const JsObject &js_obj, void *comp) override {
            T::Read(js_obj, *(T *)comp);
        }

        void WriteToJs(const void *comp, JsObject &js_obj) override {
            T::Write(*(T *)comp, js_obj);
        }
    };
}

SceneManager::SceneManager(Ren::Context &ctx, Ray::RendererBase &ray_renderer, Sys::ThreadPool &threads)
    : ctx_(ctx),
      ray_renderer_(ray_renderer),
      threads_(threads),
      cam_(Ren::Vec3f{ 0.0f, 0.0f, 1.0f },
           Ren::Vec3f{ 0.0f, 0.0f, 0.0f },
           Ren::Vec3f{ 0.0f, 1.0f, 0.0f }) {
    using namespace SceneManagerConstants;
    using namespace SceneManagerInternal;

    {   // Alloc texture for decals atlas        
        Ren::eTexColorFormat formats[] = { Ren::RawRGBA8888, Ren::Undefined };
        scene_data_.decals_atlas = Ren::TextureAtlas{ DECALS_ATLAS_RESX, DECALS_ATLAS_RESY, formats, Ren::Trilinear };
    }

    {   // Create splitter for lightmap atlas
        scene_data_.lm_splitter = Ren::TextureSplitter(SceneManagerConstants::LIGHTMAP_ATLAS_RESX,
                                                       SceneManagerConstants::LIGHTMAP_ATLAS_RESY);
    }

    {   // Allocate cubemap array
        scene_data_.probe_storage.Resize(PROBE_RES, PROBE_COUNT);
    }

    {   // Register default components
        default_comp_storage_[CompTransform].reset(new DefaultCompStorage<Transform>);
        RegisterComponent(CompTransform, default_comp_storage_[CompTransform].get());
        
        default_comp_storage_[CompDrawable].reset(new DefaultCompStorage<Drawable>);
        RegisterComponent(CompDrawable, default_comp_storage_[CompDrawable].get());

        default_comp_storage_[CompOccluder].reset(new DefaultCompStorage<Occluder>);
        RegisterComponent(CompOccluder, default_comp_storage_[CompOccluder].get());

        default_comp_storage_[CompLightmap].reset(new DefaultCompStorage<Lightmap>);
        RegisterComponent(CompLightmap, default_comp_storage_[CompLightmap].get());

        default_comp_storage_[CompLightSource].reset(new DefaultCompStorage<LightSource>);
        RegisterComponent(CompLightSource, default_comp_storage_[CompLightSource].get());

        default_comp_storage_[CompDecal].reset(new DefaultCompStorage<Decal>);
        RegisterComponent(CompDecal, default_comp_storage_[CompDecal].get());

        default_comp_storage_[CompProbe].reset(new DefaultCompStorage<LightProbe>);
        RegisterComponent(CompProbe, default_comp_storage_[CompProbe].get());
    }
}

SceneManager::~SceneManager() {
}

void SceneManager::RegisterComponent(uint32_t index, CompStorage *storage) {
    scene_data_.comp_store[index] = storage;
}

void SceneManager::LoadScene(const JsObject &js_scene) {
    using namespace SceneManagerConstants;

    LOGI("SceneManager: Loading scene!");
    ClearScene();

    std::map<std::string, Ren::MeshRef> all_meshes;
    std::map<std::string, Ren::Vec4f> decals_textures;

    if (js_scene.Has("name")) {
        const JsString &js_name = (const JsString &)js_scene.at("name");
        scene_name_ = js_name.val;
    } else {
        throw std::runtime_error("Level has no name!");
    }

    scene_texture_load_counter_ = 0;

    {
        std::string lm_base_tex_name = "lightmaps/";
        lm_base_tex_name += scene_name_;

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

        std::string lm_indir_sh_tex_name[4] = { lm_base_tex_name, lm_base_tex_name, lm_base_tex_name, lm_base_tex_name };
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            lm_indir_sh_tex_name[sh_l] += "_lm_sh_";
            lm_indir_sh_tex_name[sh_l] += std::to_string(sh_l);
            lm_indir_sh_tex_name[sh_l] += tex_ext;
        }

        scene_data_.env.lm_direct = OnLoadTexture(lm_direct_tex_name.c_str());
        //scene_data_.env.lm_indir = OnLoadTexture(lm_indir_tex_name.c_str());
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            scene_data_.env.lm_indir_sh[sh_l] = OnLoadTexture(lm_indir_sh_tex_name[sh_l].c_str());
        }
    }

    const JsObject &js_meshes = (const JsObject &)js_scene.at("meshes");
    for (const auto &js_elem : js_meshes.elements) {
        const std::string &name = js_elem.first;
        const JsString &path = (const JsString &)js_elem.second;

        std::string mesh_path = std::string(MODELS_PATH) + path.val;

        Sys::AssetFile in_file(mesh_path.c_str());
        size_t in_file_size = in_file.size();

        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
        in_file.Read((char *)&in_file_data[0], in_file_size);

        Sys::MemBuf mem = { &in_file_data[0], in_file_size };
        std::istream in_file_stream(&mem);

        using namespace std::placeholders;
        all_meshes[name] = ctx_.LoadMesh(name.c_str(), in_file_stream, std::bind(&SceneManager::OnLoadMaterial, this, _1));
    }

    auto load_decal_texture = [this](const std::string &name) {
        std::string file_name = TEXTURES_PATH + name;

        Sys::AssetFile in_file(file_name, Sys::AssetFile::IN);
        size_t in_file_size = in_file.size();

        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
        in_file.Read((char *)&in_file_data[0], in_file_size);

        int res[2];
#if !defined(__ANDROID__)
        int channels;
        uint8_t *image_data = SOIL_load_image_from_memory(&in_file_data[0], (int)in_file_size, &res[0], &res[1], &channels, 4);
        assert(channels == 4);
#else
        auto image_data = SceneManagerInternal::Decode_KTX_ASTC(&in_file_data[0], in_file_size, res[0], res[1]);

#endif

        const void *data[] = { (const void *)&image_data[0], nullptr };
        const Ren::eTexColorFormat formats[] = { Ren::RawRGBA8888, Ren::Undefined };

        int pos[2];
        int rc = scene_data_.decals_atlas.Allocate(data, formats, res, pos, 4);
        if (rc == -1) throw std::runtime_error("Cannot allocate decal!");

#if !defined(__ANDROID__)
        SOIL_free_image_data(image_data);
#endif

        return Ren::Vec4f{ float(pos[0]) / DECALS_ATLAS_RESX,
                           float(pos[1]) / DECALS_ATLAS_RESY,
                           float(res[0]) / DECALS_ATLAS_RESX,
                           float(res[1]) / DECALS_ATLAS_RESY };
    };

    const JsArray &js_objects = (const JsArray &)js_scene.at("objects");
    for (const auto &js_elem : js_objects.elements) {
        const JsObject &js_obj = (const JsObject &)js_elem;

        SceneObject obj;
        obj.comp_mask = 0;
        obj.change_mask = 0;

        Ren::Vec3f obj_bbox_min = Ren::Vec3f{ std::numeric_limits<float>::max() },
                   obj_bbox_max = Ren::Vec3f{ -std::numeric_limits<float>::max() };

        for (const auto &js_comp : js_obj.elements) {
            if (js_comp.second.type() != JS_OBJECT) continue;
            const auto &js_comp_obj = (const JsObject &)js_comp.second;
            const std::string &js_comp_name = js_comp.first;

            for (int i = 0; i < MAX_COMPONENT_TYPES; i++) {
                auto *store = scene_data_.comp_store[i];
                if (!store) continue;

                if (js_comp_name == store->name()) {
                    uint32_t index = store->Create();

                    void *new_component = store->Get(index);
                    store->ReadFromJs(js_comp_obj, new_component);

                    obj.components[i] = index;
                    obj.comp_mask |= (1 << i);

                    // TODO: refactor this into something generic
                    if (i == CompDrawable) {
                        const JsString &js_mesh_name = (const JsString &)js_comp_obj.at("mesh");

                        const auto it = all_meshes.find(js_mesh_name.val);
                        if (it == all_meshes.end()) throw std::runtime_error("Cannot find mesh!");

                        auto *dr = (Drawable *)new_component;
                        dr->mesh = it->second;

                        obj_bbox_min = Ren::Min(obj_bbox_min, dr->mesh->bbox_min());
                        obj_bbox_max = Ren::Max(obj_bbox_max, dr->mesh->bbox_max());
                    } else if (i == CompOccluder) {
                        const JsString &js_mesh_name = (const JsString &)js_comp_obj.at("mesh");

                        const auto it = all_meshes.find(js_mesh_name.val);
                        if (it == all_meshes.end()) throw std::runtime_error("Cannot find mesh!");

                        auto *occ = (Occluder *)new_component;
                        occ->mesh = it->second;

                        obj_bbox_min = Ren::Min(obj_bbox_min, occ->mesh->bbox_min());
                        obj_bbox_max = Ren::Max(obj_bbox_max, occ->mesh->bbox_max());
                    } else if (i == CompLightmap) {
                        auto *lm = (Lightmap *)new_component;

                        int node_id = scene_data_.lm_splitter.Allocate(lm->size, lm->pos);
                        if (node_id == -1) {
                            throw std::runtime_error("Cannot allocate lightmap region!");
                        }

                        lm->xform = Ren::Vec4f{
                            float(lm->pos[0]) / LIGHTMAP_ATLAS_RESX, 1.0f - float(lm->pos[1]) / LIGHTMAP_ATLAS_RESY,
                            float(lm->size[0]) / LIGHTMAP_ATLAS_RESX, -float(lm->size[1]) / LIGHTMAP_ATLAS_RESY,
                        };
                    } else if (i == CompLightSource) {
                        auto *ls = (LightSource *)scene_data_.comp_store[CompLightSource]->Get(obj.components[CompLightSource]);

                        // Compute bounding box of light source
                        Ren::Vec4f pos = { ls->offset[0], ls->offset[1], ls->offset[2], 1.0f },
                                   dir = { ls->dir[0], ls->dir[1], ls->dir[2], 0.0f };

                        Ren::Vec3f bbox_min, bbox_max;

                        Ren::Vec3f _dir = { dir[0], dir[1], dir[2] };
                        Ren::Vec3f p1 = _dir * ls->influence;

                        bbox_min = Ren::Min(bbox_min, p1);
                        bbox_max = Ren::Max(bbox_max, p1);

                        Ren::Vec3f p2 = _dir * ls->spot * ls->influence;

                        float d = std::sqrt(1.0f - ls->spot * ls->spot) * ls->influence;

                        bbox_min = Ren::Min(bbox_min, p2 - Ren::Vec3f{ d, 0.0f, d });
                        bbox_max = Ren::Max(bbox_max, p2 + Ren::Vec3f{ d, 0.0f, d });

                        if (ls->spot < 0.0f) {
                            bbox_min = Ren::Min(bbox_min, p1 - Ren::Vec3f{ ls->influence, 0.0f, ls->influence });
                            bbox_max = Ren::Max(bbox_max, p1 + Ren::Vec3f{ ls->influence, 0.0f, ls->influence });
                        }

                        Ren::Vec3f up = { 1.0f, 0.0f, 0.0f };
                        if (std::abs(_dir[1]) < std::abs(_dir[2]) && std::abs(_dir[1]) < std::abs(_dir[0])) {
                            up = { 0.0f, 1.0f, 0.0f };
                        } else if (std::abs(_dir[2]) < std::abs(_dir[0]) && std::abs(_dir[2]) < std::abs(_dir[1])) {
                            up = { 0.0f, 0.0f, 1.0f };
                        }

                        Ren::Vec3f side = Ren::Cross(_dir, up);

                        Transform ls_transform;
                        ls_transform.mat = { Ren::Vec4f{ side[0],  -_dir[0], up[0],    0.0f },
                                             Ren::Vec4f{ side[1],  -_dir[1], up[1],    0.0f },
                                             Ren::Vec4f{ side[2],  -_dir[2], up[2],    0.0f },
                                             Ren::Vec4f{ ls->offset[0], ls->offset[1], ls->offset[2], 1.0f } };

                        ls_transform.bbox_min = bbox_min;
                        ls_transform.bbox_max = bbox_max;
                        ls_transform.UpdateBBox();

                        // Combine light's bounding box with object's
                        obj_bbox_min = Ren::Min(obj_bbox_min, ls_transform.bbox_min_ws);
                        obj_bbox_max = Ren::Max(obj_bbox_max, ls_transform.bbox_max_ws);
                    } else if (i == CompDecal) {
                        auto *de = (Decal *)new_component;

                        if (js_comp_obj.Has("diff")) {
                            const JsString &js_diff = (const JsString &)js_comp_obj.at("diff");

                            auto it = decals_textures.find(js_diff.val);

                            if (it == decals_textures.end()) {
                                de->diff = load_decal_texture(js_diff.val);
                                decals_textures[js_diff.val] = de->diff;
                            } else {
                                de->diff = decals_textures[js_diff.val];
                            }
                        }

                        if (js_comp_obj.Has("norm")) {
                            const JsString &js_norm = (const JsString &)js_comp_obj.at("norm");

                            auto it = decals_textures.find(js_norm.val);

                            if (it == decals_textures.end()) {
                                de->norm = load_decal_texture(js_norm.val);
                                decals_textures[js_norm.val] = de->norm;
                            } else {
                                de->norm = decals_textures[js_norm.val];
                            }
                        }

                        if (js_comp_obj.Has("spec")) {
                            const JsString &js_spec = (const JsString &)js_comp_obj.at("spec");

                            auto it = decals_textures.find(js_spec.val);

                            if (it == decals_textures.end()) {
                                de->spec = load_decal_texture(js_spec.val);
                                decals_textures[js_spec.val] = de->spec;
                            } else {
                                de->spec = decals_textures[js_spec.val];
                            }
                        }

                        Ren::Vec4f points[] = {
                            { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, 1.0f, -1.0f, 1.0f },
                            { 1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, -1.0f, 1.0f },

                            { -1.0f, -1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f, 1.0f },
                            { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f, 1.0f }
                        };

                        Ren::Mat4f world_from_clip = Ren::Inverse(de->proj * de->view);

                        for (int i = 0; i < 8; i++) {
                            points[i] = world_from_clip * points[i];
                            points[i] /= points[i][3];

                            // Combine decals's bounding box with object's
                            obj_bbox_min = Ren::Min(obj_bbox_min, Ren::Vec3f{ points[i] });
                            obj_bbox_max = Ren::Max(obj_bbox_max, Ren::Vec3f{ points[i] });
                        }
                    } else if (i == CompProbe) {
                        auto *pr = (LightProbe *)new_component;

                        pr->layer_index = scene_data_.probe_storage.Allocate();

                        // Combine probe's bounding box with object's
                        obj_bbox_min = Ren::Min(obj_bbox_min, pr->offset - Ren::Vec3f{ pr->radius });
                        obj_bbox_max = Ren::Max(obj_bbox_max, pr->offset + Ren::Vec3f{ pr->radius });
                    }

                    break;
                }
            }
        }

        auto *tr = (Transform *)scene_data_.comp_store[CompTransform]->Get(obj.components[CompTransform]);
        tr->bbox_min = obj_bbox_min;
        tr->bbox_max = obj_bbox_max;
        tr->UpdateBBox();

        scene_data_.objects.push_back(obj);
    }

    if (js_scene.Has("environment")) {
        const JsObject &js_env = (const JsObject &)js_scene.at("environment");
        if (js_env.Has("sun_dir")) {
            const JsArray &js_dir = (const JsArray &)js_env.at("sun_dir");

            double x = ((const JsNumber &)js_dir.at(0)).val;
            double y = ((const JsNumber &)js_dir.at(1)).val;
            double z = ((const JsNumber &)js_dir.at(2)).val;

            scene_data_.env.sun_dir = Ren::Vec3f{ float(x), float(y), float(z) };
            scene_data_.env.sun_dir = -Ren::Normalize(scene_data_.env.sun_dir);
        }
        if (js_env.Has("sun_col")) {
            const JsArray &js_col = (const JsArray &)js_env.at("sun_col");

            double r = ((const JsNumber &)js_col.at(0)).val;
            double g = ((const JsNumber &)js_col.at(1)).val;
            double b = ((const JsNumber &)js_col.at(2)).val;

            scene_data_.env.sun_col = Ren::Vec3f{ float(r), float(g), float(b) };
        }
        if (js_env.Has("sun_softness")) {
            const JsNumber &js_sun_softness = js_env.at("sun_softness");
            scene_data_.env.sun_softness = (float)js_sun_softness.val;
        }
        if (js_env.Has("env_map")) {
            const JsString &js_env_map = (const JsString &)js_env.at("env_map");

            const std::string tex_names[6] = {
#if !defined(__ANDROID__)
                TEXTURES_PATH + js_env_map.val + "_PX.dds",
                TEXTURES_PATH + js_env_map.val + "_NX.dds",
                TEXTURES_PATH + js_env_map.val + "_PY.dds",
                TEXTURES_PATH + js_env_map.val + "_NY.dds",
                TEXTURES_PATH + js_env_map.val + "_PZ.dds",
                TEXTURES_PATH + js_env_map.val + "_NZ.dds"
#else
                TEXTURES_PATH + js_env_map.val + "_PX.ktx",
                TEXTURES_PATH + js_env_map.val + "_NX.ktx",
                TEXTURES_PATH + js_env_map.val + "_PY.ktx",
                TEXTURES_PATH + js_env_map.val + "_NY.ktx",
                TEXTURES_PATH + js_env_map.val + "_PZ.ktx",
                TEXTURES_PATH + js_env_map.val + "_NZ.ktx"
#endif
            };

            std::vector<uint8_t> tex_data[6];
            const void *data[6];
            int size[6];
            int res = 0;

            for (int i = 0; i < 6; i++) {
                Sys::AssetFile in_file(tex_names[i], Sys::AssetFile::IN);
                size_t in_file_size = in_file.size();

                tex_data[i].resize(in_file_size);
                in_file.Read((char *)&tex_data[i][0], in_file_size);

#if !defined(__ANDROID__)
                DDS_header header;
                memcpy(&header, &tex_data[i][0], sizeof(DDS_header));

                int w = (int)header.dwWidth;
                int h = (int)header.dwHeight;

                assert(w == h);
                res = w;
#else
                
#endif

                data[i] = (const void *)&tex_data[i][0];
                size[i] = (int)tex_data[i].size();
                
            }

            Ren::Texture2DParams p;
            p.format = Ren::RawRGBA8888;
            p.filter = Ren::Bilinear;
            p.repeat = Ren::ClampToEdge;
            p.w = res;
            p.h = res;

            std::string tex_name = js_env_map.val +
#if !defined(__ANDROID__)
                "_*.dds";
#else
                "_*.ktx";
#endif

            Ren::eTexLoadStatus load_status;
            scene_data_.env.env_map = ctx_.LoadTextureCube(tex_name.c_str(), data, size, p, &load_status);
        }
        if (js_env.Has("env_map_pt")) {
            env_map_pt_name_ = ((const JsString &)js_env.at("env_map_pt")).val;
        }
    } else {
        scene_data_.env = {};
    }

    scene_data_.decals_atlas.Finalize();

    LOGI("SceneManager: RebuildBVH!");

    RebuildBVH();
}

void SceneManager::ClearScene() {
    scene_name_.clear();
    scene_data_.objects.clear();

    ray_scene_ = nullptr;
}

void SceneManager::SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up, float fov) {
    using namespace SceneManagerConstants;

    cam_.SetupView(origin, target, up);
    cam_.Perspective(fov, float(ctx_.w()) / ctx_.h(), NEAR_CLIP, FAR_CLIP);
    cam_.UpdatePlanes();
}

Ren::MaterialRef SceneManager::OnLoadMaterial(const char *name) {
    using namespace SceneManagerConstants;

    Ren::eMatLoadStatus status;
    Ren::MaterialRef ret = ctx_.LoadMaterial(name, nullptr, &status, nullptr, nullptr);
    if (!ret->ready()) {
        Sys::AssetFile in_file(std::string(MATERIALS_PATH) + name);
        if (!in_file) {
            LOGE("Error loading material %s", name);
            return ret;
        }

        size_t file_size = in_file.size();

        std::string mat_src;
        mat_src.resize(file_size);
        in_file.Read((char *)mat_src.data(), file_size);

        using namespace std::placeholders;

        ret = ctx_.LoadMaterial(name, mat_src.data(), &status,
                                std::bind(&SceneManager::OnLoadProgram, this, _1, _2, _3),
                                std::bind(&SceneManager::OnLoadTexture, this, _1));
        assert(status == Ren::MatCreatedFromData);
    }
    return ret;
}

Ren::ProgramRef SceneManager::OnLoadProgram(const char *name, const char *vs_shader, const char *fs_shader) {
    using namespace SceneManagerConstants;

#if defined(USE_GL_RENDER)
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx_.LoadProgramGLSL(name, nullptr, nullptr, &status);
    if (!ret->ready()) {
        using namespace std;

        Sys::AssetFile vs_file(std::string(SHADERS_PATH) + vs_shader),
                       fs_file(std::string(SHADERS_PATH) + fs_shader);
        if (!vs_file || !fs_file) {
            LOGE("Error loading program %s", name);
            return ret;
        }

        size_t vs_size = vs_file.size(),
               fs_size = fs_file.size();

        string vs_src, fs_src;
        vs_src.resize(vs_size);
        fs_src.resize(fs_size);
        vs_file.Read((char *)vs_src.data(), vs_size);
        fs_file.Read((char *)fs_src.data(), fs_size);

        ret = ctx_.LoadProgramGLSL(name, vs_src.c_str(), fs_src.c_str(), &status);
        assert(status == Ren::ProgCreatedFromData);
    }
    return ret;
#elif defined(USE_SW_RENDER)
    ren::ProgramRef LoadSWProgram(ren::Context &, const char *);
    return LoadSWProgram(ctx_, name);
#endif
}

Ren::Texture2DRef SceneManager::OnLoadTexture(const char *name) {
    using namespace SceneManagerConstants;

    std::string tex_name = TEXTURES_PATH;
    tex_name += name;

    Ren::eTexLoadStatus status;
    Ren::Texture2DRef ret = ctx_.LoadTexture2D(tex_name.c_str(), nullptr, 0, {}, &status);
    if (status == Ren::TexCreatedDefault) {
        scene_texture_load_counter_++;

        std::weak_ptr<SceneManager> _self = shared_from_this();
        Sys::LoadAssetComplete(tex_name.c_str(),
        [_self, tex_name](void *data, int size) {
            auto self = _self.lock();
            if (!self) return;

            self->ctx_.ProcessSingleTask([&self, tex_name, data, size]() {
                Ren::Texture2DParams p;
                if (strstr(tex_name.c_str(), ".tga_rgbe")) {
                    p.filter = Ren::BilinearNoMipmap;
                    p.repeat = Ren::ClampToEdge;
                } else {
                    p.filter = Ren::Trilinear;
                    p.repeat = Ren::Repeat;
                }
                self->ctx_.LoadTexture2D(tex_name.c_str(), data, size, p, nullptr);
                int count = --(self->scene_texture_load_counter_);
                self.reset();
                LOGI("Texture %s loaded (%i left)", tex_name.c_str(), count);
            });
        }, [tex_name]() {
            LOGE("Error loading %s", tex_name.c_str());
        });
    }

    return ret;
}
