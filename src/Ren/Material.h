#pragma once

#include <cstdint>

#include <functional>

#include "Fwd.h"
#include "Program.h"
#include "Sampler.h"
#include "SmallVector.h"
#include "Storage.h"
#include "String.h"
#include "Texture.h"

#include "MVec.h"

namespace Ren {
class ILog;

enum class eMatFlags {
    AlphaTest = (1u << 0u),
    AlphaBlend = (1u << 1u),
    DepthWrite = (1u << 2u),
    TwoSided = (1u << 3u),
    CustomShaded = (1u << 4u),
};

enum class eMatLoadStatus { Found, SetToDefault, CreatedFromData };

using texture_load_callback = std::function<Tex2DRef(const char *name, const uint8_t color[4], uint32_t flags)>;
using sampler_load_callback = std::function<SamplerRef(SamplingParams params)>;
using pipelines_load_callback =
    std::function<void(const char *prog_name, uint32_t flags, const char *arg1, const char *arg2, const char *arg3,
                       const char *arg4, SmallVectorImpl<PipelineRef> &out_pipelines)>;

class Material : public RefCounter {
    uint32_t flags_ = 0;
    bool ready_ = false;
    String name_;

    void InitFromTXT(const char *mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
                     const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);

  public:
    SmallVector<PipelineRef, 4> pipelines;
    SmallVector<Tex2DRef, 4> textures;
    SmallVector<SamplerRef, 4> samplers;
    SmallVector<Vec4f, 4> params;
    SmallVector<uint32_t, 4> next_texture_user;

    Material() = default;
    Material(const char *name, const char *mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
             const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);
    Material(const char *name, uint32_t flags, const PipelineRef pipelines[], int pipelines_count,
             const Tex2DRef textures[], const SamplerRef samplers[], int textures_count, const Vec4f params[],
             int params_count, ILog *log);

    Material(const Mesh &rhs) = delete;
    Material(Material &&rhs) = default;

    Material &operator=(const Material &rhs) = delete;
    Material &operator=(Material &&rhs) noexcept = default;

    uint32_t flags() const { return flags_; }
    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void Init(uint32_t flags, const PipelineRef _pipelines[], int pipelines_count, const Tex2DRef _textures[],
              const SamplerRef _samplers[], int textures_count, const Vec4f _params[], int params_count, ILog *log);
    void Init(const char *mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
              const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);
};

// typedef StrongRef<Material> MaterialRef;
typedef Storage<Material> MaterialStorage;
} // namespace Ren
