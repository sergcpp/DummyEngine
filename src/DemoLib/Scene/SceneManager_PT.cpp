#include "SceneManager.h"

#include <fstream>
#include <map>

#include <Ren/Context.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Log.h>

#include "Renderer.h"

namespace SceneManagerInternal {
    void WriteTGA(const std::vector<uint8_t> &out_data, int w, int h, const std::string &name) {
        int bpp = 4;

        std::ofstream file(name, std::ios::binary);

        unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        header[12] = w & 0xFF;
        header[13] = (w >> 8) & 0xFF;
        header[14] = (h) & 0xFF;
        header[15] = (h >> 8) & 0xFF;
        header[16] = bpp * 8;

        file.write((char *)&header[0], sizeof(unsigned char) * 18);
        file.write((const char *)&out_data[0], w * h * bpp);

        static const char footer[26] = "\0\0\0\0" // no extension area
            "\0\0\0\0"// no developer directory
            "TRUEVISION-XFILE"// yep, this is a TGA file
            ".";
        file.write((const char *)&footer, sizeof(footer));
    }
}

void SceneManager::Draw_PT() {
    if (!ray_scene_) return;

    const auto &rect = ray_reg_ctx_.rect();
    if (rect.w != ctx_.w() || rect.h != ctx_.h()) {
        ray_reg_ctx_ = Ray::RegionContext{ { 0, 0, ctx_.w(), ctx_.h() } };
        ray_renderer_.Resize(ctx_.w(), ctx_.h());
    }

    // main view camera
    ray_scene_->set_current_cam(0);

    ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_);

    const auto *pixels = ray_renderer_.get_pixels_ref();
    renderer_.BlitPixels(pixels, ctx_.w(), ctx_.h(), Ren::RawRGBA32F);
}

void SceneManager::ResetLightmaps_PT() {
    if (!ray_scene_) return;

    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(1, cam_desc);

    for (size_t i = 0; i < objects_.size(); i++) {
        if (objects_[i].flags & UseLightmap) {
            cur_lm_obj_ = i;
            cam_desc.mi_index = objects_[i].pt_mi;
            break;
        }
    }

    cam_desc.skip_direct_lighting = false;
    cam_desc.skip_indirect_lighting = true;

    ray_scene_->SetCamera(1, cam_desc);

    cur_lm_indir_ = false;
}

bool SceneManager::PrepareLightmaps_PT() {
    if (!ray_scene_) return false;

    const int LM_RES = 1024;
    const int LM_SAMPLES = 256;

    const auto &rect = ray_reg_ctx_.rect();
    if (rect.w != LM_RES || rect.h != LM_RES) {
        ray_reg_ctx_ = Ray::RegionContext{ { 0, 0, LM_RES, LM_RES } };
        ray_renderer_.Resize(LM_RES, LM_RES);
    }

    // special lightmap camera
    ray_scene_->set_current_cam(1);

    if (ray_reg_ctx_.iteration >= LM_SAMPLES) {

        {   // Save lightmap to file
            const auto *pixels = ray_renderer_.get_pixels_ref();

            std::vector<Ray::pixel_color_t> temp_pixels1{ pixels, pixels + LM_RES * LM_RES },
                                            temp_pixels2{ LM_RES * LM_RES };

            const float INVAL_THRES = 0.5f;

            // apply dilation filter
            for (int i = 0; i < 16; i++) {
                bool has_invalid = false;

                for (int y = 0; y < LM_RES; y++) {
                    for (int x = 0; x < LM_RES; x++) {
                        auto in_p = temp_pixels1[y * LM_RES + x];
                        auto &out_p = temp_pixels2[y * LM_RES + x];

                        float mul = 1.0f;
                        if (in_p.a < INVAL_THRES) {
                            has_invalid = true;

                            Ray::pixel_color_t new_p = { 0 };
                            int count = 0;
                            for (int _y : { y - 1, y, y + 1 }) {
                                for (int _x : { x - 1, x, x + 1 }) {
                                    if (_x < 0 || _y < 0
                                        || _x > LM_RES - 1 || _y > LM_RES - 1) continue;

                                    const auto &p = temp_pixels1[_y * LM_RES + _x];
                                    if (p.a >= INVAL_THRES) {
                                        new_p.r += p.r;
                                        new_p.g += p.g;
                                        new_p.b += p.b;
                                        new_p.a += p.a;

                                        count++;
                                    }
                                }
                            }

                            if (count) {
                                float inv_c = 1.0f / count;
                                new_p.r *= inv_c;
                                new_p.g *= inv_c;
                                new_p.b *= inv_c;
                                new_p.a *= inv_c;

                                in_p = new_p;
                            }
                        } else {
                            mul = 1.0f / in_p.a;
                        }

                        out_p.r = in_p.r * mul;
                        out_p.g = in_p.g * mul;
                        out_p.b = in_p.b * mul;
                        out_p.a = in_p.a * mul;
                    }
                }

                std::swap(temp_pixels1, temp_pixels2);

                if (!has_invalid) break;
            }

            std::vector<uint8_t> out_rgba;
            out_rgba.resize(4 * LM_RES * LM_RES);

            for (int y = 0; y < LM_RES; y++) {
                for (int x = 0; x < LM_RES; x++) {
                    const auto &p = temp_pixels1[y * LM_RES + x];

                    uint8_t r = p.r > 1.0f ? 255 : uint8_t(p.r * 255);
                    uint8_t g = p.g > 1.0f ? 255 : uint8_t(p.g * 255);
                    uint8_t b = p.b > 1.0f ? 255 : uint8_t(p.b * 255);
                    uint8_t a = p.a > 1.0f ? 255 : uint8_t(p.a * 255);

                    out_rgba[4 * (y * LM_RES + x) + 0] = b;
                    out_rgba[4 * (y * LM_RES + x) + 1] = g;
                    out_rgba[4 * (y * LM_RES + x) + 2] = r;
                    out_rgba[4 * (y * LM_RES + x) + 3] = a;
                }
            }

            std::string out_file_name = scene_name_;
            out_file_name += "_";
            out_file_name += std::to_string(cur_lm_obj_);
            if (!cur_lm_indir_) {
                out_file_name += "_lm_direct.tga";
            } else {
                out_file_name += "_lm_indirect.tga";
            }

            out_file_name = std::string("assets/textures/lightmaps/") + out_file_name;

            SceneManagerInternal::WriteTGA(out_rgba, LM_RES, LM_RES, out_file_name);

            //std::ofstream out_file(std::string("assets/textures/lightmaps/") + out_file_name, std::ios::binary);
            //out_file.write(out_file_name.c_str(), out_file_name.length());
        }

        Ray::camera_desc_t cam_desc;
        ray_scene_->GetCamera(1, cam_desc);

        if (!cur_lm_indir_) {
            cur_lm_indir_ = true;

            cam_desc.skip_direct_lighting = true;
            cam_desc.skip_indirect_lighting = false;
        } else {
            bool found = false;

            for (size_t i = cur_lm_obj_ + 1; i < objects_.size(); i++) {
                if (objects_[i].flags & UseLightmap) {
                    cur_lm_obj_ = i;
                    cam_desc.mi_index = objects_[i].pt_mi;
                    found = true;
                    break;
                }
            }

            if (!found) {
                return false;
            }
        }

        ray_scene_->SetCamera(1, cam_desc);

        ray_reg_ctx_.Clear();
    }

    ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_);

    LOGI("Lightmap: %i %i", int(cur_lm_obj_), ray_reg_ctx_.iteration);

    const auto *pixels = ray_renderer_.get_pixels_ref();
    renderer_.BlitPixels(pixels, LM_RES, LM_RES, Ren::RawRGBA32F);

    return true;
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
        cam_desc.skip_direct_lighting = true;
        //cam_desc.skip_indirect_lighting = true;
        cam_desc.no_background = true;
        cam_desc.uv_index = 1;
        cam_desc.mi_index = 0;

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
    for (auto &obj : objects_) {
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

            obj.pt_mi = ray_scene_->AddMeshInstance(mesh_it->second, Ren::ValuePtr(tr->mat));
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