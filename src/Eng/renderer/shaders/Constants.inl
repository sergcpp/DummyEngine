const float MAX_DIST = 3.402823466e+30f;

// Resolution of frustum item grid
const int ITEM_GRID_RES_X = 16;
const int ITEM_GRID_RES_Y = 8;
const int ITEM_GRID_RES_Z = 24;

const int ITEM_CELLS_COUNT = (ITEM_GRID_RES_X * ITEM_GRID_RES_Y * ITEM_GRID_RES_Z);

const int MAX_LIGHTS_PER_CELL = 255;
const int MAX_DECALS_PER_CELL = 255;
const int MAX_PROBES_PER_CELL = 8;
const int MAX_ELLIPSES_PER_CELL = 16;
const int MAX_ITEMS_PER_CELL = 255;

// Attribute location
const int VTX_POS_LOC = 0;
const int VTX_NOR_LOC = 1;
const int VTX_TAN_LOC = 2;
const int VTX_UV1_LOC = 3;
const int VTX_AUX_LOC = 4;
const int VTX_PRE_LOC = 5; // vertex position in previous frame

// Binding slots
const int BIND_MAT_TEX0 = 0;
const int BIND_MAT_TEX1 = 1;
const int BIND_MAT_TEX2 = 2;
const int BIND_MAT_TEX3 = 3;
const int BIND_MAT_TEX4 = 4;
const int BIND_MAT_TEX5 = 5;
const int BIND_SHAD_TEX = 6;
const int BIND_SSAO_TEX_SLOT = 7;
const int BIND_BRDF_LUT = 8;
const int BIND_LIGHT_BUF = 9;
const int BIND_DECAL_BUF = 10;
const int BIND_CELLS_BUF = 11;
const int BIND_ITEMS_BUF = 12;
const int BIND_INST_BUF = 13;
const int BIND_INST_NDX_BUF = 14;
const int BIND_MATERIALS_BUF = 15;
const int BIND_DECAL_TEX = 16;
const int BIND_LMAP_SH = 17;
const int BIND_ENV_TEX = 21;
const int BIND_NOISE_TEX = 22;

const int BIND_SET_SCENETEXTURES = 1;
const int BIND_BINDLESS_TEX = 0; // shares slot with material slot 0

const int BIND_BASE0_TEX = 0;
const int BIND_BASE1_TEX = 1;
const int BIND_BASE2_TEX = 2;

const int REN_U_BASE_INSTANCE_LOC = 2;

const int BIND_UB_SHARED_DATA_BUF = 23;
const int BIND_PUSH_CONSTANT_BUF = 24;

// Shader output location
const int LOC_OUT_COLOR = 0;
const int LOC_OUT_NORM = 1;
const int LOC_OUT_SPEC = 2;
const int LOC_OUT_VELO = 3;
const int LOC_OUT_ALBEDO = 0;

// Ray type
const int RAY_TYPE_CAMERA = 0;
const int RAY_TYPE_DIFFUSE = 1;
const int RAY_TYPE_SPECULAR = 2;
const int RAY_TYPE_REFRACTION = 3;
const int RAY_TYPE_SHADOW = 4;

// Light
const int LIGHT_TYPE_SPHERE = 0;
const int LIGHT_TYPE_RECT = 1;
const int LIGHT_TYPE_DISK = 2;
const int LIGHT_TYPE_LINE = 3;
const int LIGHT_TYPE_TRI = 4;

const int LIGHT_TYPE_BITS = 0x7;
const int LIGHT_PORTAL_BIT = (1 << 3);
const int LIGHT_DIFFUSE_BIT = (1 << 4);
const int LIGHT_SPECULAR_BIT = (1 << 5);
const int LIGHT_VOLUME_BIT = (1 << 6);
const int LIGHT_DOUBLESIDED_BIT = (1 << 7);

// Shadow resolution
const int SHADOWMAP_RES_PC = 8192;
const int SHADOWMAP_RES_ANDROID = 4096;

const int SHADOWMAP_RES = SHADOWMAP_RES_PC;

// Shadow cascades definition
const float SHADOWMAP_CASCADE0_DIST = 10.0f;
const float SHADOWMAP_CASCADE1_DIST = 24.0f;
const float SHADOWMAP_CASCADE2_DIST = 48.0f;
const int SHADOWMAP_CASCADE_SOFT = 0;

const int MAX_OBJ_COUNT = 4194304;
const int MAX_TEX_COUNT = 262144;

const int MAX_TEX_PER_MATERIAL = 6;
const int MAT_TEX_BASECOLOR = 0;
const int MAT_TEX_NORMALMAP = 1;
const int MAT_TEX_ROUGHNESS = 2;
const int MAT_TEX_METALLIC = 3;
const int MAT_TEX_ALPHA = 4;
const int MAT_TEX_EMISSION = 5;
const int MAX_MATERIALS_COUNT = 262144;

const int MAX_INSTANCES_TOTAL = 262144;
const int MAX_SHADOWMAPS_TOTAL = 64;
const int MAX_PROBES_TOTAL = 32;
const int MAX_ELLIPSES_TOTAL = 64;
const int MAX_PORTALS_TOTAL = 64;
const int MAX_SKIN_XFORMS_TOTAL = 65536 / 4;
const int MAX_SKIN_REGIONS_TOTAL = 262144 / 4;
const int MAX_SKIN_VERTICES_TOTAL = 1048576 / 4;
const int MAX_SHAPE_KEYS_TOTAL = 1024;

const int MAX_SHADOW_BATCHES = 262144;
const int MAX_MAIN_BATCHES = 262144;

const int MAX_LIGHTS_TOTAL = 4096;
const int MAX_DECALS_TOTAL = 4096;
const int MAX_ITEMS_TOTAL = int(1u << 16u);

const int MAX_RT_GEO_INSTANCES = 32768;
const int MAX_RT_OBJ_INSTANCES_GI = 2048;
const int MAX_RT_OBJ_INSTANCES_TOTAL = 4096;
const int MAX_RT_TLAS_NODES = 8192; // (4096 + 2048 + 1024 + ...)

const int DECALS_BUF_STRIDE = 7;
const int BVH2_NODE_BUF_STRIDE = 4;
const int MESH_INSTANCE_BUF_STRIDE = 7;

const int PROBE_VOLUME_RES_X = 32;
const int PROBE_VOLUME_RES_Y = 16;
const int PROBE_VOLUME_RES_Z = 32;
const int PROBE_VOLUMES_COUNT = 6;
const int PROBE_TOTAL_RAYS_COUNT = 128;
const int PROBE_FIXED_RAYS_COUNT = 32;
const int PROBE_IRRADIANCE_RES = 6;
const int PROBE_DISTANCE_RES = 16;
const float PROBE_RADIANCE_EXP = 5;

const float PROBE_STATE_INACTIVE = 0;
const float PROBE_STATE_ACTIVE = 1;
const float PROBE_STATE_ACTIVE_OUTDOOR = 2;

// GI
const float GI_CACHE_MULTIBOUNCE_FACTOR = 0.8f;

// Transparency
const int OIT_LAYERS_HIGH = 4;
const int OIT_LAYERS_ULTRA = 6;
const int OIT_REFLECTION_LAYERS = 2;

const int MATERIAL_SOLID_BIT = 32768;  // 0b1000000000000000
const int MATERIAL_INDEX_BITS = 16383; // 0b0011111111111111

// Atmosphere
const int SKY_MULTISCATTER_LUT_RES = 32;
const int SKY_PRE_ATMOSPHERE_SAMPLE_COUNT = 4;
const int SKY_MAIN_ATMOSPHERE_SAMPLE_COUNT = 12;
const int SKY_CLOUDS_SAMPLE_COUNT = 96;
const float SKY_CLOUDS_HORIZON_CUTOFF = 0.005f;
const float SKY_CLOUDS_OFFSET_SCALE = 0.00007f;
const float SKY_MOON_SUN_RELATION = 0.0000001f;
const float SKY_STARS_THRESHOLD = 14.0f;
const float SKY_SUN_BLEND_VAL = 0.000005f;

// Autoexposure
const int EXPOSURE_HISTOGRAM_RES = 64;

#define USE_OCT_PACKED_NORMALS 1

#define FLT_EPS 0.0000001
