#include "SceneManager.h"

#include <map>

#include <Ren/Context.h>
#include <Sys/AssetFileIO.h>

#include "Renderer.h"

void SceneManager::Draw_PT() {
    if (!ray_scene_) return;

    const auto &rect = ray_reg_ctx_.rect();
    if (rect.w != ctx_.w() || rect.h != ctx_.h()) {
        ray_reg_ctx_ = Ray::RegionContext{ { 0, 0, ctx_.w(), ctx_.h() } };
        ray_renderer_.Resize(ctx_.w(), ctx_.h());
    }

    //ray_scene_->set_current_cam(1);

    ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_);

    const auto *pixels = ray_renderer_.get_pixels_ref();
    renderer_.BlitPixels(pixels, ctx_.w(), ctx_.h(), Ren::RawRGBA32F);
}

void SceneManager::PrepareLightmaps_PT() {
    if (!ray_scene_) return;
}

void SceneManager::InitScene_PT(bool _override) {
    if (ray_scene_) {
        if (_override) {
            ray_scene_ = nullptr;
        } else {
            return;
        }
    }

    ray_scene_ = ray_renderer_.CreateScene();

    // Setup environment
    {   Ray::environment_desc_t env_desc;
        memcpy(&env_desc.env_col[0], &env_.sky_col[0], 3 * sizeof(float));
        ray_scene_->SetEnvironment(env_desc);
    }

    // Add main camera
    {   Ray::camera_desc_t cam_desc;
        cam_desc.type = Ray::Persp;
        cam_desc.filter = Ray::Tent;
        cam_desc.origin[0] = cam_desc.origin[1] = cam_desc.origin[2] = 0.0f;
        cam_desc.fwd[0] = cam_desc.fwd[1] = 0.0f;
        cam_desc.fwd[2] = -1.0f;
        cam_desc.fov = cam_.angle();
        cam_desc.gamma = 2.2f;
        cam_desc.focus_distance = 1.0f;
        cam_desc.focus_factor = 0.0f;

        ray_scene_->AddCamera(cam_desc);
    }

    // Add camera for lightmapping
    {   Ray::camera_desc_t cam_desc;
        cam_desc.type = Ray::Geo;
        cam_desc.filter = Ray::Box;
        cam_desc.gamma = 1.0f;
        cam_desc.lighting_only = true;
        cam_desc.skip_indirect_lighting = true;
        cam_desc.uv_index = 1;

        ray_scene_->AddCamera(cam_desc);
    }

    // Add sun lamp
    if (Ren::Dot(env_.sun_dir, env_.sun_dir) > 0.00001f && Ren::Dot(env_.sun_col, env_.sun_col) > 0.00001f){
        Ray::light_desc_t sun_desc;
        sun_desc.type = Ray::DirectionalLight;

        sun_desc.direction[0] = -env_.sun_dir[0];
        sun_desc.direction[1] = -env_.sun_dir[1];
        sun_desc.direction[2] = -env_.sun_dir[2];

        memcpy(&sun_desc.color[0], &env_.sun_col[0], 3 * sizeof(float));

        sun_desc.angle = env_.sun_softness;

        ray_scene_->AddLight(sun_desc);
    }

    std::map<std::string, uint32_t> loaded_materials, loaded_meshes, loaded_textures;

    uint32_t default_white_tex;

    {   //  Add default white texture
        Ray::pixel_color8_t white = { 255, 255, 255, 255 };

        Ray::tex_desc_t tex_desc;
        tex_desc.data = &white;
        tex_desc.w = tex_desc.h = 1;
        tex_desc.generate_mipmaps = true;

        default_white_tex = ray_scene_->AddTexture(tex_desc);
    }

    uint32_t default_glow_mat;

    {
        Ray::mat_desc_t mat_desc;
        mat_desc.type = Ray::EmissiveMaterial;
        mat_desc.main_texture = default_white_tex;
        mat_desc.main_color[0] = 1.0f;
        mat_desc.main_color[1] = 0.0f;
        mat_desc.main_color[2] = 0.0f;

        default_glow_mat = ray_scene_->AddMaterial(mat_desc);
    }

    // Add objects
    for (const auto &obj : objects_) {
        const uint32_t drawable_flags = HasMesh | HasTransform;
        if ((obj.flags & drawable_flags) == drawable_flags) {
            const auto *mesh = obj.mesh.get();
            const char *mesh_name = mesh->name();

            auto mesh_it = loaded_meshes.find(mesh_name);
            if (mesh_it == loaded_meshes.end()) {
                Ray::mesh_desc_t mesh_desc;
                mesh_desc.prim_type = Ray::TriangleList;
                mesh_desc.layout = Ray::PxyzNxyzBxyzTuvTuv;
                mesh_desc.vtx_attrs = (const float *)mesh->attribs();
                mesh_desc.vtx_attrs_count = (uint32_t)(mesh->attribs_size() / (13 * sizeof(float)));
                mesh_desc.vtx_indices = (const uint32_t *)mesh->indices();
                mesh_desc.vtx_indices_count = (uint32_t)(mesh->indices_size() / sizeof(uint32_t));

                const Ren::TriStrip *s = &mesh->strip(0);
                while (s->offset != -1) {
                    const auto *mat = s->mat.get();
                    const char *mat_name = mat->name();

                    auto mat_it = loaded_materials.find(mat_name);
                    if (mat_it == loaded_materials.end()) {
                        Ray::mat_desc_t mat_desc;
                        mat_desc.type = Ray::DiffuseMaterial;
                        mat_desc.main_color[0] = mat_desc.main_color[1] = mat_desc.main_color[2] = 1.0f;

                        auto tex_ref = mat->texture(0);
                        if (tex_ref) {
                            const char *tex_name = tex_ref->name();

                            auto tex_it = loaded_textures.find(tex_name);
                            if (tex_it == loaded_textures.end()) {
                                auto params = tex_ref->params();

                                std::unique_ptr<Ray::pixel_color8_t[]> tex_data(new Ray::pixel_color8_t[params.w * params.h]);
                                tex_ref->ReadTextureData(Ren::RawRGBA8888, (void *)&tex_data[0]);

                                Ray::tex_desc_t tex_desc;
                                tex_desc.w = params.w;
                                tex_desc.h = params.h;
                                tex_desc.data = &tex_data[0];
                                tex_desc.generate_mipmaps = true;

                                uint32_t new_tex = ray_scene_->AddTexture(tex_desc);
                                tex_it = loaded_textures.emplace(tex_name, new_tex).first;
                            }

                            mat_desc.main_texture = tex_it->second;
                        }

                        uint32_t new_mat = ray_scene_->AddMaterial(mat_desc);
                        mat_it = loaded_materials.emplace(mat_name, new_mat).first;
                    }

                    mesh_desc.shapes.emplace_back(mat_it->second, (uint32_t)default_glow_mat, (size_t)(s->offset / sizeof(uint32_t)), (size_t)s->num_indices);
                    ++s;
                }

                uint32_t new_mesh = ray_scene_->AddMesh(mesh_desc);
                mesh_it = loaded_meshes.emplace(mesh_name, new_mesh).first;
            }

            const auto *tr = obj.tr.get();

            ray_scene_->AddMeshInstance(mesh_it->second, Ren::ValuePtr(tr->mat));
        }
    }
}

void SceneManager::SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up) {
    if (!ray_scene_) return;

    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(0, cam_desc);

    auto fwd = Ren::Normalize(target - origin);

    memcpy(&cam_desc.origin[0], Ren::ValuePtr(origin), 3 * sizeof(float));
    memcpy(&cam_desc.fwd[0], Ren::ValuePtr(fwd), 3 * sizeof(float));

    ray_scene_->SetCamera(0, cam_desc);
}

void SceneManager::Clear_PT() {
    if (!ray_scene_) return;

    ray_reg_ctx_.Clear();
    ray_renderer_.Clear();
}