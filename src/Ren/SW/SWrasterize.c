#include "SWrasterize.h"

void _swProcessCurveRecursive(SWprogram *p, SWframebuffer *f,
                              SWfloat p1[SW_MAX_VTX_ATTRIBS],
                              SWfloat p2[SW_MAX_VTX_ATTRIBS],
                              SWfloat p3[SW_MAX_VTX_ATTRIBS],
                              SWfloat p4[SW_MAX_VTX_ATTRIBS], SWint b_depth_test,
                              SWint b_depth_write, SWfloat tolerance) {
    SWfloat p12[SW_MAX_VTX_ATTRIBS], p23[SW_MAX_VTX_ATTRIBS], p34[SW_MAX_VTX_ATTRIBS],
        p123[SW_MAX_VTX_ATTRIBS], p234[SW_MAX_VTX_ATTRIBS], p1234[SW_MAX_VTX_ATTRIBS];
    SWint i;

    for (i = 0; i < p->v_out_size; i++) {
        p12[i] = (p1[i] + p2[i]) * 0.5f;
        p23[i] = (p2[i] + p3[i]) * 0.5f;
        p34[i] = (p3[i] + p4[i]) * 0.5f;
        p123[i] = (p12[i] + p23[i]) * 0.5f;
        p234[i] = (p23[i] + p34[i]) * 0.5f;
        p1234[i] = (p123[i] + p234[i]) * 0.5f;
    }

    SWfloat dx = p4[0] - p1[0], dy = p4[1] - p1[1];
    SWfloat d2 = sw_abs((p2[0] - p4[0]) * dy - (p2[1] - p4[1]) * dx),
            d3 = sw_abs((p3[0] - p4[0]) * dy - (p3[1] - p4[1]) * dx);

    if ((d2 + d3) * (d2 + d3) < tolerance * (dx * dx + dy * dy)) {
        _swProcessLine(p, f, p1, p4, b_depth_test, b_depth_write);
    } else {
        _swProcessCurveRecursive(p, f, p1, p12, p123, p1234, b_depth_test, b_depth_write,
                                 tolerance);
        _swProcessCurveRecursive(p, f, p1234, p234, p34, p4, b_depth_test, b_depth_write,
                                 tolerance);
    }
}