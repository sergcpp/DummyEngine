#pragma once

#include "MMat.h"
#include "MQuat.h"

namespace Phy {
#ifdef DOUBLE_PRECISION
using real = double;
#else
using real = float;
#endif

using Vec2 = Vec<real, 2>;
using Vec3 = Vec<real, 3>;
using Vec4 = Vec<real, 4>;

using Mat2 = Mat<real, 2, 2>;
using Mat3 = Mat<real, 3, 3>;
using Mat4 = Mat<real, 4, 4>;

using Quat = QuatT<real>;
}