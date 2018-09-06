#include "SceneManager.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <map>

#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Log.h>

#include "Renderer.h"

namespace SceneManagerConstants {
    const float NEAR_CLIP = 0.5f;
    const float FAR_CLIP = 10000;

    const char *MODELS_PATH = "./assets/models/";
}

SceneManager::SceneManager(Ren::Context &ctx, Renderer &renderer) : ctx_(ctx),
                                                renderer_(renderer),
                                                cam_(Ren::Vec3f{ 0.0f, 0.0f, 1.0f },
                                                     Ren::Vec3f{ 0.0f, 0.0f, 0.0f },
                                                     Ren::Vec3f{ 0.0f, 1.0f, 0.0f }) {
}

SceneManager::~SceneManager() {
    renderer_.WaitForBackgroundThreadIteration();
}

TimingInfo SceneManager::timings() const { return renderer_.timings(); };
TimingInfo SceneManager::back_timings() const { return renderer_.back_timings(); };

void SceneManager::SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up) {
    cam_.SetupView(origin, target, up);
    cam_.UpdatePlanes();
}

void SceneManager::LoadScene(const JsObject &js_scene) {
    using namespace SceneManagerConstants;

    ClearScene();

    std::map<std::string, Ren::MeshRef> all_meshes;

    const JsObject &js_meshes = (const JsObject &)js_scene.at("meshes");
    for (const auto &js_elem : js_meshes.elements) {
        const std::string &name = js_elem.first;
        const JsString &path = (const JsString &)js_elem.second;

        std::string mesh_path = std::string(MODELS_PATH) + path.val;

        std::ifstream in_file(mesh_path.c_str(), std::ios::binary);

        using namespace std::placeholders;
        all_meshes[name] = ctx_.LoadMesh(name.c_str(), in_file, std::bind(&SceneManager::OnLoadMaterial, this, _1));
    }

    const JsArray &js_objects = (const JsArray &)js_scene.at("objects");
    for (const auto &js_elem : js_objects.elements) {
        const JsObject &js_obj = (const JsObject &)js_elem;
        const JsString &js_mesh_name = (const JsString &)js_obj.at("mesh");

        const auto it = all_meshes.find(js_mesh_name.val);
        if (it == all_meshes.end()) throw std::runtime_error("Cannot find mesh!");

        SceneObject obj;
        obj.flags = HasMesh | HasTransform;
        obj.mesh = it->second;
        obj.tr = transforms_.Add();
        
        if (js_obj.Has("pos")) {
            const JsArray &js_pos = (const JsArray &)js_obj.at("pos");
            
            double x = ((const JsNumber &)js_pos.at(0)).val;
            double y = ((const JsNumber &)js_pos.at(1)).val;
            double z = ((const JsNumber &)js_pos.at(2)).val;

            obj.tr->mat = Ren::Translate(obj.tr->mat, Ren::Vec3f{ (float)x, (float)y, (float)z });
        }

        obj.tr->UpdateBBox(it->second->bbox_min(), it->second->bbox_max());

        if (js_obj.Has("occluder_mesh")) {
            const JsString &js_occ_mesh = (const JsString &)js_obj.at("occluder_mesh");

            const auto it = all_meshes.find(js_occ_mesh.val);
            if (it == all_meshes.end()) throw std::runtime_error("Cannot find mesh!");

            obj.flags |= HasOccluder;
            obj.occ_mesh = it->second;
        }

        objects_.push_back(obj);
    }

    RebuildBVH();
}

void SceneManager::ClearScene() {
    objects_.clear();

    assert(transforms_.Size() == 0);
}

void SceneManager::Draw() {
    using namespace SceneManagerConstants;

    cam_.Perspective(60.0f, float(ctx_.w()) / ctx_.h(), NEAR_CLIP, FAR_CLIP);

    renderer_.DrawObjects(cam_, &nodes_[0], 0, &objects_[0], objects_.size());
}

Ren::MaterialRef SceneManager::OnLoadMaterial(const char *name) {
    Ren::eMatLoadStatus status;
    Ren::MaterialRef ret = ctx_.LoadMaterial(name, nullptr, &status, nullptr, nullptr);
    if (!ret->ready()) {
        Sys::AssetFile in_file(std::string("assets/materials/") + name);
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
#if defined(USE_GL_RENDER)
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx_.LoadProgramGLSL(name, nullptr, nullptr, &status);
    if (!ret->ready()) {
        using namespace std;

        Sys::AssetFile vs_file(std::string("assets/shaders/") + vs_shader),
                       fs_file(std::string("assets/shaders/") + fs_shader);
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
    Ren::eTexLoadStatus status;
    Ren::Texture2DRef ret = ctx_.LoadTexture2D(name, nullptr, 0, {}, &status);
    if (!ret->ready()) {
        std::string tex_name = name;
        Sys::LoadAssetComplete((std::string("assets/textures/") + tex_name).c_str(),
            [this, tex_name](void *data, int size) {

            ctx_.ProcessSingleTask([this, tex_name, data, size]() {
                Ren::Texture2DParams p;
                p.filter = Ren::Trilinear;
                p.repeat = Ren::Repeat;
                ctx_.LoadTexture2D(tex_name.c_str(), data, size, p, nullptr);
                LOGI("Texture %s loaded", tex_name.c_str());
            });
        }, [tex_name]() {
            LOGE("Error loading %s", tex_name.c_str());
        });
    }

    return ret;
}
