#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <Ren/Camera.h>
#include <Ren/RingBuffer.h>
#include <Ren/Span.h>
#include <Sys/AsyncFileReader.h>
#include <Sys/DynLib.h>

#include "SceneData.h"

// TODO: get rid of this dependency!
#include "../renderer/Renderer_Structs.h"

namespace Phy {
struct prim_t;
struct split_settings_t;
} // namespace Phy

namespace Sys {
template <typename T, typename FallBackAllocator> class MultiPoolAllocator;
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char, std::allocator<char>>>;
class ThreadPool;
} // namespace Sys

namespace Snd {
class Context;
}

namespace Eng {
class ShaderLoader;
} // namespace Eng

#include <Sys/Json.h>

namespace SceneManagerInternal {
// TODO: remove this from header file
struct AssetCache {
    Sys::JsObjectP js_db;
    Ren::HashMap32<std::string, uint32_t> texture_averages;

    explicit AssetCache(const Sys::MultiPoolAllocator<char> &mp_alloc) : js_db(mp_alloc) {}

    void WriteTextureAverage(const char *tex_name, const uint8_t average_color[4]) {
        uint32_t color;
        memcpy(&color, average_color, 4);
        texture_averages.Insert(tex_name, color);

        Sys::JsObjectP &js_files = js_db["files"].as_obj();
        const size_t i = js_files.IndexOf(tex_name);
        if (i < js_files.Size()) {
            Sys::JsObjectP &js_file = js_files[i].second.as_obj();
            if (js_file.Has("color")) {
                Sys::JsNumber &js_color = js_file.at("color").as_num();
                js_color.val = double(color);
            } else {
                auto js_color = Sys::JsNumber{double(color)};
                js_file.Insert("color", js_color);
            }
        }
    }
};
} // namespace SceneManagerInternal

namespace Eng {
struct path_config_t {
    const char *models_path = "./assets_pc/models/";
    const char *textures_path = "./assets_pc/textures/";
    const char *materials_path = "./assets_pc/materials/";
    const char *shaders_path = "./assets_pc/shaders/";
};

struct assets_context_t {
    std::string_view platform;
    Ren::ILog *log;
    std::unique_ptr<SceneManagerInternal::AssetCache> cache;
    Sys::MultiPoolAllocator<char> *mp_alloc;
    Sys::ThreadPool *p_threads;
    Sys::DynLib spirv_compiler;
    std::mutex cache_mtx;
};

enum class eAssetBuildFlags : uint32_t { DebugOnly, ReleaseOnly, GLOnly, VKOnly };

struct asset_output_t {
    std::string name;
    Ren::Bitmask<eAssetBuildFlags> flags;
};

class SceneManager {
  public:
    SceneManager(Ren::Context &ren_ctx, Eng::ShaderLoader &sh, Snd::Context *snd_ctx, Sys::ThreadPool &threads,
                 const path_config_t &paths);
    ~SceneManager();

    SceneManager(const SceneManager &rhs) = delete;

    const Ren::Camera &main_cam() const { return cam_; }
    Ren::Camera &main_cam() { return cam_; }
    Ren::Camera &ext_cam() { return ext_cam_; }
    Ren::Mesh *cam_rig() { return cam_rig_.get(); }
    Eng::SceneData &scene_data() { return scene_data_; }
    Snd::Source &ambient_sound() { return amb_sound_; }

    const Eng::PersistentGpuData &persistent_data() const { return scene_data_.persistent_data; }

    void set_tex_memory_limit(const size_t limit) { tex_memory_limit_ = limit; }

    void set_load_flags(const Ren::Bitmask<eSceneLoadFlags> load_flags) { scene_data_.load_flags = load_flags; }

    Eng::SceneObject *GetObject(const uint32_t i) { return &scene_data_.objects[i]; }

    uint32_t FindObject(std::string_view name) {
        uint32_t *p_ndx = scene_data_.name_to_object.Find(name);
        return p_ndx ? (*p_ndx) : 0xffffffff;
    }

    void InvalidateObjects(Ren::Span<const uint32_t> indices, const uint32_t change_mask) {
        for (const uint32_t ndx : indices) {
            scene_data_.objects[ndx].change_mask |= change_mask;
        }
        changed_objects_.insert(changed_objects_.end(), indices.begin(), indices.end());
    }
    void InvalidateTexture(const Ren::TexRef &ref);

    void LoadScene(const Sys::JsObjectP &js_scene, Ren::Bitmask<eSceneLoadFlags> load_flags = SceneLoadAll);
    void SaveScene(Sys::JsObjectP &js_scene);
    void ClearScene();

    void LoadEnvMap();
    void ReleaseEnvMap(bool immediate = false);

    void AllocGICache();
    void ReleaseGICache(bool immediate = false);

    void Alloc_TLAS();
    void Release_TLAS(bool immediate = false);

    void AllocMeshBuffers();
    void LoadMeshBuffers();
    void ReleaseMeshBuffers(bool immediate = false);

    void ReleaseTextures(bool immediate = false);

    void RebuildLightTree();
    void ReleaseLightTree(bool immediate = false);

    void AllocInstanceBuffer();
    void ReleaseInstanceBuffer(bool immediate = false);

    void AllocMaterialsBuffer();
    void ReleaseMaterialsBuffer(bool immediate = false);

    void LoadProbeCache();

    void SetupView(const Ren::Vec3d &origin, const Ren::Vec3d &target, const Ren::Vec3f &up, float fov,
                   Ren::Vec2f sensor_shift, float gamma, float min_exposure, float max_exposure);

    using PostLoadFunc = void(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void RegisterComponent(uint32_t index, Eng::CompStorage *storage, const std::function<PostLoadFunc> &post_init);

    void SetPipelineInitializer(
        std::function<void(const Ren::ProgramRef &prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                           Ren::PipelineStorage &storage, Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines)> &&f) {
        init_pipelines_ = std::move(f);
    }

    void UpdateObjects();

    void ClearGICache(Ren::CommandBuffer cmd_buf = {});

    void UpdateTexturePriorities(Ren::Span<const TexEntry> visible_textures,
                                 Ren::Span<const TexEntry> desired_textures);
    void TexturesGCIteration(Ren::Span<const TexEntry> visible_textures, Ren::Span<const TexEntry> desired_textures);

    void StartTextureLoaderThread(int requests_count = 4, int mip_levels_per_request = 1);
    void StopTextureLoaderThread();
    void ForceTextureReload();

    bool Serve(int texture_budget = 1);

    using ConvertAssetFunc = std::function<bool(assets_context_t &ctx, const char *in_file, const char *out_file,
                                                Ren::SmallVectorImpl<std::string> &out_dependencies,
                                                Ren::SmallVectorImpl<asset_output_t> &out_outputs)>;
    static void RegisterAsset(const char *in_ext, const char *out_ext, const ConvertAssetFunc &convert_func);
    static bool PrepareAssets(const char *in_folder, const char *out_folder, std::string_view platform,
                              Sys::ThreadPool *p_threads, Ren::ILog *log);
    static bool WriteProbeCache(const char *out_folder, const char *scene_name, const Ren::ProbeStorage &probes,
                                const Eng::CompStorage *light_probe_storage, Ren::ILog *log);

  private:
    void PostloadDrawable(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadOccluder(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightmap(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightSource(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadDecal(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightProbe(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadSoundSource(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadAccStructure(const Sys::JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);

    std::array<Ren::MaterialRef, 3> OnLoadMaterial(std::string_view name);
    void OnLoadPipelines(Ren::Bitmask<Ren::eMatFlags> flags, std::string_view v_shader, std::string_view f_shader,
                         std::string_view tc_shader, std::string_view te_shader,
                         Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines);
    Ren::TexRef OnLoadTexture(std::string_view name, const uint8_t color[4], Ren::Bitmask<Ren::eTexFlags> flags);
    Ren::SamplerRef OnLoadSampler(Ren::SamplingParams params);

    Ren::MeshRef LoadMesh(std::string_view name, std::istream *data, const Ren::material_load_callback &on_mat_load,
                          Ren::eMeshLoadStatus *load_status);
    Ren::MaterialRef LoadMaterial(std::string_view name, std::string_view mat_src, Ren::eMatLoadStatus *status,
                                  const Ren::pipelines_load_callback &on_pipes_load,
                                  const Ren::texture_load_callback &on_tex_load,
                                  const Ren::sampler_load_callback &on_sampler_load);
    Ren::TexRef LoadTexture(std::string_view name, Ren::Span<const uint8_t> data, const Ren::TexParams &p,
                            Ren::eTexLoadStatus *load_status);
    Ren::Vec4f LoadDecalTexture(std::string_view name);

    void EstimateTextureMemory(int portion_size);
    bool ProcessPendingTextures(int portion_size);

    void RebuildMaterialTextureGraph();

    std::function<void(const Ren::ProgramRef &prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                       Ren::PipelineStorage &storage, Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines)>
        init_pipelines_;

    void UpdateWorldScrolling(const Ren::Vec3d &new_origin);

    bool UpdateMaterialsBuffer();
    bool UpdateInstanceBuffer();
    void UpdateInstanceBufferRange(uint32_t obj_beg, uint32_t obj_end);
    std::unique_ptr<Ren::IAccStructure> Build_HWRT_BLAS(const AccStructure &acc);
    std::unique_ptr<Ren::IAccStructure> Build_SWRT_BLAS(const AccStructure &acc);
    void Alloc_HWRT_TLAS();
    void Alloc_SWRT_TLAS();

    void RebuildSceneBVH();
    void RemoveNode(uint32_t node_index);

    Ren::Context &ren_ctx_;
    Eng::ShaderLoader &sh_;
    Snd::Context *snd_ctx_ = nullptr;
    Ren::MeshRef cam_rig_;
    Ren::TexRef white_tex_, error_tex_;
    Sys::ThreadPool &threads_;
    path_config_t paths_;

    Ren::Camera cam_, ext_cam_;
    Ren::Vec3d last_cam_pos_;
    double last_cam_time_s_ = 0.0;
    Snd::Source amb_sound_;

    Eng::SceneData scene_data_;
    std::vector<uint32_t> changed_objects_, last_changed_objects_;
    std::vector<uint32_t> instance_data_to_update_;

    std::unique_ptr<Eng::CompStorage> default_comp_storage_[Eng::MAX_COMPONENT_TYPES];
    std::function<PostLoadFunc> component_post_load_[Eng::MAX_COMPONENT_TYPES];

    struct TextureRequest {
        Ren::TexRef ref;
        uint32_t sort_key = 0xffffffff;

        uint16_t frame_dist = 0;

        Ren::eTexFormat orig_format = Ren::eTexFormat::Undefined;
        uint16_t read_offset = 0;
        uint16_t orig_w, orig_h;
        uint8_t orig_mip_count;
        uint8_t mip_offset_to_init, mip_count_to_init;
    };

    enum class eRequestState { Idle, InProgress, PendingIO, PendingUpdate, PendingError };

    struct TextureRequestPending : public TextureRequest {
        std::unique_ptr<Sys::FileReadBufBase> buf;
        Sys::FileReadEvent ev;
        eRequestState state = eRequestState::Idle;
    };
    Ren::RingBuffer<TextureRequest> requested_textures_;

    std::mutex gc_textures_mtx_;
    Ren::RingBuffer<TextureRequest> finished_textures_;
    uint32_t finished_index_ = 0;
    Ren::RingBuffer<TextureRequest> gc_textures_;
    std::atomic<size_t> tex_memory_limit_ = 2ull * 1024 * 1024 * 1024;

    std::mutex tex_requests_lock_;
    std::thread tex_loader_thread_;
    std::condition_variable tex_loader_cnd_;
    bool tex_loader_stop_ = false;

    Sys::AsyncFileReader tex_reader_;

    Ren::SmallVector<TextureRequestPending, 16> io_pending_tex_;
    int mip_levels_per_request_ = 1;

    void TextureLoaderProc();

    static uint32_t PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                        int primitive_alignment, std::vector<gpu_bvh_node_t> &out_nodes,
                                        std::vector<uint32_t> &out_indices);
    static uint32_t ConvertToWBVH_r(Ren::Span<const gpu_light_bvh_node_t> nodes, uint32_t node_index,
                                    uint32_t parent_index, std::vector<gpu_light_wbvh_node_t> &out_nodes);
    static uint32_t ConvertToCWBVH_r(Ren::Span<const gpu_light_bvh_node_t> nodes, uint32_t node_index,
                                     uint32_t parent_index, std::vector<gpu_light_cwbvh_node_t> &out_nodes);
    static uint32_t ConvertToBVH2(Ren::Span<const gpu_bvh_node_t> nodes, std::vector<gpu_bvh2_node_t> &out_nodes);

    std::vector<char> temp_buf;

    // bool cur_lm_indir_ = false;
    // uint32_t cur_lm_obj_ = 0;

    // PT temp data
    // std::vector<Ray::color_rgba_t> pt_lm_direct_, pt_lm_indir_, pt_lm_indir_sh_[4];
    // double pt_lm_started_time_s_ = 0.0;

    // static data for assets conversion
    struct Handler {
        const char *ext;
        ConvertAssetFunc convert;
    };

    static Ren::HashMap32<std::string, Handler> g_asset_handlers;

    static bool HSkip(assets_context_t &ctx, const char *in_file, const char *out_file,
                      Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);
    static bool HCopy(assets_context_t &ctx, const char *in_file, const char *out_file,
                      Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);

    // image textures
    static bool HConvToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                           Ren::SmallVectorImpl<std::string> &out_dependencies, Ren::SmallVectorImpl<asset_output_t> &);
    static bool HConvHDRToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &out_dependencies,
                              Ren::SmallVectorImpl<asset_output_t> &);

    static bool HConvHDRToRGBM(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);

    // probe textures
    static bool HConvImgToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);

    // shaders
    static bool ResolveIncludes(assets_context_t &ctx, const char *in_file, std::string &output,
                                Ren::SmallVectorImpl<std::string> &out_dependencies);
    static bool HCompileShader(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &out_dependencies,
                               Ren::SmallVectorImpl<asset_output_t> &out_outputs);

    // meshes
    static bool HConvGLTFToMesh(assets_context_t &ctx, const char *in_file, const char *out_file,
                                Ren::SmallVectorImpl<std::string> &out_dependencies,
                                Ren::SmallVectorImpl<asset_output_t> &);

    // materials
    static bool HPreprocessMaterial(assets_context_t &ctx, const char *in_file, const char *out_file,
                                    Ren::SmallVectorImpl<std::string> &out_dependencies,
                                    Ren::SmallVectorImpl<asset_output_t> &);

    // scenes
    static bool HPreprocessJson(assets_context_t &ctx, const char *in_file, const char *out_file,
                                Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);

    // fonts
    static bool HConvTTFToFont(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &);
};
} // namespace Eng