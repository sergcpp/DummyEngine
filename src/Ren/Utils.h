#pragma once

#include <memory>

namespace Ren {
enum eTexColorFormat;
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexColorFormat &format);

std::unique_ptr<uint16_t[]> ConvertRGBE_to_RGB16F(const uint8_t *image_data, int w, int h);

void ReorderTriangleIndices(const uint32_t *indices, uint32_t indices_count, uint32_t vtx_count, uint32_t *out_indices);

struct vertex_t {
    float p[3], n[3], b[3], t[2][2];
};

void ComputeTextureBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> &new_vtx_indices,
                         const uint32_t *indices, size_t indices_count);
}