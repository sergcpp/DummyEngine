#include "SWculling.h"
#include "SWrasterize.h"

#define SW_CULL_TILE_HEIGHT_SHIFT 3
#define SW_CULL_TILE_SIZE_Y (1 << SW_CULL_TILE_HEIGHT_SHIFT)

#define USE_AVX2
#include "SWculling_rast.inl"
#undef USE_AVX2

#undef SW_CULL_TILE_HEIGHT_SHIFT
#undef SW_CULL_TILE_SIZE_Y
