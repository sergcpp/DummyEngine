#pragma once

#include <cstdint>
#undef Always

#include "SamplingParams.h"
#include "TextureParams.h"
#include "Utils.h"

namespace Ren {
class Context;

int GetBlockLenBytes(eTexFormat format);
int GetBlockCount(int w, int h, eTexFormat format);
int GetMipDataLenBytes(int w, int h, eTexFormat format);
uint32_t EstimateMemory(const Tex2DParams &params);

eTexFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, bool *is_srgb);
int BlockLenFromGLInternalFormat(uint32_t gl_internal_format);

void ParseDDSHeader(const DDSHeader &hdr, Tex2DParams *params);
} // namespace Ren

#if defined(REN_GL_BACKEND)
#include "TextureGL.h"
#elif defined(REN_VK_BACKEND)
#include "TextureVK.h"
#elif defined(REN_SW_BACKEND)
#include "TextureSW.h"
#endif

namespace Ren {
using Tex3DRef = StrongRef<Texture3D, NamedStorage<Texture3D>>;
using WeakTex3DRef = WeakRef<Texture3D, NamedStorage<Texture3D>>;
using Texture3DStorage = NamedStorage<Texture3D>;

using Tex2DRef = StrongRef<Texture2D, NamedStorage<Texture2D>>;
using WeakTex2DRef = WeakRef<Texture2D, NamedStorage<Texture2D>>;
using Texture2DStorage = NamedStorage<Texture2D>;

using Tex1DRef = StrongRef<Texture1D, NamedStorage<Texture1D>>;
using WeakTex1DRef = WeakRef<Texture1D, NamedStorage<Texture1D>>;
using Texture1DStorage = NamedStorage<Texture1D>;

eTexUsage TexUsageFromState(eResState state);
} // namespace Ren
