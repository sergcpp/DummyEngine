#include "SceneManager.h"

#include <Ren/Context.h>

#include "Renderer.h"

void SceneManager::DrawPT() {
    if (!ray_scene_) {
        ray_scene_ = ray_renderer_.CreateScene();

        // Setup environment
        {   Ray::environment_desc_t env_desc;
            memcpy(&env_desc.env_col[0], &env_.sky_col[0], 3 * sizeof(float));
            ray_scene_->SetEnvironment(env_desc);
        }

        // Add camera
        {   Ray::camera_desc_t cam_desc;
            cam_desc.type = Ray::Persp;
            cam_desc.filter = Ray::Tent;
            cam_desc.origin[0] = cam_desc.origin[1] = cam_desc.origin[2] = 0.0f;
            cam_desc.fwd[0] = cam_desc.fwd[1] = 0.0f;
            cam_desc.fwd[2] = -1.0f;
            cam_desc.fov = 45.0f;
            cam_desc.gamma = 2.2f;
            cam_desc.focus_distance = 1.0f;
            cam_desc.focus_factor = 0.0f;

            ray_scene_->AddCamera(cam_desc);
        }

        // Add sun lamp
        if (Ren::Dot(env_.sun_dir, env_.sun_dir) > 0.00001f && Ren::Dot(env_.sun_col, env_.sun_col) > 0.00001f){
            Ray::light_desc_t sun_desc;
            sun_desc.type = Ray::DirectionalLight;

            memcpy(&sun_desc.direction[0], &env_.sun_dir[0], 3 * sizeof(float));
            memcpy(&sun_desc.color[0], &env_.sun_col[0], 3 * sizeof(float));

            sun_desc.angle = env_.sun_softness;

            ray_scene_->AddLight(sun_desc);
        }

        // Add objects
        for (const auto &o : objects_) {
            const uint32_t drawable_flags = HasMesh | HasTransform;
            if ((o.flags & drawable_flags) == drawable_flags) {

            }
        }
    }

    const auto &rect = ray_reg_ctx_.rect();
    if (rect.w != ctx_.w() || rect.h != ctx_.h()) {
        ray_reg_ctx_ = Ray::RegionContext{ { 0, 0, ctx_.w(), ctx_.h() } };
        ray_renderer_.Resize(ctx_.w(), ctx_.h());
    }

    ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_);

    const auto *pixels = ray_renderer_.get_pixels_ref();
    renderer_.BlitPixels(pixels, ctx_.w(), ctx_.h(), Ren::RawRGBA32F);
}
