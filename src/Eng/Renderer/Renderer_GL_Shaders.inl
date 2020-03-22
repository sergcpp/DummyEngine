
static const char skydome_vs[] =
#include "Shaders/skydome.vert.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char skydome_fs[] =
#include "Shaders/skydome.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char fillz_solid_vs[] =
#include "Shaders/fillz.vert.glsl"
;
static const char fillz_solid_fs[] =
#include "Shaders/fillz.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char fillz_transp_vs[] =
#include "Shaders/fillz.vert.glsl"
;
static const char fillz_transp_fs[] =
#include "Shaders/fillz.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM\n#define OUTPUT_VELOCITY"
static const char fillz_transp_vel_fs[] =
#include "Shaders/fillz.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char fillz_vege_solid_vs[] =
#include "Shaders/fillz_vege.vert.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define OUTPUT_VELOCITY"
static const char fillz_vege_solid_vel_vs[] =
#include "Shaders/fillz_vege.vert.glsl"
;
static const char fillz_solid_vel_fs[] =
#include "Shaders/fillz.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char fillz_vege_transp_vs[] =
#include "Shaders/fillz_vege.vert.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM\n#define OUTPUT_VELOCITY"
static const char fillz_vege_transp_vel_vs[] =
#include "Shaders/fillz_vege.vert.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char shadow_solid_vs[] =
#include "Shaders/shadow.vert.glsl"
;
static const char shadow_solid_fs[] =
#include "Shaders/shadow.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char shadow_transp_vs[] =
#include "Shaders/shadow.vert.glsl"
;
static const char shadow_transp_fs[] =
#include "Shaders/shadow.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char shadow_vege_solid_vs[] =
#include "Shaders/shadow_vege.vert.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define TRANSPARENT_PERM"
static const char shadow_vege_transp_vs[] =
#include "Shaders/shadow_vege.vert.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_vs[] =
#include "Shaders/blit.vert.glsl"
;

static const char blit_fs[] =
#include "Shaders/blit.frag.glsl"
;

static const char blit_ms_fs[] =
#include "Shaders/blit_ms.frag.glsl"
;

static const char blit_ms_resolve_fs[] =
#include "Shaders/blit_ms_resolve.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_combine_fs[] =
#include "Shaders/blit_combine.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_combine_ms_fs[] =
#include "Shaders/blit_combine.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_reduced_fs[] =
#include "Shaders/blit_reduced.frag.glsl"
;

static const char blit_down_fs[] =
#include "Shaders/blit_down.frag.glsl"
;

static const char blit_down_ms_fs[] =
#include "Shaders/blit_down_ms.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_down_depth_fs[] =
#include "Shaders/blit_down_depth.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__


#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_down_depth_ms_fs[] =
#include "Shaders/blit_down_depth.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_gauss_fs[] =
#include "Shaders/blit_gauss.frag.glsl"
;

static const char blit_gauss_sep_fs[] =
#include "Shaders/blit_gauss_sep.frag.glsl"
;

static const char blit_bilateral_fs[] =
#include "Shaders/blit_bilateral.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_upscale_fs[] =
#include "Shaders/blit_upscale.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_upscale_ms_fs[] =
#include "Shaders/blit_upscale.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_debug_fs[] =
#include "Shaders/blit_debug.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_debug_ms_fs[] =
#include "Shaders/blit_debug.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_depth_fs[] =
#include "Shaders/blit_depth.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_ssr_fs[] =
#include "Shaders/blit_ssr.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_ssr_ms_fs[] =
#include "Shaders/blit_ssr.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_ssr_compose_fs[] =
#include "Shaders/blit_ssr_compose.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_ssr_compose_ms_fs[] =
#include "Shaders/blit_ssr_compose.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_ssr_dilate_fs[] =
#include "Shaders/blit_ssr_dilate.frag.glsl"
;

static const char blit_ssao_fs[] =
#include "Shaders/blit_ssao.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_multiply_fs[] =
#include "Shaders/blit_multiply.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_multiply_ms_fs[] =
#include "Shaders/blit_multiply.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_debug_bvh_fs[] =
#include "Shaders/blit_debug_bvh.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_debug_bvh_ms_fs[] =
#include "Shaders/blit_debug_bvh.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_rgbm_fs[] =
#include "Shaders/blit_rgbm.frag.glsl"
;

static const char blit_mipmap_fs[] =
#include "Shaders/blit_mipmap.frag.glsl"
;

static const char blit_prefilter_fs[] =
#include "Shaders/blit_prefilter.frag.glsl"
;

static const char blit_project_sh_fs[] =
#include "Shaders/blit_project_sh.frag.glsl"
;

static const char blit_fxaa_fs[] =
#include "Shaders/blit_fxaa.frag.glsl"
;

static const char blit_taa_fs[] =
#include "Shaders/blit_taa.frag.glsl"
;

static const char blit_static_vel_fs[] =
#include "Shaders/blit_static_vel.frag.glsl"
;

#define __ADDITIONAL_DEFINES_STR__ ""
static const char blit_transparent_compose_fs[] =
#include "Shaders/blit_transparent_compose.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

#define __ADDITIONAL_DEFINES_STR__ "#define MSAA_4"
static const char blit_transparent_compose_ms_fs[] =
#include "Shaders/blit_transparent_compose.frag.glsl"
;
#undef __ADDITIONAL_DEFINES_STR__

static const char blit_transparent_init_fs[] =
#include "Shaders/blit_transparent_init.frag.glsl"
;

static const char probe_vs[] =
#include "Shaders/probe.vert.glsl"
;

static const char probe_fs[] =
#include "Shaders/probe.frag.glsl"
;

#ifndef __ANDROID__
#define GLSL_VERSION_STR "430"
#else
#define GLSL_VERSION_STR "310 es"
#endif

static const char skinning_cs[] =
#include "Shaders/skinning.comp.glsl"
;
