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

enum class eMatFlags { AlphaTest, AlphaBlend, DepthWrite, TwoSided, Emissive, CustomShaded };

enum class eMatLoadStatus { Found, SetToDefault, CreatedFromData, CreatedFromData_NeedsMore };

using texture_load_callback =
    std::function<Tex2DRef(std::string_view name, const uint8_t color[4], Bitmask<eTexFlags> flags)>;
using sampler_load_callback = std::function<SamplerRef(SamplingParams params)>;
using pipelines_load_callback =
    std::function<void(Bitmask<eMatFlags> flags, std::string_view arg1, std::string_view arg2, std::string_view arg3,
                       std::string_view arg4, SmallVectorImpl<PipelineRef> &out_pipelines)>;

class Material : public RefCounter {
    Bitmask<eMatFlags> flags_;
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
    Material(std::string_view name, Bitmask<eMatFlags> flags, Span<const PipelineRef> pipelines,
             Span<const Tex2DRef> textures, Span<const SamplerRef> samplers, Span<const Vec4f> params, ILog *log);

    Material(const Mesh &rhs) = delete;
    Material(Material &&rhs) = default;

    Material &operator=(const Material &rhs) = delete;
    Material &operator=(Material &&rhs) noexcept = default;

    Bitmask<eMatFlags> flags() const { return flags_; }
    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void Init(Bitmask<eMatFlags> flags, Span<const PipelineRef> _pipelines, Span<const Tex2DRef> _textures,
              Span<const SamplerRef> _samplers, Span<const Vec4f> _params, ILog *log);
    void Init(std::string_view mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
              const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load, ILog *log);
};

using MaterialRef = StrongRef<Material, NamedStorage<Material>>;
using MaterialStorage = NamedStorage<Material>;
} // namespace Ren
