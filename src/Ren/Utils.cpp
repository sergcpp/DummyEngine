#include "Utils.h"

#include <deque>

#include "Texture.h"

namespace Ren {
    uint16_t f32_to_f16(float value) {
        int32_t i;
        memcpy(&i, &value, sizeof(float));

        int32_t s = (i >> 16) & 0x00008000;
        int32_t e = ((i >> 23) & 0x000000ff) - (127 - 15);
        int32_t m = i & 0x007fffff;
        if (e <= 0) {
            if (e < -10) {
                uint16_t ret;
                memcpy(&ret, &s, sizeof(uint16_t));
                return ret;
            }

            m = (m | 0x00800000) >> (1 - e);

            if (m & 0x00001000)
                m += 0x00002000;

            s = s | (m >> 13);
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        } else if (e == 0xff - (127 - 15)) {
            if (m == 0) {
                s = s | 0x7c00;
                uint16_t ret;
                memcpy(&ret, &s, sizeof(uint16_t));
                return ret;
            } else {
                m >>= 13;

                s = s | 0x7c00 | m | (m == 0);
                uint16_t ret;
                memcpy(&ret, &s, sizeof(uint16_t));
                return ret;
            }
        } else {
            if (m & 0x00001000) {
                m += 0x00002000;

                if (m & 0x00800000) {
                    m = 0;     // overflow in significand,
                    e += 1;     // adjust exponent
                }
            }

            if (e > 30) {
                s = s | 0x7c00;
                uint16_t ret;
                memcpy(&ret, &s, sizeof(uint16_t));
                return ret;
            }

            s = s | (e << 10) | (m >> 13);
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }
    }
}

std::unique_ptr<uint8_t[]> Ren::ReadTGAFile(const void *data, int &w, int &h, eTexColorFormat &format) {
    uint8_t tga_header[12] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const uint8_t *tga_compare = (const uint8_t *)data;
    const uint8_t *img_header = (const uint8_t *)data + sizeof(tga_header);
    uint32_t img_size;
    bool compressed = false;

    if (memcmp(tga_header, tga_compare, sizeof(tga_header)) != 0) {
        if (tga_compare[2] == 1) {
            fprintf(stderr, "Image cannot be indexed color.");
        }
        if (tga_compare[2] == 3) {
            fprintf(stderr, "Image cannot be greyscale color.");
        }
        if (tga_compare[2] == 9 || tga_compare[2] == 10) {
            //fprintf(stderr, "Image cannot be compressed.");
            compressed = true;
        }
    }

    w = img_header[1] * 256u + img_header[0];
    h = img_header[3] * 256u + img_header[2];

    if (w <= 0 || h <= 0 ||
            (img_header[4] != 24 && img_header[4] != 32)) {
        if (w <= 0 || h <= 0) {
            fprintf(stderr, "Image must have a width and height greater than 0");
        }
        if (img_header[4] != 24 && img_header[4] != 32) {
            fprintf(stderr, "Image must be 24 or 32 bit");
        }
        return nullptr;
    }

    uint32_t bpp = img_header[4];
    uint32_t bytes_per_pixel = bpp / 8;
    img_size = w * h * bytes_per_pixel;
    const uint8_t *image_data = (const uint8_t *)data + 18;

    std::unique_ptr<uint8_t[]> image_ret(new uint8_t[img_size]);
    uint8_t *_image_ret = &image_ret[0];

    if (!compressed) {
        for (unsigned i = 0; i < img_size; i += bytes_per_pixel) {
            _image_ret[i] = image_data[i + 2];
            _image_ret[i + 1] = image_data[i + 1];
            _image_ret[i + 2] = image_data[i];
            if (bytes_per_pixel == 4) {
                _image_ret[i + 3] = image_data[i + 3];
            }
        }
    } else {
        for (unsigned num = 0; num < img_size;) {
            uint8_t packet_header = *image_data++;
            if (packet_header & (1 << 7)) {
                uint8_t color[4];
                unsigned size = (packet_header & ~(1 << 7)) + 1;
                size *= bytes_per_pixel;
                for (unsigned i = 0; i < bytes_per_pixel; i++) {
                    color[i] = *image_data++;
                }
                for (unsigned i = 0; i < size; i += bytes_per_pixel, num += bytes_per_pixel) {
                    _image_ret[num] = color[2];
                    _image_ret[num + 1] = color[1];
                    _image_ret[num + 2] = color[0];
                    if (bytes_per_pixel == 4) {
                        _image_ret[num + 3] = color[3];
                    }
                }
            } else {
                unsigned size = (packet_header & ~(1 << 7)) + 1;
                size *= bytes_per_pixel;
                for (unsigned i = 0; i < size; i += bytes_per_pixel, num += bytes_per_pixel) {
                    _image_ret[num] = image_data[i + 2];
                    _image_ret[num + 1] = image_data[i + 1];
                    _image_ret[num + 2] = image_data[i];
                    if (bytes_per_pixel == 4) {
                        _image_ret[num + 3] = image_data[i + 3];
                    }
                }
                image_data += size;
            }
        }
    }

    if (bpp == 32) {
        format = RawRGBA8888;
    } else if (bpp == 24) {
        format = RawRGB888;
    }

    return image_ret;
}

std::unique_ptr<float[]> Ren::ConvertRGBE_to_RGB32F(const uint8_t *image_data, int w, int h) {
    std::unique_ptr<float[]> fp_data(new float[w * h * 3]);

    for (int i = 0; i < w * h; i++) {
        uint8_t r = image_data[4 * i + 0];
        uint8_t g = image_data[4 * i + 1];
        uint8_t b = image_data[4 * i + 2];
        uint8_t a = image_data[4 * i + 3];

        float f = std::exp2(float(a) - 128.0f);
        float k = 1.0f / 255;

        fp_data[3 * i + 0] = k * r * f;
        fp_data[3 * i + 1] = k * g * f;
        fp_data[3 * i + 2] = k * b * f;
    }

    return fp_data;
}

std::unique_ptr<uint16_t[]> Ren::ConvertRGBE_to_RGB16F(const uint8_t *image_data, int w, int h) {
    std::unique_ptr<uint16_t[]> fp_data(new uint16_t[w * h * 3]);

    for (int i = 0; i < w * h; i++) {
        uint8_t r = image_data[4 * i + 0];
        uint8_t g = image_data[4 * i + 1];
        uint8_t b = image_data[4 * i + 2];
        uint8_t a = image_data[4 * i + 3];

        float f = std::exp2(float(a) - 128.0f);
        float k = 1.0f / 255;

        fp_data[3 * i + 0] = f32_to_f16(k * r * f);
        fp_data[3 * i + 1] = f32_to_f16(k * g * f);
        fp_data[3 * i + 2] = f32_to_f16(k * b * f);
    }

    return fp_data;
}

void Ren::ReorderTriangleIndices(const uint32_t *indices, uint32_t indices_count, uint32_t vtx_count, uint32_t *out_indices) {
    // From https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html

    uint32_t prim_count = indices_count / 3;

    struct vtx_data_t {
        int32_t cache_pos = -1;
        float score = 0.0f;
        uint32_t ref_count = 0;
        uint32_t active_tris_count = 0;
        std::unique_ptr<int32_t[]> tris;
    };

    const int MaxSizeVertexCache = 32;

    auto get_vertex_score = [MaxSizeVertexCache](int32_t cache_pos, uint32_t active_tris_count) -> float {
        const float CacheDecayPower = 1.5f;
        const float LastTriScore = 0.75f;
        const float ValenceBoostScale = 2.0f;
        const float ValenceBoostPower = 0.5f;

        if (active_tris_count == 0) {
            // No tri needs this vertex!
            return -1.0f;
        }

        float score = 0.0f;

        if (cache_pos < 0) {
            // Vertex is not in FIFO cache - no score.
        } else if (cache_pos < 3) {
            // This vertex was used in the last triangle,
            // so it has a fixed score, whichever of the three
            // it's in. Otherwise, you can get very different
            // answers depending on whether you add
            // the triangle 1,2,3 or 3,1,2 - which is silly.
            score = LastTriScore;
        } else {
            assert(cache_pos < MaxSizeVertexCache);
            // Points for being high in the cache.
            const float scaler = 1.0f / (MaxSizeVertexCache - 3);
            score = 1.0f - (cache_pos - 3) * scaler;
            score = std::pow(score, CacheDecayPower);
        }

        // Bonus points for having a low number of tris still to
        // use the vert, so we get rid of lone verts quickly.

        float valence_boost = std::pow((float)active_tris_count, -ValenceBoostPower);
        score += ValenceBoostScale * valence_boost;
        return score;
    };

    struct tri_data_t {
        bool is_in_list = false;
        float score = 0.0f;
        uint32_t indices[3] = {};
    };

    std::vector<vtx_data_t> vertices(vtx_count);
    std::vector<tri_data_t> triangles(prim_count);

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i/3];

        tri.indices[0] = indices[i + 0];
        tri.indices[1] = indices[i + 1];
        tri.indices[2] = indices[i + 2];

        vertices[indices[i + 0]].active_tris_count++;
        vertices[indices[i + 1]].active_tris_count++;
        vertices[indices[i + 2]].active_tris_count++;
    }

    for (auto &v : vertices) {
        v.tris.reset(new int32_t[v.active_tris_count]);
        v.score = get_vertex_score(v.cache_pos, v.active_tris_count);
    }

    int32_t next_best_index = -1, next_next_best_index = -1;
    float next_best_score = -1.0f, next_next_best_score = -1.0f;

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i / 3];

        auto &v0 = vertices[indices[i + 0]];
        auto &v1 = vertices[indices[i + 1]];
        auto &v2 = vertices[indices[i + 2]];

        v0.tris[v0.ref_count++] = i / 3;
        v1.tris[v1.ref_count++] = i / 3;
        v2.tris[v2.ref_count++] = i / 3;

        tri.score = v0.score + v1.score + v2.score;

        if (tri.score > next_best_score) {
            if (next_best_score > next_next_best_score) {
                next_next_best_index = next_best_index;
                next_next_best_score = next_best_score;
            }
            next_best_index = i / 3;
            next_best_score = tri.score;
        }

        if (tri.score > next_next_best_score) {
            next_next_best_index = i / 3;
            next_next_best_score = tri.score;
        }
    }

    std::deque<uint32_t> lru_cache;

    auto use_vertex = [](std::deque<uint32_t> &lru_cache, uint32_t vtx_index) {
        auto it = std::find(std::begin(lru_cache), std::end(lru_cache), vtx_index);

        if (it == std::end(lru_cache)) {
            lru_cache.push_back(vtx_index);
            it = std::begin(lru_cache);
        }

        if (it != std::begin(lru_cache)) {
            lru_cache.erase(it);
            lru_cache.push_front(vtx_index);
        }
    };

    auto enforce_size = [&get_vertex_score](std::deque<uint32_t> &lru_cache, vtx_data_t *vertices, uint32_t max_size, std::vector<uint32_t> &out_tris_to_update) {
        out_tris_to_update.clear();

        if (lru_cache.size() > max_size) {
            lru_cache.resize(max_size);
        }

        for (size_t i = 0; i < lru_cache.size(); i++) {
            auto &v = vertices[lru_cache[i]];

            v.cache_pos = (int32_t)i;
            v.score = get_vertex_score(v.cache_pos, v.active_tris_count);

            for (uint32_t j = 0; j < v.ref_count; j++) {
                int tri_index = v.tris[j];
                if (tri_index != -1) {
                    auto it = std::find(std::begin(out_tris_to_update), std::end(out_tris_to_update), tri_index);
                    if (it == std::end(out_tris_to_update)) {
                        out_tris_to_update.push_back(tri_index);
                    }
                }
            }
        }
    };

    for (int32_t out_index = 0; out_index < (int32_t)indices_count; ) {
        if (next_best_index < 0) {
            next_best_score = next_next_best_score = -1.0f;
            next_best_index = next_next_best_index = -1;

            for (int32_t i = 0; i < (int32_t)prim_count; i++) {
                auto &tri = triangles[i];

                if (!tri.is_in_list) {
                    if (tri.score > next_best_score) {
                        if (next_best_score > next_next_best_score) {
                            next_next_best_index = next_best_index;
                            next_next_best_score = next_best_score;
                        }
                        next_best_index = i;
                        next_best_score = tri.score;
                    }

                    if (tri.score > next_next_best_score) {
                        next_next_best_index = i;
                        next_next_best_score = tri.score;
                    }
                }
            }
        }

        auto &next_best_tri = triangles[next_best_index];

        for (int32_t j = 0; j < 3; j++) {
            out_indices[out_index++] = next_best_tri.indices[j];

            auto &v = vertices[next_best_tri.indices[j]];
            v.active_tris_count--;
            for (uint32_t k = 0; k < v.ref_count; k++) {
                if (v.tris[k] == next_best_index) {
                    v.tris[k] = -1;
                    break;
                }
            }

            use_vertex(lru_cache, next_best_tri.indices[j]);
        }

        next_best_tri.is_in_list = true;

        std::vector<uint32_t> tris_to_update;
        enforce_size(lru_cache, &vertices[0], MaxSizeVertexCache, tris_to_update);

        next_best_score = -1.0f;
        next_best_index = -1;

        for (const auto ti : tris_to_update) {
            auto &tri = triangles[ti];

            if (!tri.is_in_list) {
                tri.score = vertices[tri.indices[0]].score + vertices[tri.indices[1]].score + vertices[tri.indices[2]].score;

                if (tri.score > next_best_score) {
                    if (next_best_score > next_next_best_score) {
                        next_next_best_index = next_best_index;
                        next_next_best_score = next_best_score;
                    }
                    next_best_index = ti;
                    next_best_score = tri.score;
                }

                if (tri.score > next_next_best_score) {
                    next_next_best_index = ti;
                    next_next_best_score = tri.score;
                }
            }
        }

        if (next_best_index == -1 && next_next_best_index != -1) {
            if (!triangles[next_next_best_index].is_in_list) {
                next_best_index = next_next_best_index;
                next_best_score = next_next_best_score;
            }

            next_next_best_index = -1;
            next_next_best_score = -1.0f;
        }
    }
}

void Ren::ComputeTextureBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> &new_vtx_indices,
                              const uint32_t *indices, size_t indices_count) {
    const float flt_eps = 0.0000001f;

    std::vector<std::array<uint32_t, 3>> twin_verts(vertices.size(), { 0, 0, 0 });
    std::vector<Vec3f> binormals(vertices.size());
    for (size_t i = 0; i < indices_count; i += 3) {
        auto *v0 = &vertices[indices[i + 0]];
        auto *v1 = &vertices[indices[i + 1]];
        auto *v2 = &vertices[indices[i + 2]];

        auto &b0 = binormals[indices[i + 0]];
        auto &b1 = binormals[indices[i + 1]];
        auto &b2 = binormals[indices[i + 2]];

        Vec3f dp1 = MakeVec3(v1->p) - MakeVec3(v0->p);
        Vec3f dp2 = MakeVec3(v2->p) - MakeVec3(v0->p);

        Vec2f dt1 = MakeVec2(v1->t[0]) - MakeVec2(v0->t[0]);
        Vec2f dt2 = MakeVec2(v2->t[0]) - MakeVec2(v0->t[0]);

        Vec3f tangent, binormal;

        float det = dt1[0] * dt2[1] - dt1[1] * dt2[0];
        if (std::abs(det) > flt_eps) {
            float inv_det = 1.0f / det;
            tangent = (dp1 * dt2[1] - dp2 * dt1[1]) * inv_det;
            binormal = (dp2 * dt1[0] - dp1 * dt2[0]) * inv_det;
        } else {
            Vec3f plane_N = Cross(dp1, dp2);
            tangent = Vec3f{ 0.0f, 1.0f, 0.0f };
            if (std::abs(plane_N[0]) <= std::abs(plane_N[1]) && std::abs(plane_N[0]) <= std::abs(plane_N[2])) {
                tangent = Vec3f{ 1.0f, 0.0f, 0.0f };
            } else if (std::abs(plane_N[2]) <= std::abs(plane_N[0]) && std::abs(plane_N[2]) <= std::abs(plane_N[1])) {
                tangent = Vec3f{ 0.0f, 0.0f, 1.0f };
            }

            binormal = Normalize(Cross(Vec3f(plane_N), tangent));
            tangent = Normalize(Cross(Vec3f(plane_N), binormal));
        }

        int i1 = (v0->b[0] * tangent[0] + v0->b[1] * tangent[1] + v0->b[2] * tangent[2]) < 0;
        int i2 = 2 * (b0[0] * binormal[0] + b0[1] * binormal[1] + b0[2] * binormal[2] < 0);

        if (i1 || i2) {
            uint32_t index = twin_verts[indices[i + 0]][i1 + i2 - 1];
            if (index == 0) {
                index = (uint32_t)(vertices.size());
                vertices.push_back(*v0);
                memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                twin_verts[indices[i + 0]][i1 + i2 - 1] = index;

                v1 = &vertices[indices[i + 1]];
                v2 = &vertices[indices[i + 2]];
            }
            new_vtx_indices[i] = index;
            v0 = &vertices[index];
        } else {
            b0 = binormal;
        }

        v0->b[0] += tangent[0];
        v0->b[1] += tangent[1];
        v0->b[2] += tangent[2];

        i1 = v1->b[0] * tangent[0] + v1->b[1] * tangent[1] + v1->b[2] * tangent[2] < 0;
        i2 = 2 * (b1[0] * binormal[0] + b1[1] * binormal[1] + b1[2] * binormal[2] < 0);

        if (i1 || i2) {
            uint32_t index = twin_verts[indices[i + 1]][i1 + i2 - 1];
            if (index == 0) {
                index = (uint32_t)(vertices.size());
                vertices.push_back(*v1);
                memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                twin_verts[indices[i + 1]][i1 + i2 - 1] = index;

                v0 = &vertices[indices[i + 0]];
                v2 = &vertices[indices[i + 2]];
            }
            new_vtx_indices[i + 1] = index;
            v1 = &vertices[index];
        } else {
            b1 = binormal;
        }

        v1->b[0] += tangent[0];
        v1->b[1] += tangent[1];
        v1->b[2] += tangent[2];

        i1 = v2->b[0] * tangent[0] + v2->b[1] * tangent[1] + v2->b[2] * tangent[2] < 0;
        i2 = 2 * (b2[0] * binormal[0] + b2[1] * binormal[1] + b2[2] * binormal[2] < 0);

        if (i1 || i2) {
            uint32_t index = twin_verts[indices[i + 2]][i1 + i2 - 1];
            if (index == 0) {
                index = (uint32_t)(vertices.size());
                vertices.push_back(*v2);
                memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                twin_verts[indices[i + 2]][i1 + i2 - 1] = index;

                v0 = &vertices[indices[i + 0]];
                v1 = &vertices[indices[i + 1]];
            }
            new_vtx_indices[i + 2] = index;
            v2 = &vertices[index];
        } else {
            b2 = binormal;
        }

        v2->b[0] += tangent[0];
        v2->b[1] += tangent[1];
        v2->b[2] += tangent[2];
    }

    for (auto &v : vertices) {
        if (std::abs(v.b[0]) > flt_eps || std::abs(v.b[1]) > flt_eps || std::abs(v.b[2]) > flt_eps) {
            Vec3f tangent = MakeVec3(v.b);
            Vec3f binormal = Cross(MakeVec3(v.n), tangent);
            float l = Length(binormal);
            if (l > flt_eps) {
                binormal /= l;
                memcpy(&v.b[0], &binormal[0], 3 * sizeof(float));
            }
        }
    }
}