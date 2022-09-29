#pragma once

#include <cstdint>
#undef Always

#include "SamplingParams.h"
#include "TextureParams.h"

namespace Ren {
class Context;

bool IsCompressedFormat(eTexFormat format);
int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
int GetBlockLenBytes(eTexFormat format, eTexBlock block);
int GetBlockCount(int w, int h, eTexBlock block);
int GetMipDataLenBytes(const int w, const int h, const eTexFormat format, const eTexBlock block);
uint32_t EstimateMemory(const Tex2DParams &params);

eTexFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, eTexBlock *block, bool *is_srgb);
int BlockLenFromGLInternalFormat(uint32_t gl_internal_format);
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "TextureGL.h"
#elif defined(USE_VK_RENDER)
#include "TextureVK.h"
#elif defined(USE_SW_RENDER)
#include "TextureSW.h"
#endif

namespace Ren {
using Tex2DRef = StrongRef<Texture2D>;
using WeakTex2DRef = WeakRef<Texture2D>;
using Texture2DStorage = Storage<Texture2D>;

using Tex1DRef = StrongRef<Texture1D>;
using WeakTex1DRef = WeakRef<Texture1D>;
using Texture1DStorage = Storage<Texture1D>;

eTexUsage TexUsageFromState(eResState state);
} // namespace Ren
