#ifndef OIT_SCHEDULE_RAYS_INTERFACE_H
#define OIT_SCHEDULE_RAYS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(OITScheduleRays)

const uint OIT_DEPTH_BUF_SLOT = 4;

const uint RAY_COUNTER_SLOT = 1;
const uint RAY_LIST_SLOT = 2;
const uint RAY_BITMASK_SLOT = 3;

INTERFACE_END

#endif // OIT_SCHEDULE_RAYS_INTERFACE_H