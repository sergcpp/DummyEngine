

// Resolution of frustum item grid
#define REN_GRID_RES_X 16
#define REN_GRID_RES_Y 8
#define REN_GRID_RES_Z 24

// Attribute location
#define REN_VTX_POS_LOC 0
#define REN_VTX_NOR_LOC 1
#define REN_VTX_TAN_LOC 2
#define REN_VTX_UV1_LOC 3
#define REN_VTX_AUX_LOC 4

// Texture binding
#define REN_MAT_TEX0_SLOT   0
#define REN_MAT_TEX1_SLOT   1
#define REN_MAT_TEX2_SLOT   2
#define REN_MAT_TEX3_SLOT   3
#define REN_SHAD_TEX_SLOT   4
#define REN_LMAP_SH_SLOT    5
#define REN_DECAL_TEX_SLOT  9
#define REN_SSAO_TEX_SLOT   10
#define REN_BRDF_TEX_SLOT   11
#define REN_LIGHT_BUF_SLOT  12
#define REN_DECAL_BUF_SLOT  13
#define REN_CELLS_BUF_SLOT  14
#define REN_ITEMS_BUF_SLOT  15
#define REN_INST_BUF_SLOT   16
#define REN_ENV_TEX_SLOT    17
#define REN_MOMENTS0_TEX_SLOT 5
#define REN_MOMENTS1_TEX_SLOT 6
#define REN_MOMENTS2_TEX_SLOT 7
#define REN_MOMENTS0_MS_TEX_SLOT 8
#define REN_MOMENTS1_MS_TEX_SLOT 10
#define REN_MOMENTS2_MS_TEX_SLOT 18
#define REN_NOISE_TEX_SLOT 18
#define REN_CONE_RT_LUT_SLOT 19

#define REN_ALPHATEST_TEX_SLOT 0

#define REN_BASE0_TEX_SLOT 0
#define REN_BASE1_TEX_SLOT 1
#define REN_BASE2_TEX_SLOT 2

#define REN_REFL_DEPTH_TEX_SLOT 0
#define REN_REFL_NORM_TEX_SLOT  1
#define REN_REFL_SPEC_TEX_SLOT  2
#define REN_REFL_PREV_TEX_SLOT  3
#define REN_REFL_SSR_TEX_SLOT   4
#define REN_REFL_ENV_TEX_SLOT   5
#define REN_REFL_BRDF_TEX_SLOT  6
#define REN_REFL_DEPTH_LOW_TEX_SLOT 7

#define REN_U_M_MATRIX_LOC  0
#define REN_U_INSTANCES_LOC 1

#define REN_UB_SHARED_DATA_LOC  0

// Shader output location
#define REN_OUT_COLOR_INDEX 0
#define REN_OUT_NORM_INDEX  1
#define REN_OUT_SPEC_INDEX  2
#define REN_OUT_VELO_INDEX  3

// Shadow resolution
#define REN_SHAD_RES_PC         8192
#define REN_SHAD_RES_ANDROID    4096
#define REN_SHAD_RES REN_SHAD_RES_PC


// Shadow cascades definition
#define REN_SHAD_CASCADE0_DIST      10.0
#define REN_SHAD_CASCADE0_SAMPLES   16
#define REN_SHAD_CASCADE1_DIST      24.0
#define REN_SHAD_CASCADE1_SAMPLES   8
#define REN_SHAD_CASCADE2_DIST      48.0
#define REN_SHAD_CASCADE2_SAMPLES   4
#define REN_SHAD_CASCADE3_DIST      96.0
#define REN_SHAD_CASCADE3_SAMPLES   4
#define REN_SHAD_CASCADE_SOFT       1

#define REN_MAX_OBJ_COUNT			4194304
#define REN_MAX_BATCH_SIZE			8

#define REN_MAX_INSTANCES_TOTAL     262144
#define REN_MAX_SHADOWMAPS_TOTAL    32
#define REN_MAX_PROBES_TOTAL        32
#define REN_MAX_ELLIPSES_TOTAL      64
#define REN_SKIN_REGION_SIZE        256
#define REN_MAX_SKIN_XFORMS_TOTAL   65536
#define REN_MAX_SKIN_REGIONS_TOTAL  262144
#define REN_MAX_SKIN_VERTICES_TOTAL 1048576

#define REN_MAX_SHADOW_BATCHES  262144
#define REN_MAX_MAIN_BATCHES    262144

#define REN_CELLS_COUNT (REN_GRID_RES_X * REN_GRID_RES_Y * REN_GRID_RES_Z)

#define REN_MAX_LIGHTS_PER_CELL 255
#define REN_MAX_DECALS_PER_CELL 255
#define REN_MAX_PROBES_PER_CELL 8
#define REN_MAX_ELLIPSES_PER_CELL 16
#define REN_MAX_ITEMS_PER_CELL  255

#define REN_MAX_LIGHTS_TOTAL 4096
#define REN_MAX_DECALS_TOTAL 4096
#define REN_MAX_ITEMS_TOTAL int(1u << 16u)

#define REN_OIT_DISABLED            0
#define REN_OIT_MOMENT_BASED        1
#define REN_OIT_WEIGHTED_BLENDED    2

#define REN_OIT_MOMENT_RENORMALIZE  1

#define REN_OIT_MODE REN_OIT_DISABLED

#define FLT_EPS 0.0000001

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

struct EllipsItem {
    vec4 pos_and_radius;
    vec4 axis_and_perp;
};

struct SharedData {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[REN_MAX_SHADOWMAPS_TOTAL];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[REN_MAX_PROBES_TOTAL];
    EllipsItem uEllipsoids[REN_MAX_ELLIPSES_TOTAL];
};



#define VEGE_MAX_MOVEMENT 8.0
#define VEGE_MAX_BRANCH_AMPLITUDE 1.0
#define VEGE_MAX_LEAVES_AMPLITUDE 0.2

#define VEGE_NOISE_SCALE_LF (1.0 / 256.0)
#define VEGE_NOISE_SCALE_HF (1.0 / 8.0)

vec3 TransformVegetation(in vec3 vtx_pos_ls, vec4 vtx_color, vec4 wind_scroll, vec4 wind_params, vec4 wind_vec_ls, sampler2D _noise_texture) {
	{	// Branch/detail bending
		const float scale = 4.0;
		
        vec3 pivot_dir = scale * (2.0 * vtx_color.xyz - vec3(1.0));
        float leaf_flex = VEGE_MAX_LEAVES_AMPLITUDE * clamp(vtx_color.w - 0.01, 0.0, 1.0);
		
		float movement_scale = VEGE_MAX_MOVEMENT * wind_params.x;
		float bend_scale = 0.005 * wind_params.z;
		float stretch = wind_params.w;

		vec3 pivot_pos_ls = vtx_pos_ls + pivot_dir;
		float dist = length(pivot_dir);
        float branch_flex = VEGE_MAX_BRANCH_AMPLITUDE * step(0.005, vtx_color.w) * dist / scale;
		
		mat2 uv_tr;
		uv_tr[0] = normalize(vec2(wind_vec_ls.x, wind_vec_ls.z));
		uv_tr[1] = vec2(uv_tr[0].y, -uv_tr[0].x);

		vec2 vtx_pos_ts = uv_tr * vtx_pos_ls.xz + vec2(2.0 * pivot_pos_ls.y, 0.0);
		
        vec3 noise_dir_lf = wind_vec_ls.w * texture(_noise_texture, wind_scroll.xy + VEGE_NOISE_SCALE_LF * vtx_pos_ts).rgb;
		vec3 noise_dir_hf = wind_vec_ls.w * texture(_noise_texture, wind_scroll.zw + VEGE_NOISE_SCALE_HF * vtx_pos_ts).rgb;
		
        vec3 new_pos_ls = vtx_pos_ls + movement_scale * (branch_flex * (bend_scale * wind_vec_ls.xyz + noise_dir_lf) + leaf_flex * noise_dir_hf);
		vtx_pos_ls = mix(pivot_pos_ls + normalize(new_pos_ls - pivot_pos_ls) * dist, new_pos_ls, stretch);
    }
	
	{   // Trunk bending
        const float bend_scale = 0.0015;
		float tree_mode = wind_params.y;
        float bend_factor = vtx_pos_ls.y * tree_mode * bend_scale;
        // smooth bending factor and increase its nearby height limit
        bend_factor += 1.0;
        bend_factor *= bend_factor;
        bend_factor = bend_factor * bend_factor - bend_factor;
        // displace position
        vec3 new_pos_ls = vtx_pos_ls;
        new_pos_ls.xz += wind_vec_ls.xz * bend_factor;
        // rescale
        vtx_pos_ls = normalize(new_pos_ls) * length(vtx_pos_ls);
    }
	
	return vtx_pos_ls;
}

