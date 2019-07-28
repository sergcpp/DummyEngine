
static const char skydome_vs[] =
#include "Shaders/skydome.vs.glsl"
;

static const char skydome_fs[] =
#include "Shaders/skydome.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char fillz_solid_vs[] =
#include "Shaders/fillz.vs.glsl"
;
static const char fillz_solid_fs[] =
#include "Shaders/fillz.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char fillz_transp_vs[] =
#include "Shaders/fillz.vs.glsl"
;
static const char fillz_transp_fs[] =
#include "Shaders/fillz.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char shadow_solid_vs[] =
#include "Shaders/shadow.vs.glsl"
;
static const char shadow_solid_fs[] =
#include "Shaders/shadow.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char shadow_transp_vs[] =
#include "Shaders/shadow.vs.glsl"
;
static const char shadow_transp_fs[] =
#include "Shaders/shadow.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_vs[] =
#include "Shaders/blit.vs.glsl"
;

static const char blit_fs[] =
#include "Shaders/blit.fs.glsl"
;

static const char blit_ms_fs[] =
#include "Shaders/blit_ms.fs.glsl"
;

static const char blit_ms_resolve_fs[] =
#include "Shaders/blit_ms_resolve.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_combine_fs[] =
#include "Shaders/blit_combine.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_combine_ms_fs[] =
#include "Shaders/blit_combine.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_reduced_fs[] =
#include "Shaders/blit_reduced.fs.glsl"
;

static const char blit_down_fs[] =
#include "Shaders/blit_down.fs.glsl"
;

static const char blit_down_ms_fs[] =
#include "Shaders/blit_down_ms.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_down_depth_fs[] =
#include "Shaders/blit_down_depth.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__


#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_down_depth_ms_fs[] =
#include "Shaders/blit_down_depth.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_gauss_fs[] =
#include "Shaders/blit_gauss.fs.glsl"
;

static const char blit_gauss_sep_fs[] =
#include "Shaders/blit_gauss_sep.fs.glsl"
;

static const char blit_bilateral_fs[] =
#include "Shaders/blit_bilateral.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_upscale_fs[] =
#include "Shaders/blit_upscale.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_upscale_ms_fs[] =
#include "Shaders/blit_upscale.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_debug_fs[] =
#include "Shaders/blit_debug.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_debug_ms_fs[] =
#include "Shaders/blit_debug.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_depth_fs[] =
#include "Shaders/blit_depth.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_ssr_fs[] =
#include "Shaders/blit_ssr.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_ssr_ms_fs[] =
#include "Shaders/blit_ssr.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_ssr_compose_fs[] =
#include "Shaders/blit_ssr_compose.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_ssr_compose_ms_fs[] =
#include "Shaders/blit_ssr_compose.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_ssr_dilate_fs[] =
#include "Shaders/blit_ssr_dilate.fs.glsl"
;

static const char blit_ssao_fs[] =
#include "Shaders/blit_ssao.fs.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_multiply_fs[] =
#include "Shaders/blit_multiply.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_multiply_ms_fs[] =
#include "Shaders/blit_multiply.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_debug_bvh_fs[] =
#include "Shaders/blit_debug_bvh.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_debug_bvh_ms_fs[] =
#include "Shaders/blit_debug_bvh.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_rgbm_fs[] =
#include "Shaders/blit_rgbm.fs.glsl"
;

static const char blit_mipmap_fs[] =
#include "Shaders/blit_mipmap.fs.glsl"
;

static const char blit_prefilter_fs[] =
#include "Shaders/blit_prefilter.fs.glsl"
;

static const char blit_project_sh_fs[] =
#include "Shaders/blit_project_sh.fs.glsl"
;

static const char blit_fxaa_fs[] =
#include "Shaders/blit_fxaa.fs.glsl"
;

static const char probe_vs[] =
#include "Shaders/probe.vs.glsl"
;

static const char probe_fs[] =
#include "Shaders/probe.fs.glsl"
;

#ifndef __ANDROID__
#define GLSL_VERSION_STR "430"
#else
#define GLSL_VERSION_STR "310 es"
#endif

static const char skinning_cs[] =
#include "Shaders/skinning.cs.glsl"
;