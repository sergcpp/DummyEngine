#include "SceneManager.h"

#include <map>

#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../Renderer/Renderer.h"
#include "../Utils/Load.h"

namespace SceneManagerConstants {
extern const char *MODELS_PATH;
extern const char *TEXTURES_PATH;
extern const char *MATERIALS_PATH;
extern const char *SHADERS_PATH;

extern const int LIGHTMAP_ATLAS_RESX, LIGHTMAP_ATLAS_RESY;
} // namespace SceneManagerConstants

namespace SceneManagerInternal {
bool Write_RGB(const Ray::pixel_color_t *out_data, int w, int h, const char *name);
bool Write_RGBM(const float *out_data, int w, int h, int channels, bool flip_y, const char *name);
bool Write_RGBE(const Ray::pixel_color_t *out_data, int w, int h, const char *name);

void LoadTGA(Sys::AssetFile &in_file, int w, int h, Ray::color_rgba8_t *out_data);

std::vector<Ray::pixel_color_t> FlushSeams(const Ray::pixel_color_t *pixels, int width, int height,
                                           float invalid_threshold, int filter_size);

std::unique_ptr<Ray::color_rgba8_t[]> GetTextureData(const Ren::Tex2DRef &tex_ref, bool flip_y);
} // namespace SceneManagerInternal

const float *SceneManager::Draw_PT(int *w, int *h) {
    if (!ray_scene_)
        return nullptr;

    if (ray_reg_ctx_.empty()) {
        // if (ray_renderer_.type() == Ray::RendererOCL) {
        if (0) {
            ray_reg_ctx_.emplace_back(Ray::rect_t{0, 0, ren_ctx_.w(), ren_ctx_.h()});
        } else {
            const int TILE_SIZE = 64;

#if defined(__ANDROID__)
            const int pt_res_w = 640, pt_res_h = 360;
#else
            const int pt_res_w = ren_ctx_.w(), pt_res_h = ren_ctx_.h();
#endif

            for (int y = 0; y < pt_res_h + TILE_SIZE - 1; y += TILE_SIZE) {
                for (int x = 0; x < pt_res_w + TILE_SIZE - 1; x += TILE_SIZE) {
                    auto rect = Ray::rect_t{x, y, std::min(TILE_SIZE, pt_res_w - x), std::min(TILE_SIZE, pt_res_h - y)};
                    if (rect.w > 0 && rect.h > 0) {
                        ray_reg_ctx_.emplace_back(rect);
                    }
                }
            }

            ray_renderer_.Resize(pt_res_w, pt_res_h);
        }
    }

    // main view camera
    ray_scene_->set_current_cam(Ray::CameraHandle{0});

    // if (ray_renderer_.type() == Ray::RendererOCL) {
    if (0) {
        // ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_[0]);
    } else {
        auto render_task = [this](int i) { ray_renderer_.RenderScene(ray_scene_.get(), ray_reg_ctx_[i]); };
        std::vector<std::future<void>> ev(ray_reg_ctx_.size());
        for (int i = 0; i < (int)ray_reg_ctx_.size(); i++) {
            ev[i] = threads_.Enqueue(render_task, i);
        }
        for (const std::future<void> &e : ev) {
            e.wait();
        }
    }

    std::tie(*w, *h) = ray_renderer_.size();

    const Ray::pixel_color_t *pixels = ray_renderer_.get_pixels_ref();
    return &pixels[0].r;
}

void SceneManager::ResetLightmaps_PT() {
    if (!ray_scene_)
        return;

    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(Ray::CameraHandle{1}, cam_desc);

    for (uint32_t i = 0; i < (uint32_t)scene_data_.objects.size(); i++) {
        if (scene_data_.objects[i].comp_mask & CompLightmapBit) {
            const auto *tr = (Transform *)scene_data_.comp_store[CompTransform]->Get(
                scene_data_.objects[i].components[CompTransform]);
            const auto *lm =
                (Lightmap *)scene_data_.comp_store[CompLightmap]->Get(scene_data_.objects[i].components[CompLightmap]);

            cur_lm_obj_ = i;
            cam_desc.mi_index = tr->pt_mi;
            break;
        }
    }

    cam_desc.skip_direct_lighting = false;
    cam_desc.skip_indirect_lighting = true;

    ray_scene_->SetCamera(Ray::CameraHandle{1}, cam_desc);

    cur_lm_indir_ = false;
}

bool SceneManager::PrepareLightmaps_PT(const float **preview_pixels, int *w, int *h) {
    using namespace SceneManagerConstants;

    if (!ray_scene_) {
        return false;
    }

    const int LmSamplesDirect =
#ifdef NDEBUG
        64;
#else
        16;
#endif

    const int LmSamplesIndirect =
#ifdef NDEBUG
        16 * 4096;
#else
        64;
#endif
    const int LmSamplesPerPass = 16;
    const int TileSizeCPU = 64;

    const SceneObject &cur_obj = scene_data_.objects[cur_lm_obj_];
    const auto *lm = (Lightmap *)scene_data_.comp_store[CompLightmap]->Get(cur_obj.components[CompLightmap]);
    const int res = lm->size[0];

    if (ray_reg_ctx_.empty()) {
        // if (ray_renderer_.type() == Ray::RendererOCL) {
        if (0) {
            ray_reg_ctx_.emplace_back(Ray::rect_t{0, 0, res, res});
            ray_renderer_.Resize(res, res);
        } else {
            for (int y = 0; y < res + TileSizeCPU - 1; y += TileSizeCPU) {
                for (int x = 0; x < res + TileSizeCPU - 1; x += TileSizeCPU) {
                    auto rect = Ray::rect_t{x, y, std::min(TileSizeCPU, res - x), std::min(TileSizeCPU, res - y)};
                    if (rect.w > 0 && rect.h > 0) {
                        ray_reg_ctx_.emplace_back(rect);
                    }
                }
            }
        }
    }

    const std::pair<int, int> &cur_size = ray_renderer_.size();

    if (cur_size.first != res || cur_size.second != res) {
        // if (ray_renderer_.type() == Ray::RendererOCL) {
        if (0) {
            ray_reg_ctx_[0] = Ray::RegionContext{{0, 0, res, res}};
        } else {
            ray_reg_ctx_.clear();

            for (int y = 0; y < res + TileSizeCPU - 1; y += TileSizeCPU) {
                for (int x = 0; x < res + TileSizeCPU - 1; x += TileSizeCPU) {
                    auto rect = Ray::rect_t{x, y, std::min(TileSizeCPU, res - x), std::min(TileSizeCPU, res - y)};
                    if (rect.w > 0 && rect.h > 0) {
                        ray_reg_ctx_.emplace_back(rect);
                    }
                }
            }
        }
        ray_renderer_.Resize(res, res);
    }

    // special lightmap camera
    ray_scene_->set_current_cam(Ray::CameraHandle{1});

    const float InvalidThreshold = 0.5f;

    if ((!cur_lm_indir_ && ray_reg_ctx_[0].iteration >= LmSamplesDirect) ||
        (cur_lm_indir_ && ray_reg_ctx_[0].iteration >= LmSamplesIndirect)) {
        { // Save lightmap to file
            const Ray::pixel_color_t *pixels = ray_renderer_.get_pixels_ref();

            const int xpos = lm->pos[0], ypos = lm->pos[1];

            // Copy image to lightmap atlas
            Ray::pixel_color_t *pt_lm_target = cur_lm_indir_ ? pt_lm_indir_.data() : pt_lm_direct_.data();
            for (int j = 0; j < res; j++) {
                memcpy(&pt_lm_target[(ypos + j) * LIGHTMAP_ATLAS_RESX + xpos], &pixels[j * res],
                       res * sizeof(Ray::pixel_color_t));
            }

            if (cur_lm_indir_) {
                std::vector<Ray::shl1_data_t> sh_data(ray_renderer_.get_sh_data_ref(),
                                                      ray_renderer_.get_sh_data_ref() + res * res);
                std::vector<Ray::pixel_color_t> temp_pixels1(res * res, Ray::pixel_color_t{0.0f, 0.0f, 0.0f, 1.0f});

                for (int i = 0; i < res * res; i++) {
                    const float coverage = pixels[i].a;
                    if (coverage < InvalidThreshold) {
                        continue;
                    }

                    sh_data[i].coeff_r[0] /= 2.0f * coverage;
                    sh_data[i].coeff_g[0] /= 2.0f * coverage;
                    sh_data[i].coeff_b[0] /= 2.0f * coverage;

                    for (int sh_l = 1; sh_l < 4; sh_l++) {
                        sh_data[i].coeff_r[sh_l] /= 2.0f * coverage;
                        sh_data[i].coeff_g[sh_l] /= 2.0f * coverage;
                        sh_data[i].coeff_b[sh_l] /= 2.0f * coverage;

                        if (sh_data[i].coeff_r[0] > std::numeric_limits<float>::epsilon()) {
                            sh_data[i].coeff_r[sh_l] /= sh_data[i].coeff_r[0];
                        }
                        if (sh_data[i].coeff_g[0] > std::numeric_limits<float>::epsilon()) {
                            sh_data[i].coeff_g[sh_l] /= sh_data[i].coeff_g[0];
                        }
                        if (sh_data[i].coeff_b[0] > std::numeric_limits<float>::epsilon()) {
                            sh_data[i].coeff_b[sh_l] /= sh_data[i].coeff_b[0];
                        }

                        sh_data[i].coeff_r[sh_l] = 0.5f * sh_data[i].coeff_r[sh_l] + 0.5f;
                        sh_data[i].coeff_g[sh_l] = 0.5f * sh_data[i].coeff_g[sh_l] + 0.5f;
                        sh_data[i].coeff_b[sh_l] = 0.5f * sh_data[i].coeff_b[sh_l] + 0.5f;

                        sh_data[i].coeff_r[sh_l] = Ren::Clamp(sh_data[i].coeff_r[sh_l], 0.0f, 1.0f);
                        sh_data[i].coeff_g[sh_l] = Ren::Clamp(sh_data[i].coeff_g[sh_l], 0.0f, 1.0f);
                        sh_data[i].coeff_b[sh_l] = Ren::Clamp(sh_data[i].coeff_b[sh_l], 0.0f, 1.0f);
                    }
                }

                // Fill alpha channel with pixel 'validity'
                for (int i = 0; i < res * res; i++) {
                    // Coverage division is already applied in previous step
                    if (pixels[i].a > InvalidThreshold) {
                        temp_pixels1[i].a = 1.0f;
                    } else {
                        // Mark pixel as invalid, so it is be processed during dilate
                        temp_pixels1[i].a = 0.0f;
                    }
                }

                for (int sh_l = 0; sh_l < 4; sh_l++) {
                    for (int i = 0; i < res * res; i++) {
                        temp_pixels1[i].r = sh_data[i].coeff_r[sh_l];
                        temp_pixels1[i].g = sh_data[i].coeff_g[sh_l];
                        temp_pixels1[i].b = sh_data[i].coeff_b[sh_l];
                    }

                    // Add image to atlas
                    for (int j = 0; j < res; j++) {
                        memcpy(&pt_lm_indir_sh_[sh_l][(ypos + j) * LIGHTMAP_ATLAS_RESX + xpos], &temp_pixels1[j * res],
                               res * sizeof(Ray::pixel_color_t));
                    }
                }
            }
        }

        Ray::camera_desc_t cam_desc;
        ray_scene_->GetCamera(Ray::CameraHandle{1}, cam_desc);

        if (!cur_lm_indir_) {
            cur_lm_indir_ = true;

            cam_desc.skip_direct_lighting = true;
            cam_desc.skip_indirect_lighting = false;
            cam_desc.output_sh = true;
        } else {
            bool found = false;

            for (uint32_t i = cur_lm_obj_ + 1; i < (uint32_t)scene_data_.objects.size(); i++) {
                if (scene_data_.objects[i].comp_mask & CompLightmapBit) {
                    const auto *tr = (Transform *)scene_data_.comp_store[CompTransform]->Get(
                        scene_data_.objects[i].components[CompTransform]);

                    cur_lm_obj_ = i;
                    cam_desc.mi_index = tr->pt_mi;
                    found = true;
                    break;
                }
            }

            if (!found) {
                const int FilterSize = 32;

                { // Save direct lightmap
                    ren_ctx_.log()->Info("Flushing seams...");
                    const double t1 = Sys::GetTimeS();
                    const std::vector<Ray::pixel_color_t> out_pixels = SceneManagerInternal::FlushSeams(
                        &pt_lm_direct_[0], LIGHTMAP_ATLAS_RESX, LIGHTMAP_ATLAS_RESY, InvalidThreshold, FilterSize);
                    ren_ctx_.log()->Info("                 done (%fs)", Sys::GetTimeS() - t1);

                    std::string out_file_name = "./assets/textures/lightmaps/";
                    out_file_name += scene_data_.name.c_str();
                    out_file_name += "_lm_direct.png";

                    if (!SceneManagerInternal::Write_RGBM(&out_pixels[0].r, LIGHTMAP_ATLAS_RESX, LIGHTMAP_ATLAS_RESY, 4,
                                                          false /* flip_y */, out_file_name.c_str())) {
                        ren_ctx_.log()->Error("Failed to write %s", out_file_name.c_str());
                        return false;
                    }
                }

                { // Save indirect lightmap
                    ren_ctx_.log()->Info("Flushing seams...");
                    const double t1 = Sys::GetTimeS();
                    const std::vector<Ray::pixel_color_t> out_pixels = SceneManagerInternal::FlushSeams(
                        &pt_lm_indir_[0], LIGHTMAP_ATLAS_RESX, LIGHTMAP_ATLAS_RESY, InvalidThreshold, FilterSize);
                    ren_ctx_.log()->Info("                 done (%fs)", Sys::GetTimeS() - t1);

                    std::string out_file_name = "./assets/textures/lightmaps/";
                    out_file_name += scene_data_.name.c_str();
                    out_file_name += "_lm_indirect.png";

                    if (!SceneManagerInternal::Write_RGBM(&out_pixels[0].r, LIGHTMAP_ATLAS_RESX, LIGHTMAP_ATLAS_RESY, 4,
                                                          false /* flip_y */, out_file_name.c_str())) {
                        ren_ctx_.log()->Error("Failed to write %s", out_file_name.c_str());
                        return false;
                    }
                }

                { // Save indirect SH-lightmap
                    for (int sh_l = 0; sh_l < 4; sh_l++) {
                        ren_ctx_.log()->Info("Flushing seams...");
                        const double t1 = Sys::GetTimeS();
                        std::vector<Ray::pixel_color_t> out_pixels =
                            SceneManagerInternal::FlushSeams(&pt_lm_indir_sh_[sh_l][0], LIGHTMAP_ATLAS_RESX,
                                                             LIGHTMAP_ATLAS_RESY, InvalidThreshold, FilterSize);
                        ren_ctx_.log()->Info("                 done (%fs)", Sys::GetTimeS() - t1);

                        std::string out_file_name = "./assets/textures/lightmaps/";
                        out_file_name += scene_data_.name.c_str();
                        out_file_name += "_lm_sh_";
                        out_file_name += std::to_string(sh_l);
                        out_file_name += ".png";

                        if (sh_l == 0) {
                            // Save first band as HDR
                            if (!SceneManagerInternal::Write_RGBM(&out_pixels[0].r, LIGHTMAP_ATLAS_RESX,
                                                                  LIGHTMAP_ATLAS_RESY, 4, false /* flip_y */,
                                                                  out_file_name.c_str())) {
                                ren_ctx_.log()->Error("Failed to write %s", out_file_name.c_str());
                                return false;
                            }
                        } else {
                            // Save rest as LDR
                            if (!SceneManagerInternal::Write_RGB(&out_pixels[0], LIGHTMAP_ATLAS_RESX,
                                                                 LIGHTMAP_ATLAS_RESY, out_file_name.c_str())) {
                                ren_ctx_.log()->Error("Failed to write %s", out_file_name.c_str());
                                return false;
                            }
                        }
                    }
                }

                // Release memory
                pt_lm_direct_ = {};
                pt_lm_indir_ = {};
                for (int sh_l = 0; sh_l < 4; sh_l++) {
                    pt_lm_indir_sh_[sh_l] = {};
                }

                return false;
            }

            cur_lm_indir_ = false;
            cam_desc.skip_direct_lighting = false;
            cam_desc.skip_indirect_lighting = true;
            cam_desc.output_sh = false;
        }

        ray_scene_->SetCamera(Ray::CameraHandle{1}, cam_desc);

        ray_renderer_.Clear({});
        for (Ray::RegionContext &c : ray_reg_ctx_) {
            c.Clear();
        }
    }

    if (!ray_reg_ctx_[0].iteration) {
        // This is first iteration
        pt_lm_started_time_s_ = Sys::GetTimeS();
    }

    for (int i = 0; i < LmSamplesPerPass; i++) {
        // if (ray_renderer_.type() == Ray::RendererOCL) {
        if (0) {
            // ray_renderer_.RenderScene(ray_scene_, ray_reg_ctx_[0]);
        } else {
            auto render_task = [this](int i) { ray_renderer_.RenderScene(ray_scene_.get(), ray_reg_ctx_[i]); };
            std::vector<std::future<void>> ev(ray_reg_ctx_.size());
            for (int i = 0; i < (int)ray_reg_ctx_.size(); i++) {
                ev[i] = threads_.Enqueue(render_task, i);
            }
            for (const std::future<void> &e : ev) {
                e.wait();
            }
        }
    }

    const double seconds_per_iteration = (Sys::GetTimeS() - pt_lm_started_time_s_) / double(ray_reg_ctx_[0].iteration);

    const int SamplesDone = ray_reg_ctx_[0].iteration;
    const int SamplesTotal = cur_lm_indir_ ? LmSamplesIndirect : LmSamplesDirect;

    ren_ctx_.log()->Info("Lightmap: %i %i/%i (%.1fs left)", int(cur_lm_obj_), SamplesDone, SamplesTotal,
                         seconds_per_iteration * (SamplesTotal - SamplesDone));

    const Ray::pixel_color_t *pixels = ray_renderer_.get_pixels_ref();
    *preview_pixels = &pixels[0].r;
    *w = res;
    *h = res;

    return true;
}

void SceneManager::InitScene_PT(bool _override) {
    using namespace SceneManagerConstants;

    if (ray_scene_) {
        if (_override) {
            ray_scene_ = nullptr;
        } else {
            return;
        }
    }

    ray_scene_.reset(ray_renderer_.CreateScene());
    ray_reg_ctx_.clear();

    { // Setup environment
        Ray::environment_desc_t env_desc;
        env_desc.env_col[0] = env_desc.env_col[1] = env_desc.env_col[2] = 1.0f;

        if (!scene_data_.env.env_map_name_pt.empty()) {
            std::string env_map_path = "./assets/textures/";
            env_map_path += scene_data_.env.env_map_name_pt.c_str();

            int w, h;
            const std::vector<uint8_t> tex_data = LoadHDR(env_map_path.c_str(), w, h);

            Ray::tex_desc_t tex_desc;
            tex_desc.data = (const Ray::color_rgba8_t *)&tex_data[0];
            tex_desc.w = w;
            tex_desc.h = h;
            tex_desc.generate_mipmaps = false;

            env_desc.env_map = ray_scene_->AddTexture(tex_desc);
        }

        ray_scene_->SetEnvironment(env_desc);
    }

    { // Add main camera
        Ray::camera_desc_t cam_desc;
        cam_desc.type = Ray::Persp;
        cam_desc.dtype = Ray::None;
        cam_desc.filter = Ray::Tent;
        cam_desc.origin[0] = cam_desc.origin[1] = cam_desc.origin[2] = 0.0f;
        cam_desc.fwd[0] = cam_desc.fwd[1] = 0.0f;
        cam_desc.fwd[2] = -1.0f;
        cam_desc.fov = cam_.angle();
        cam_desc.gamma = 1.0f;
        cam_desc.focus_distance = 1.0f;

        ray_scene_->AddCamera(cam_desc);
    }

    { // Add camera for lightmapping
        Ray::camera_desc_t cam_desc;
        cam_desc.type = Ray::Geo;
        cam_desc.dtype = Ray::None;
        cam_desc.filter = Ray::Box;
        cam_desc.gamma = 1.0f;
        cam_desc.lighting_only = true;
        cam_desc.skip_direct_lighting = true;
        // cam_desc.skip_indirect_lighting = true;
        cam_desc.no_background = true;
        cam_desc.uv_index = 1;
        cam_desc.mi_index = 0;
        cam_desc.output_sh = true;
        // cam_desc.use_coherent_sampling = true;

        ray_scene_->AddCamera(cam_desc);
    }

    // Add sun lamp
    if (Ren::Dot(scene_data_.env.sun_dir, scene_data_.env.sun_dir) > 0.00001f &&
        Ren::Dot(scene_data_.env.sun_col, scene_data_.env.sun_col) > 0.00001f) {
        Ray::directional_light_desc_t sun_desc;

        sun_desc.direction[0] = -scene_data_.env.sun_dir[0];
        sun_desc.direction[1] = -scene_data_.env.sun_dir[1];
        sun_desc.direction[2] = -scene_data_.env.sun_dir[2];

        memcpy(&sun_desc.color[0], &scene_data_.env.sun_col[0], 3 * sizeof(float));

        sun_desc.angle = scene_data_.env.sun_softness;

        ray_scene_->AddLight(sun_desc);
    }

    std::map<std::string, Ray::MaterialHandle> loaded_materials;
    std::map<std::string, Ray::MeshHandle> loaded_meshes;
    std::map<std::string, Ray::TextureHandle> loaded_textures;

    Ray::TextureHandle default_white_tex;

    { //  Add default white texture
        const Ray::color_rgba8_t white = {255, 255, 255, 255};

        Ray::tex_desc_t tex_desc;
        tex_desc.data = &white;
        tex_desc.w = tex_desc.h = 1;
        tex_desc.generate_mipmaps = true;

        default_white_tex = ray_scene_->AddTexture(tex_desc);
    }

    (void)default_white_tex;

    /*uint32_t default_glow_mat;

    {
        Ray::mat_desc_t mat_desc;
        mat_desc.type = Ray::EmissiveMaterial;
        mat_desc.main_texture = default_white_tex;
        mat_desc.main_color[0] = 1.0f;
        mat_desc.main_color[1] = 0.0f;
        mat_desc.main_color[2] = 0.0f;

        default_glow_mat = ray_scene_->AddMaterial(mat_desc);
    }*/

    // Add objects
    for (SceneObject &obj : scene_data_.objects) {
        const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
        if ((obj.comp_mask & drawable_flags) == drawable_flags) {
            const auto *dr = (Drawable *)scene_data_.comp_store[CompDrawable]->Get(obj.components[CompDrawable]);
            const Ren::Mesh *mesh = dr->pt_mesh ? dr->pt_mesh.get() : dr->mesh.get();
            if (!(dr->vis_mask & uint32_t(Drawable::eDrVisibility::VisShadow)) ||
                (mesh->type() != Ren::eMeshType::Simple && mesh->type() != Ren::eMeshType::Colored)) {
                continue;
            }

            const char *mesh_name = mesh->name().c_str();
            auto mesh_it = loaded_meshes.find(mesh_name);
            if (mesh_it == loaded_meshes.end()) {
                Ray::mesh_desc_t mesh_desc;
                mesh_desc.prim_type = Ray::TriangleList;
                mesh_desc.layout =
                    (mesh->type() == Ren::eMeshType::Colored) ? Ray::PxyzNxyzBxyzTuv : Ray::PxyzNxyzBxyzTuvTuv;
                mesh_desc.vtx_attrs = (const float *)mesh->attribs();
                mesh_desc.vtx_attrs_count = (uint32_t)(mesh->attribs_buf1().size / 16);
                mesh_desc.vtx_indices = (const uint32_t *)mesh->indices();
                mesh_desc.vtx_indices_count = (uint32_t)(mesh->indices_buf().size / sizeof(uint32_t));
                mesh_desc.base_vertex = 0;

                bool is_sparse = false;

                for (const auto &grp : mesh->groups()) {
                    const Ren::Material *mat = grp.mat.get();
                    if (mat->flags() & (uint32_t(Ren::eMatFlags::AlphaBlend) | uint32_t(Ren::eMatFlags::AlphaTest))) {
                        // TODO: Properly support transparent objects
                        is_sparse = true;
                        continue;
                    }
                    const char *mat_name = mat->name().c_str();

                    auto mat_it = loaded_materials.find(mat_name);
                    if (mat_it == loaded_materials.end()) {
                        Ray::shading_node_desc_t mat_desc;
                        mat_desc.base_color[0] = mat_desc.base_color[1] = mat_desc.base_color[2] = 1.0f;

                        Ren::Tex2DRef tex_ref;

                        // if (mat->flags() & Ren::AlphaBlend) {
                        //    mat_desc.type = Ray::TransparentMaterial;
                        //    tex_ref = mat->texture(0);
                        //} else {
                        mat_desc.type = Ray::DiffuseNode;
                        tex_ref = mat->textures[0];
                        //}

                        if (tex_ref) {
                            const char *tex_name = tex_ref->name().c_str();

                            auto tex_it = loaded_textures.find(tex_name);
                            if (tex_it == loaded_textures.end()) {
                                std::unique_ptr<Ray::color_rgba8_t[]> tex_data =
                                    SceneManagerInternal::GetTextureData(tex_ref, true /* flip_y */);

                                const Ren::Tex2DParams &params = tex_ref->params;

                                Ray::tex_desc_t tex_desc;
                                tex_desc.w = params.w;
                                tex_desc.h = params.h;
                                tex_desc.data = &tex_data[0];
                                tex_desc.generate_mipmaps = true;
                                tex_desc.is_srgb = bool(params.flags & Ren::eTexFlagBits::SRGB);

                                const Ray::TextureHandle new_tex = ray_scene_->AddTexture(tex_desc);
                                tex_it = loaded_textures.emplace(tex_name, new_tex).first;
                            }

                            mat_desc.base_texture = tex_it->second;
                        }

                        const Ray::MaterialHandle new_mat = ray_scene_->AddMaterial(mat_desc);
                        mat_it = loaded_materials.emplace(mat_name, new_mat).first;
                    }

                    mesh_desc.shapes.emplace_back(mat_it->second, Ray::InvalidMaterialHandle,
                                                  size_t(grp.offset / sizeof(uint32_t)), size_t(grp.num_indices));
                }

                if (!mesh_desc.shapes.empty()) {
                    std::unique_ptr<uint32_t[]> compacted_indices;
                    if (is_sparse) {
                        compacted_indices.reset(new uint32_t[mesh_desc.vtx_indices_count]);

                        mesh_desc.vtx_indices_count = 0;
                        for (Ray::shape_desc_t &s : mesh_desc.shapes) {
                            memcpy(&compacted_indices[mesh_desc.vtx_indices_count], &mesh_desc.vtx_indices[s.vtx_start],
                                   s.vtx_count * sizeof(uint32_t));
                            s.vtx_start = mesh_desc.vtx_indices_count;
                            mesh_desc.vtx_indices_count += s.vtx_count;
                        }

                        mesh_desc.vtx_indices = compacted_indices.get();
                    }

                    if (mesh_desc.vtx_indices_count) {
                        const Ray::MeshHandle new_mesh = ray_scene_->AddMesh(mesh_desc);
                        mesh_it = loaded_meshes.emplace(mesh_name, new_mesh).first;
                    }
                }
            }

            if (mesh_it != loaded_meshes.end()) {
                auto *tr = (Transform *)scene_data_.comp_store[CompTransform]->Get(obj.components[CompTransform]);
                tr->pt_mi = ray_scene_->AddMeshInstance(mesh_it->second, ValuePtr(tr->world_from_object))._index;
            }
        }
    }

    pt_lm_direct_.resize(LIGHTMAP_ATLAS_RESX * LIGHTMAP_ATLAS_RESY);
    pt_lm_indir_.resize(LIGHTMAP_ATLAS_RESX * LIGHTMAP_ATLAS_RESY);
    for (int i = 0; i < 4; i++) {
        pt_lm_indir_sh_[i].resize(LIGHTMAP_ATLAS_RESX * LIGHTMAP_ATLAS_RESY);
    }

    for (int j = 0; j < LIGHTMAP_ATLAS_RESY; j++) {
        for (int i = 0; i < LIGHTMAP_ATLAS_RESX; i++) {
            int ndx = j * LIGHTMAP_ATLAS_RESX + i;

            pt_lm_direct_[ndx] = {0.0f, 0.0f, 0.0f, 0.0f};
            pt_lm_indir_[ndx] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (int i = 0; i < 4; i++) {
                pt_lm_indir_sh_[i][ndx] = {0.0f, 0.0f, 0.0f, 0.0f};
            }
        }
    }
}

void SceneManager::SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up, float fov) {
    if (!ray_scene_) {
        return;
    }

    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(Ray::CameraHandle{0}, cam_desc);

    Ren::Vec3f fwd = Normalize(target - origin);

    memcpy(&cam_desc.origin[0], ValuePtr(origin), 3 * sizeof(float));
    memcpy(&cam_desc.fwd[0], ValuePtr(fwd), 3 * sizeof(float));
    memcpy(&cam_desc.up[0], ValuePtr(up), 3 * sizeof(float));

    cam_desc.fov = fov;

    ray_scene_->SetCamera(Ray::CameraHandle{0}, cam_desc);
}

void SceneManager::Clear_PT() {
    if (!ray_scene_) {
        return;
    }

    for (Ray::RegionContext &c : ray_reg_ctx_) {
        c.Clear();
    }
    ray_renderer_.Clear({});
}