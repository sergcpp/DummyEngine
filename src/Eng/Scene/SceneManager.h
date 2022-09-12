#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <Ray/RendererBase.h>
#include <Ren/Camera.h>
#include <Ren/RingBuffer.h>
#include <Sys/AsyncFileReader.h>

#include "SceneData.h"

namespace Sys {
template <typename T, typename FallBackAllocator> class MultiPoolAllocator;
}
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char, std::allocator<char>>>;

namespace Sys {
class ThreadPool;
}

namespace Snd {
class Context;
}

class ShaderLoader;

#include <Sys/Json.h>

namespace SceneManagerInternal {
// TODO: remove this from header file
struct AssetCache {
    JsObjectP js_db;
    Ren::HashMap32<const char *, int> db_map;
    Ren::HashMap32<const char *, uint32_t> texture_averages;

    explicit AssetCache(const Sys::MultiPoolAllocator<char> &mp_alloc) : js_db(mp_alloc) {}

    void WriteTextureAverage(const char *tex_name, const uint8_t average_color[4]) {
        uint32_t color;
        memcpy(&color, average_color, 4);
        texture_averages.Insert(tex_name, color);

        JsObjectP &js_files = js_db["files"].as_obj();
        const int *index = db_map.Find(tex_name);
        if (index) {
            JsObjectP &js_file = js_files.elements[*index].second.as_obj();

            if (js_file.Has("color")) {
                JsNumber &js_color = js_file.at("color").as_num();
                js_color.val = double(color);
            } else {
                auto js_color = JsNumber{double(color)};
                js_file.Push("color", js_color);
            }
        }
    }
};
} // namespace SceneManagerInternal

struct assets_context_t {
    const char *platform;
    Ren::ILog *log;
    std::unique_ptr<SceneManagerInternal::AssetCache> cache;
    Sys::MultiPoolAllocator<char> *mp_alloc;
    Sys::ThreadPool *p_threads;
};

// TODO: remove this!!!
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

class SceneManager : public std::enable_shared_from_this<SceneManager> {
  public:
    SceneManager(Ren::Context &ren_ctx, ShaderLoader &sh, Snd::Context &snd_ctx, Ray::RendererBase &ray_renderer,
                 Sys::ThreadPool &threads);
    ~SceneManager();

    SceneManager(const SceneManager &rhs) = delete;

    const Ren::Camera &main_cam() const { return cam_; }
    Ren::Camera &main_cam() { return cam_; }
    Ren::Mesh *cam_rig() { return cam_rig_.get(); }
    SceneData &scene_data() { return scene_data_; }
    bool load_complete() const { return scene_texture_load_counter_ == 0; }
    Sys::MultiPoolAllocator<char> &mp_alloc() { return mp_alloc_; }

    Snd::Source &ambient_sound() { return amb_sound_; }

    const PersistentGpuData &persistent_data() const { return scene_data_.persistent_data; }

    SceneObject *GetObject(const uint32_t i) { return &scene_data_.objects[i]; }

    uint32_t FindObject(const char *name) {
        uint32_t *p_ndx = scene_data_.name_to_object.Find(name);
        return p_ndx ? (*p_ndx) : 0xffffffff;
    }

    void InvalidateObjects(const uint32_t *indices, const uint32_t count, const uint32_t change_mask) {
        for (uint32_t i = 0; i < count; i++) {
            scene_data_.objects[indices[i]].change_mask |= change_mask;
        }
        changed_objects_.insert(changed_objects_.end(), indices, indices + count);
    }
    void InvalidateTexture(const Ren::Tex2DRef &ref);

    void LoadScene(const JsObjectP &js_scene);
    void SaveScene(JsObjectP &js_scene);
    void ClearScene();

    void LoadProbeCache();

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up, float fov,
                   float max_exposure);

    using PostLoadFunc = void(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void RegisterComponent(uint32_t index, CompStorage *storage, const std::function<PostLoadFunc> &post_init);

    void InitScene_PT(bool _override = false);
    void SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up, float fov);
    const float *Draw_PT(int *w, int *h);
    void Clear_PT();

    void ResetLightmaps_PT();
    bool PrepareLightmaps_PT(const float **preview_pixels, int *w, int *h);

    void UpdateObjects();

    void UpdateTexturePriorities(const TexEntry visible_textures[], int visible_count,
                                 const TexEntry desired_textures[], int desired_count);
    void TexturesGCIteration(const TexEntry visible_textures[], int visible_count, const TexEntry desired_textures[],
                             int desired_count);

    void StartTextureLoader();
    void StopTextureLoader();
    void ForceTextureReload();

    void Serve(int texture_budget = 1);

    using ConvertAssetFunc = std::function<bool(assets_context_t &ctx, const char *in_file, const char *out_file,
                                                Ren::SmallVectorImpl<std::string> &out_dependencies)>;
    static void RegisterAsset(const char *in_ext, const char *out_ext, const ConvertAssetFunc &convert_func);
    static bool PrepareAssets(const char *in_folder, const char *out_folder, const char *platform,
                              Sys::ThreadPool *p_threads, Ren::ILog *log);
    static bool WriteProbeCache(const char *out_folder, const char *scene_name, const Ren::ProbeStorage &probes,
                                const CompStorage *light_probe_storage, Ren::ILog *log);

  private:
    void PostloadDrawable(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadOccluder(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightmap(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightSource(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadDecal(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadLightProbe(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadSoundSource(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);
    void PostloadAccStructure(const JsObjectP &js_comp_obj, void *comp, Ren::Vec3f obj_bbox[2]);

    Ren::MaterialRef OnLoadMaterial(const char *name);
    void OnLoadPipelines(const char *name, uint32_t flags, const char *v_shader, const char *f_shader,
                         const char *tc_shader, const char *te_shader,
                         Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines);
    Ren::Tex2DRef OnLoadTexture(const char *name, const uint8_t color[4], Ren::eTexFlags flags);
    Ren::SamplerRef OnLoadSampler(Ren::SamplingParams params);

    Ren::MeshRef LoadMesh(const char *name, std::istream *data, const Ren::material_load_callback &on_mat_load,
                          Ren::eMeshLoadStatus *load_status);
    Ren::MaterialRef LoadMaterial(const char *name, const char *mat_src, Ren::eMatLoadStatus *status,
                                  const Ren::pipelines_load_callback &on_pipes_load,
                                  const Ren::texture_load_callback &on_tex_load,
                                  const Ren::sampler_load_callback &on_sampler_load);
    Ren::Tex2DRef LoadTexture(const char *name, const void *data, int size, const Ren::Tex2DParams &p,
                              Ren::eTexLoadStatus *load_status);
    Ren::Vec4f LoadDecalTexture(const char *name);

    void EstimateTextureMemory(int portion_size);
    void ProcessPendingTextures(int portion_size);

    void RebuildMaterialTextureGraph();

    // TODO: move these to renderer
    Ren::VertexInput draw_pass_vi_;
    Ren::RenderPass rp_main_draw_;
    void UpdateMaterialsBuffer();
    void InitPipelinesForProgram(const Ren::ProgramRef &prog, uint32_t mat_flags,
                                 Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines);
    void InitHWAccStructures();
    void InitSWAccStructures();

    void RebuildSceneBVH();
    void RemoveNode(uint32_t node_index);

    int scene_texture_load_counter_ = 0;

    Ren::Context &ren_ctx_;
    ShaderLoader &sh_;
    Snd::Context &snd_ctx_;
    Ren::MeshRef cam_rig_;
    Ren::Tex2DRef error_tex_;
    Ray::RendererBase &ray_renderer_;
    Sys::ThreadPool &threads_;
    std::vector<Ray::RegionContext> ray_reg_ctx_;
    std::unique_ptr<Ray::SceneBase> ray_scene_;

    Ren::Camera cam_;
    Ren::Vec3f last_cam_pos_;
    double last_cam_time_s_ = 0.0;
    Snd::Source amb_sound_;

    SceneData scene_data_;
    std::vector<uint32_t> changed_objects_, last_changed_objects_;

    Sys::MultiPoolAllocator<char> mp_alloc_;
    std::unique_ptr<CompStorage> default_comp_storage_[MAX_COMPONENT_TYPES];
    std::function<PostLoadFunc> component_post_load_[MAX_COMPONENT_TYPES];

    struct TextureRequest {
        Ren::Tex2DRef ref;
        uint32_t sort_key = 0xffffffff;

        uint16_t frame_dist = 0;

        Ren::eTexFormat orig_format = Ren::eTexFormat::Undefined;
        Ren::eTexBlock orig_block;
        uint16_t orig_w, orig_h;
        uint8_t orig_mip_count;
        uint8_t mip_offset_to_init, mip_count_to_init;
    };

    enum class eRequestState { Idle, PendingIO, PendingUpdate };

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

    static const int MaxSimultaneousRequests = 4;

    std::mutex tex_requests_lock_;
    std::thread tex_loader_thread_;
    std::condition_variable tex_loader_cnd_;
    bool tex_loader_stop_ = false;

    Sys::AsyncFileReader tex_reader_;

    TextureRequestPending io_pending_tex_[MaxSimultaneousRequests];

    Ren::RingBuffer<Ren::Tex2DRef> lod_transit_textures_;

    void TextureLoaderProc();

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

    static bool HSkip(assets_context_t &ctx, const char *in_file, const char *out_file,
                      Ren::SmallVectorImpl<std::string> &);
    static bool HCopy(assets_context_t &ctx, const char *in_file, const char *out_file,
                      Ren::SmallVectorImpl<std::string> &);

    // image textures
    static bool HConvToASTC(assets_context_t &ctx, const char *in_file, const char *out_file,
                            Ren::SmallVectorImpl<std::string> &);
    static bool HConvToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                           Ren::SmallVectorImpl<std::string> &);

    static bool HConvHDRToRGBM(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &);
    static bool HPreprocessHeightmap(assets_context_t &ctx, const char *in_file, const char *out_file,
                                     Ren::SmallVectorImpl<std::string> &);

    // probe textures
    static bool HConvImgToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &);
    static bool HConvImgToASTC(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &);

    // shaders
    static void InlineShaderConstants(assets_context_t &ctx, std::string &line);
    static bool ResolveIncludes(assets_context_t &ctx, const char *in_file, std::ostream &dst_stream,
                                Ren::SmallVectorImpl<std::string> &out_dependencies);
    static bool HPreprocessShader(assets_context_t &ctx, const char *in_file, const char *out_file,
                                  Ren::SmallVectorImpl<std::string> &out_dependencies);

    // materials
    static bool HPreprocessMaterial(assets_context_t &ctx, const char *in_file, const char *out_file,
                                    Ren::SmallVectorImpl<std::string> &out_dependencies);

    // scenes
    static bool HPreprocessJson(assets_context_t &ctx, const char *in_file, const char *out_file,
                                Ren::SmallVectorImpl<std::string> &);

    // fonts
    static bool HConvTTFToFont(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &);
};
