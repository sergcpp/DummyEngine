#pragma once

#include <cstdint>
#undef Always

#include "ImageParams.h"
#include "SamplingParams.h"
#include "Utils.h"

namespace Ren {
class Context;

int GetBlockLenBytes(eFormat format);
int GetBlockCount(int w, int h, eFormat format);
int GetDataLenBytes(int w, int h, int d, eFormat format);
inline int GetDataLenBytes(int w, int h, eFormat format) { return GetDataLenBytes(w, h, 1, format); }
uint32_t GetDataLenBytes(const ImgParams &params);

eFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, bool *is_srgb);
int BlockLenFromGLInternalFormat(uint32_t gl_internal_format);

void ParseDDSHeader(const DDSHeader &hdr, ImgParams *params);
} // namespace Ren

#if defined(REN_GL_BACKEND)
#include "ImageGL.h"
#elif defined(REN_VK_BACKEND)
#include "ImageVK.h"
#elif defined(REN_SW_BACKEND)
#include "ImageSW.h"
#endif

namespace Ren {
using ImgRef = StrongRef<Image, NamedStorage<Image>>;
using WeakImgRef = WeakRef<Image, NamedStorage<Image>>;
using ImageStorage = NamedStorage<Image>;

eImgUsage ImgUsageFromState(eResState state);
} // namespace Ren
