#include "SWculling.h"
#include "SWrasterize.h"

#include <float.h>

#define SW_CULL_TILE_HEIGHT_SHIFT 4
#define SW_CULL_TILE_SIZE_Y (1 << SW_CULL_TILE_HEIGHT_SHIFT)

#define USE_AVX512
#include "SWculling_rast.inl"
#undef USE_AVX512

#undef SW_CULL_TILE_HEIGHT_SHIFT
#undef SW_CULL_TILE_SIZE_Y
