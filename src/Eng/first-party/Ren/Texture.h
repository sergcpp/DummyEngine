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
int GetDataLenBytes(int w, int h, int d, eTexFormat format);
inline int GetDataLenBytes(int w, int h, eTexFormat format) { return GetDataLenBytes(w, h, 1, format); }
uint32_t GetDataLenBytes(const TexParams &params);

eTexFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, bool *is_srgb);
int BlockLenFromGLInternalFormat(uint32_t gl_internal_format);

void ParseDDSHeader(const DDSHeader &hdr, TexParams *params);
} // namespace Ren

#if defined(REN_GL_BACKEND)
#include "TextureGL.h"
#elif defined(REN_VK_BACKEND)
#include "TextureVK.h"
#elif defined(REN_SW_BACKEND)
#include "TextureSW.h"
#endif

namespace Ren {
using TexRef = StrongRef<Texture, NamedStorage<Texture>>;
using WeakTexRef = WeakRef<Texture, NamedStorage<Texture>>;
using TextureStorage = NamedStorage<Texture>;

eTexUsage TexUsageFromState(eResState state);
} // namespace Ren
