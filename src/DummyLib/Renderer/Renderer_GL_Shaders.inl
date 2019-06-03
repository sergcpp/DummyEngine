
static const char skydome_vs[] =
#include "Shaders/skydome.vs.glsl"
;

static const char skydome_fs[] =
#include "Shaders/skydome.fs.glsl"
;

static const char fillz_vs[] =
#include "Shaders/fillz.vs.glsl"
;

static const char fillz_fs[] =
#include "Shaders/fillz.fs.glsl"
;

static const char shadow_vs[] =
#include "Shaders/shadow.vs.glsl"
;

static const char shadow_fs[] =
#include "Shaders/shadow.fs.glsl"
;

static const char blit_vs[] =
#include "Shaders/blit.vs.glsl"
;

static const char blit_fs[] =
#include "Shaders/blit.fs.glsl"
;

static const char blit_ms_fs[] =
#include "Shaders/blit_ms.fs.glsl"
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

static const char blit_gauss_fs[] =
#include "Shaders/blit_gauss.fs.glsl"
;

static const char blit_gauss_sep_fs[] =
#include "Shaders/blit_gauss_sep.fs.glsl"
;

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
static const char blit_ssao_fs[] =
#include "Shaders/blit_ssao.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_ssao_ms_fs[] =
#include "Shaders/blit_ssao.fs.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

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