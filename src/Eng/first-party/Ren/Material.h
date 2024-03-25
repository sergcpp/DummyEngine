#pragma once

#include <cstdint>

#include <functional>
#include <string_view>

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

using texture_load_callback = std::function<Tex2DRef(std::string_view name, const uint8_t color[4], eTexFlags flags)>;
using sampler_load_callback = std::function<SamplerRef(SamplingParams params)>;
using pipelines_load_callback =
    std::function<void(std::string_view prog_name, uint32_t flags, const char *arg1, const char *arg2, const char *arg3,
                       const char *arg4, SmallVectorImpl<PipelineRef> &out_pipelines)>;

class Material : public RefCounter {
    uint32_t flags_ = 0;
    bool ready_ = false;
    String name_;

    void InitFromMAT(std::string_view mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
                     const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);

  public:
    SmallVector<PipelineRef, 4> pipelines;
    SmallVector<Tex2DRef, 4> textures;
    SmallVector<SamplerRef, 4> samplers;
    SmallVector<Vec4f, 4> params;
    SmallVector<uint32_t, 4> next_texture_user;

    Material() = default;
    Material(std::string_view name, std::string_view mat_src, eMatLoadStatus *status,
             const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
             const sampler_load_callback &on_sampler_load, ILog *log);
    Material(std::string_view name, uint32_t flags, Span<const PipelineRef> pipelines, Span<const Tex2DRef> textures,
             Span<const SamplerRef> samplers, Span<const Vec4f> params, ILog *log);

    Material(const Mesh &rhs) = delete;
    Material(Material &&rhs) = default;

    Material &operator=(const Material &rhs) = delete;
    Material &operator=(Material &&rhs) noexcept = default;

    uint32_t flags() const { return flags_; }
    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void Init(uint32_t flags, Span<const PipelineRef> _pipelines, Span<const Tex2DRef> _textures,
              Span<const SamplerRef> _samplers, Span<const Vec4f> _params, ILog *log);
    void Init(std::string_view mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
              const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);
};

// typedef StrongRef<Material> MaterialRef;
typedef Storage<Material> MaterialStorage;
} // namespace Ren
