#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Transform : public ComponentBase {
    Ren::Mat4f  mat;
    Ren::Vec3f  bbox_min;
    uint32_t    node_index;
    Ren::Vec3f  bbox_max;
    Ren::Vec3f  bbox_min_ws, bbox_max_ws;

    void Read(const JsObject &js_in) override;
    void Write(JsObject &js_out) override;

    void UpdateBBox() {
        bbox_min_ws = bbox_max_ws = Ren::Vec3f(mat[3]);

        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                float a = mat[i][j] * bbox_min[i];
                float b = mat[i][j] * bbox_max[i];

                if (a < b) {
                    bbox_min_ws[j] += a;
                    bbox_max_ws[j] += b;
                } else {
                    bbox_min_ws[j] += b;
                    bbox_max_ws[j] += a;
                }
            }
        }
    }
};