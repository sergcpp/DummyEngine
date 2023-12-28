#include "SWculling.h"
#include "SWrasterize.h"

#include <float.h>

#define SW_CULL_TILE_HEIGHT_SHIFT 2
#define SW_CULL_TILE_SIZE_Y (1 << SW_CULL_TILE_HEIGHT_SHIFT)

#define USE_SSE2
#include "SWculling_rast.inl"
#undef USE_SSE2

#undef SW_CULL_TILE_HEIGHT_SHIFT
#undef SW_CULL_TILE_SIZE_Y