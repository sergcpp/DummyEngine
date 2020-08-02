#pragma once

#include <deque>
#include <memory>

#include <Ray/RendererBase.h>
#include <Ren/Camera.h>
#include <Ren/RingBuffer.h>

#include "SceneData.h"

struct JsObject;

namespace Sys {
class ThreadPool;
}

namespace Snd {
class Context;
}

struct assets_context_t {
    const char *platform;
    Ren::ILog *log;
    JsObject &js_db;
    Ren::HashMap32<const char *, int> &db_map;
    Sys::ThreadPool *p_threads;
};

class SceneManager : public std::enable_shared_from_this<SceneManager> {
  public:
    SceneManager(Ren::Context &ren_ctx, Snd::Context &snd_ctx,
                 Ray::RendererBase &ray_renderer, Sys::ThreadPool &threads);
    ~SceneManager();

    SceneManager(const SceneManager &rhs) = delete;

    const Ren::Camera &main_cam() const { return cam_; }
    Ren::Camera &main_cam() { return cam_; }
    Ren::Mesh *cam_rig() { return cam_rig_.get(); }
    SceneData &scene_data() { return scene_data_; }
    bool load_complete() const { return scene_texture_load_counter_ == 0; }

    Snd::Source &ambient_sound() { return amb_sound_; }

    SceneObject *GetObject(uint32_t i) { return &scene_data_.objects[i]; }

    uint32_t FindObject(const char *name) {
        uint32_t *p_ndx = scene_data_.name_to_object.Find(name);
        return p_ndx ? (*p_ndx) : 0xffffffff;
    }

    void InvalidateObjects(const uint32_t *indices, uint32_t count,
                           uint32_t change_mask) {
        for (uint32_t i = 0; i < count; i++) {
            scene_data_.objects[indices[i]].change_mask |= change_mask;
        }
        changed_objects_.insert(changed_objects_.end(), indices, indices + count);
    }

    void LoadScene(const JsObject &js_scene);
    void SaveScene(JsObject &js_scene);
    void ClearScene();

    void LoadProbeCache();

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target,
                   const Ren::Vec3f &up, float fov, float max_exposure);

    using PostLoadFunc = void(const JsObject &js_comp_obj, void *comp,
                              Ren::Vec3f obj_bbox[2]);
    void RegisterComponent(uint32_t index, CompStorage *storage,
                           const std::function<PostLoadFunc> &post_init);

    void InitScene_PT(bool _override = false);
    void SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target,
                      const Ren::Vec3f &up, float fov);
    const float *Draw_PT(int *w, int *h);
    void Clear_PT();

    void ResetLightmaps_PT();
    bool PrepareLightmaps_PT(const float **preview_pixels, int *w, int *h);

    void UpdateObjects();

    void Serve(int texture_budget = 1);

    using ConvertAssetFunc = std::function<bool(
        assets_context_t &ctx, const char *in_file, const char *out_file)>;
    static void RegisterAsset(const char *in_ext, const char *out_ext,
                              const ConvertAssetFunc &convert_func);
    static bool PrepareAssets(const char *in_folder, const char *out_folder,
                              const char *platform, Sys::ThreadPool *p_threads,
                              Ren::ILog *log);
    static bool WriteProbeCache(const char *out_folder, const char *scene_name,
                                const ProbeStorage &probes,
                                const CompStorage *light_probe_storage, Ren::ILog *log);

  private:
    void PostloadDrawable(const JsObject &js_comp_obj, void *comp,
                          Ren::Vec3f obj_bbox[2]);
    void PostloadOccluder(const JsObject &js_comp_obj, void *comp,
                          Ren::Vec3f obj_bbox[2]);
    void PostloadLightmap(const JsObject &js_comp_obj, void *comp,
                          Ren::Vec3f obj_bbox[2]);
    void PostloadLightSource(const JsObject &js_comp_obj, void *comp,
                             Ren::Vec3f obj_bbox[2]);
    void PostloadDecal(const JsObject &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightProbe(const JsObject &js_comp_obj, void *comp,
                            Ren::Vec3f obj_bbox[2]);
    void PostloadSoundSource(const JsObject &js_comp_obj, void *comp,
                             Ren::Vec3f obj_bbox[2]);

    Ren::MaterialRef OnLoadMaterial(const char *name);
    Ren::ProgramRef OnLoadProgram(const char *name, const char *v_shader,
                                  const char *f_shader, const char *tc_shader,
                                  const char *te_shader);
    Ren::Texture2DRef OnLoadTexture(const char *name, uint32_t flags);

    Ren::Vec4f LoadDecalTexture(const char *name);

    void RebuildBVH();
    void RemoveNode(uint32_t node_index);

    int scene_texture_load_counter_ = 0;

    Ren::Context &ren_ctx_;
    Snd::Context &snd_ctx_;
    Ren::MeshRef cam_rig_;
    Ray::RendererBase &ray_renderer_;
    Sys::ThreadPool &threads_;
    std::vector<Ray::RegionContext> ray_reg_ctx_;
    std::shared_ptr<Ray::SceneBase> ray_scene_;

    Ren::Camera cam_;
    Ren::Vec3f last_cam_pos_;
    double last_cam_time_s_ = 0.0;
    Snd::Source amb_sound_;

    SceneData scene_data_;
    std::vector<uint32_t> changed_objects_, last_changed_objects_;

    std::unique_ptr<CompStorage> default_comp_storage_[MAX_COMPONENT_TYPES];
    std::function<PostLoadFunc> component_post_load_[MAX_COMPONENT_TYPES];

    struct TextureRequest {
        Ren::Texture2DRef ref;
        const void *data;
        int data_size;
    };
    std::deque<Ren::Texture2DRef> requested_textures_;
    Ren::RingBuffer<TextureRequest> loading_textures_, pending_textures_;

    static void OnTextureDataLoaded(void *arg, void *data, int size);
    static void OnTextureDataFailed(void *arg);

    std::vector<char> temp_buf;

    bool cur_lm_indir_ = false;
    uint32_t cur_lm_obj_ = 0;

    // PT temp data
    std::vector<Ray::pixel_color_t> pt_lm_direct_, pt_lm_indir_, pt_lm_indir_sh_[4];
    double pt_lm_started_time_s_ = 0.0;

    // static data for assets conversion
    struct Handler {
        const char *ext;
        ConvertAssetFunc convert;
    };

    static Ren::HashMap32<std::string, Handler> g_asset_handlers;

    static void InitASTCCodec();
    static void WriteCommonShaderIncludes(const char *in_folder);

    static bool HSkip(assets_context_t &ctx, const char *in_file, const char *out_file);
    static bool HCopy(assets_context_t &ctx, const char *in_file, const char *out_file);

    // image textures
    static bool HConvToASTC(assets_context_t &ctx, const char *in_file,
                            const char *out_file);
    static bool HConvToDDS(assets_context_t &ctx, const char *in_file,
                           const char *out_file);

    static bool HConvHDRToRGBM(assets_context_t &ctx, const char *in_file,
                               const char *out_file);
    static bool HPreprocessHeightmap(assets_context_t &ctx, const char *in_file,
                                     const char *out_file);

    // probe textures
    static bool HConvImgToDDS(assets_context_t &ctx, const char *in_file,
                              const char *out_file);
    static bool HConvImgToASTC(assets_context_t &ctx, const char *in_file,
                               const char *out_file);

    // shaders
    static void InlineShaderConstants(assets_context_t &ctx, std::string &line);
    static bool HPreprocessShader(assets_context_t &ctx, const char *in_file,
                                  const char *out_file);

    // materials
    static bool HPreprocessMaterial(assets_context_t &ctx, const char *in_file,
                                    const char *out_file);

    // scenes
    static bool HPreprocessJson(assets_context_t &ctx, const char *in_file,
                                const char *out_file);

    // fonts
    static bool HConvTTFToFont(assets_context_t &ctx, const char *in_file,
                               const char *out_file);
};
