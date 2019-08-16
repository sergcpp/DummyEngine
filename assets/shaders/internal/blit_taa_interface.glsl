
#include "_interface_common.glsl"

INTERFACE_START(TempAA)

struct Params {
	VEC4_TYPE transform;
	VEC2_TYPE tex_size;
	float tonemap;
	float gamma;
	float exposure;
	float fade;
};

#ifdef __cplusplus
	const int CURR_TEX_SLOT = 0;
	const int HIST_TEX_SLOT = 1;

	const int DEPTH_TEX_SLOT = 2;
	const int VELOCITY_TEX_SLOT = 3;
#else
	#define CURR_TEX_SLOT 0
	#define HIST_TEX_SLOT 1

	#define DEPTH_TEX_SLOT 2
	#define VELOCITY_TEX_SLOT 3
#endif

INTERFACE_END