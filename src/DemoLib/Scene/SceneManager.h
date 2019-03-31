#pragma once

#include <memory>

#include <Ray/RendererBase.h>
#include <Ren/Camera.h>

#include "SceneData.h"

struct JsObject;

namespace Sys {
    class ThreadPool;
}

class SceneManager : public std::enable_shared_from_this<SceneManager> {
public:
    SceneManager(Ren::Context &ctx, Ray::RendererBase &ray_renderer, Sys::ThreadPool &threads);
    ~SceneManager();

    const Ren::Camera &main_cam() const { return cam_; }
    const SceneData &scene_data() const { return scene_data_; }

    SceneObject *GetObject(int i) { return &scene_data_.objects[i]; }

    void InvalidateObjects(const uint32_t *indices, uint32_t count, uint32_t change_mask) {
        for (uint32_t i = 0; i < count; i++) {
            scene_data_.objects[indices[i]].change_mask |= change_mask;
        }
        changed_objects_.insert(changed_objects_.end(), indices, indices + count);
    }

    void LoadScene(const JsObject &js_scene);
    void ClearScene();

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);

    void InitScene_PT(bool _override = false);
    void SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);
    const float *Draw_PT(int *w, int *h);
    void Clear_PT();

    void ResetLightmaps_PT();
    bool PrepareLightmaps_PT(const float **preview_pixels, int *w, int *h);

    void UpdateObjects();

    static bool PrepareAssets(const char *in_folder, const char *out_folder, const char *platform, Sys::ThreadPool *p_threads);
private:
    Ren::MaterialRef OnLoadMaterial(const char *name);
    Ren::ProgramRef OnLoadProgram(const char *name, const char *arg1, const char *arg2);
    Ren::Texture2DRef OnLoadTexture(const char *name);

    void RebuildBVH();
    void RemoveNode(uint32_t node_index);
    void UpdateBVH();

    std::string scene_name_;

    Ren::Context &ctx_;
    Ray::RendererBase &ray_renderer_;
    Sys::ThreadPool &threads_;
    std::vector<Ray::RegionContext> ray_reg_ctx_;
    std::shared_ptr<Ray::SceneBase> ray_scene_;

    Ren::Camera cam_;
    std::string env_map_pt_name_;

    SceneData scene_data_;
    std::vector<uint32_t> changed_objects_;

    std::vector<char> temp_buf;

    bool cur_lm_indir_ = false;
    size_t cur_lm_obj_ = 0;

    // PT temp data
    std::vector<Ray::pixel_color_t> pt_lm_direct_, pt_lm_indir_,
                                    pt_lm_indir_sh_[4];
};