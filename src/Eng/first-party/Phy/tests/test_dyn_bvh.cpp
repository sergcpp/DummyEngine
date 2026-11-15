#include "test_common.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

#include "../Bounds.h"
#include "../DynBVH.h"

namespace DynBVHTests {
using namespace Phy;

uint32_t next_rand(uint32_t &seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

float rand_range(uint32_t &seed, const float lo, const float hi) {
    return lo + (hi - lo) * (float(next_rand(seed) >> 8) / float(0xFFFFFFu));
}

float rand_int(uint32_t &seed, const int count) {
    return float(next_rand(seed) % uint32_t(count));
}

dyn_bvh_settings_t force_split_settings() {
    dyn_bvh_settings_t s;
    // Exact settings used by Eng SceneManager::RebuildSceneBVH
    s.split.oversplit_threshold = std::numeric_limits<float>::max();
    s.split.min_primitives_in_leaf = 1;
    return s;
}

std::vector<dyn_bvh_prim_t> make_random_scene(const int count, uint32_t &seed, const float spread, const float ext_min,
                                              const float ext_max) {
    std::vector<dyn_bvh_prim_t> prims;
    prims.reserve(count);
    for (int i = 0; i < count; i++) {
        const Vec3f center{rand_range(seed, -spread, spread), rand_range(seed, -spread, spread),
                           rand_range(seed, -spread, spread)};
        const float ex = rand_range(seed, ext_min, ext_max);
        const float ey = rand_range(seed, ext_min, ext_max);
        const float ez = rand_range(seed, ext_min, ext_max);
        const Vec3f bmin{center[0] - 0.5f * ex, center[1] - 0.5f * ey, center[2] - 0.5f * ez};
        const Vec3f bmax{center[0] + 0.5f * ex, center[1] + 0.5f * ey, center[2] + 0.5f * ez};
        prims.push_back({bmin, bmax, uint32_t(i)});
    }
    return prims;
}

std::vector<dyn_bvh_prim_t> make_coincident_prims(const int count, const Vec3f &bmin, const Vec3f &bmax) {
    std::vector<dyn_bvh_prim_t> prims;
    prims.reserve(count);
    for (int i = 0; i < count; i++) {
        prims.push_back({bmin, bmax, uint32_t(i)});
    }
    return prims;
}

std::vector<dyn_bvh_prim_t> make_grid_scene(const int dim) {
    std::vector<dyn_bvh_prim_t> prims;
    for (int z = 0; z < dim; z++) {
        for (int y = 0; y < dim; y++) {
            for (int x = 0; x < dim; x++) {
                const int i = z * dim * dim + y * dim + x;
                const Vec3f bmin{float(x) * 2.0f, float(y) * 2.0f, float(z) * 2.0f};
                prims.push_back({bmin, Vec3f{bmin[0] + 1.0f, bmin[1] + 1.0f, bmin[2] + 1.0f}, uint32_t(i)});
            }
        }
    }
    return prims;
}

Bounds scene_bounds(const std::vector<dyn_bvh_prim_t> &prims) {
    Bounds b;
    for (const dyn_bvh_prim_t &p : prims) {
        b.Expand(p.bbox_min);
        b.Expand(p.bbox_max);
    }
    return b;
}

// leaf index per prim_id (BVH_NO_NODE for missing prims)
std::vector<uint32_t> leaf_map(const DynBVH &bvh, const int num_prims) {
    std::vector<uint32_t> map(size_t(num_prims), BVH_NO_NODE);
    const auto nodes = bvh.nodes();
    for (size_t i = 0; i < size_t(nodes.size()); i++) {
        if (bvh_is_leaf(nodes[i])) {
            const uint32_t pid = bvh_prim_id(nodes[i]);
            require(pid < uint32_t(num_prims));
            require(map[pid] == BVH_NO_NODE);
            map[pid] = uint32_t(i);
        }
    }
    return map;
}

// Every prim_id in [0, num_prims) must appear at exactly one leaf
void require_full_coverage(const DynBVH &bvh, const int num_prims) {
    const auto map = leaf_map(bvh, num_prims);
    for (const uint32_t m : map) {
        require(m != BVH_NO_NODE);
    }
}

// Ray probe rig: grid-sampled ortho stack traversal (quality + stack-safety gates)

struct probe_result {
    std::vector<int> visits; // per grid sample
    std::vector<std::vector<int>> dir_visits; // per-direction res*res grids, for the debug TGA
    int total_visits = 0;
    int stack_hwm = 0;
    int max_depth = 0;
};

struct ray_ws {
    Vec3f inv, neg_o_inv;
};

ray_ws make_ray(const Vec3f &o, const Vec3f &d) {
    ray_ws r;
    r.inv = Vec3f{1.0f} / d;
    r.neg_o_inv = -o * r.inv;
    return r;
}

bool ray_hits_box(const ray_ws &r, const Vec3f &bmin, const Vec3f &bmax) {
    float tmin = -1e30f, tmax = 1e30f;
    for (int a = 0; a < 3; a++) {
        float t1 = bmin[a] * r.inv[a] + r.neg_o_inv[a];
        float t2 = bmax[a] * r.inv[a] + r.neg_o_inv[a];
        if (t1 > t2) {
            const float t = t1;
            t1 = t2;
            t2 = t;
        }
        tmin = t1 > tmin ? t1 : tmin;
        tmax = t2 < tmax ? t2 : tmax;
        if (tmin > tmax) {
            return false;
        }
    }
    return true;
}

// Stack traversal mirroring production gather (bbox test, no frustum skip flags)
probe_result probe_grid(const DynBVH &bvh, const Vec3f &o0, const Vec3f &step_u, const Vec3f &step_v, const Vec3f &dir,
                        const int res) {
    probe_result pr;
    pr.visits.assign(size_t(res) * size_t(res), 0);
    if (bvh.root() == BVH_NO_NODE) {
        return pr;
    }
    const auto nodes = bvh.nodes();

    std::vector<uint32_t> stack;
    std::vector<int> depth_stack;
    stack.reserve(256);
    depth_stack.reserve(256);
    for (int j = 0; j < res; j++) {
        for (int i = 0; i < res; i++) {
            const Vec3f o = o0 + step_u * Vec3f{float(i)} + step_v * Vec3f{float(j)};
            const ray_ws r = make_ray(o, dir);

            stack.clear();
            depth_stack.clear();
            stack.push_back(bvh.root());
            depth_stack.push_back(0);
            if (int(stack.size()) > pr.stack_hwm) {
                pr.stack_hwm = int(stack.size());
            }

            int v = 0;
            while (!stack.empty()) {
                const uint32_t idx = stack.back();
                stack.pop_back();
                const int depth = depth_stack.back();
                depth_stack.pop_back();
                const dyn_bvh_node_t &n = nodes[idx];
                if (!ray_hits_box(r, n.bbox_min, n.bbox_max)) {
                    continue;
                }
                v++;
                if (depth > pr.max_depth) {
                    pr.max_depth = depth;
                }
                if (bvh_is_leaf(n)) {
                    continue;
                }
                stack.push_back(bvh_left_child(n));
                depth_stack.push_back(depth + 1);
                stack.push_back(bvh_right_child(n));
                depth_stack.push_back(depth + 1);
                if (int(stack.size()) > pr.stack_hwm) {
                    pr.stack_hwm = int(stack.size());
                }
            }
            pr.visits[size_t(j) * size_t(res) + size_t(i)] = v;
            pr.total_visits += v;
        }
    }
    return pr;
}

// Grids along +X, +Y, +Z and a diagonal; axis dirs get a tiny tilt so inv_d stays finite
struct probe_rig {
    Vec3f dir, o0, step_u, step_v;
};

// Axis dirs sample the bbox min face; the diag samples a plane through the scene center
// perpendicular to the ray dir, its grid square circumscribing the scene bounding sphere,
// so the tile shows a true orthographic projection of the scene along the diagonal
probe_rig make_probe_rig(const Bounds &sb, const int dir_kind, const int res) {
    const float ex = sb.maxs[0] - sb.mins[0];
    const float ey = sb.maxs[1] - sb.mins[1];
    const float ez = sb.maxs[2] - sb.mins[2];
    const float pad = 0.05f * Max(Max(ex, ey), ez) + 1.0f;

    probe_rig rig;
    if (dir_kind < 3) {
        const int w_axis = dir_kind;
        const int u = (w_axis + 1) % 3;
        const int v = (w_axis + 2) % 3;

        Vec3f dir{0.0f};
        dir[w_axis] = 1.0f;
        dir[u] = 1e-6f;
        dir[v] = -1e-6f;

        Vec3f o0{0.0f};
        o0[w_axis] = sb.mins[w_axis] - pad;
        o0[u] = sb.mins[u] - pad;
        o0[v] = sb.mins[v] - pad;

        Vec3f step_u{0.0f}, step_v{0.0f};
        step_u[u] = (sb.maxs[u] - sb.mins[u] + 2.0f * pad) / float(res - 1);
        step_v[v] = (sb.maxs[v] - sb.mins[v] + 2.0f * pad) / float(res - 1);

        rig.dir = dir;
        rig.o0 = o0;
        rig.step_u = step_u;
        rig.step_v = step_v;
    } else {
        // u,v: orthonormal screen basis perpendicular to d; v = cross(d, u)
        const float s3 = 1.0f / std::sqrt(3.0f), s2 = 1.0f / std::sqrt(2.0f), s6 = 1.0f / std::sqrt(6.0f);
        const Vec3f d{s3, s3, s3}, u{s2, -s2, 0.0f}, v{s6, s6, -2.0f * s6};
        const Vec3f center = (sb.mins + sb.maxs) * Vec3f{0.5f};
        const float reach = 0.5f * std::sqrt(ex * ex + ey * ey + ez * ez) + pad;
        const float du = 2.0f * reach / float(res - 1);

        rig.dir = d;
        rig.o0 = center - d * Vec3f{reach} - u * Vec3f{reach} - v * Vec3f{reach};
        rig.step_u = u * Vec3f{du};
        rig.step_v = v * Vec3f{du};
    }
    return rig;
}

probe_result probe_all_axes(const DynBVH &bvh, const Bounds &sb, const int res) {
    probe_result total;
    for (int dir_kind = 0; dir_kind < 4; dir_kind++) {
        const probe_rig rig = make_probe_rig(sb, dir_kind, res);
        const probe_result pr = probe_grid(bvh, rig.o0, rig.step_u, rig.step_v, rig.dir, res);
        total.dir_visits.push_back(pr.visits);
        total.total_visits += pr.total_visits;
        total.stack_hwm = Max(total.stack_hwm, pr.stack_hwm);
        total.max_depth = Max(total.max_depth, pr.max_depth);
    }
    return total;
}

// Traversal must report at least every prim whose raw bbox the ray intersects
void require_probe_completeness(const DynBVH &bvh, const std::vector<dyn_bvh_prim_t> &prims, const Bounds &sb) {
    const int res = 24;
    std::vector<uint8_t> reported(prims.size(), 0);
    const auto nodes = bvh.nodes();

    for (int dir_kind = 0; dir_kind < 4; dir_kind++) {
        const probe_rig rig = make_probe_rig(sb, dir_kind, res);

        std::vector<uint32_t> stack;
        stack.reserve(256);
        for (int j = 0; j < res; j++) {
            for (int i = 0; i < res; i++) {
                const Vec3f o = rig.o0 + rig.step_u * Vec3f{float(i)} + rig.step_v * Vec3f{float(j)};
                const ray_ws r = make_ray(o, rig.dir);

                std::fill(reported.begin(), reported.end(), uint8_t(0));
                stack.clear();
                stack.push_back(bvh.root());
                while (!stack.empty()) {
                    const dyn_bvh_node_t &n = nodes[stack.back()];
                    stack.pop_back();
                    if (!ray_hits_box(r, n.bbox_min, n.bbox_max)) {
                        continue;
                    }
                    if (bvh_is_leaf(n)) {
                        reported[bvh_prim_id(n)] = 1;
                    } else {
                        stack.push_back(bvh_left_child(n));
                        stack.push_back(bvh_right_child(n));
                    }
                }

                for (const dyn_bvh_prim_t &p : prims) {
                    if (ray_hits_box(r, p.bbox_min, p.bbox_max)) {
                        require(reported[p.prim_id] == 1);
                    }
                }
            }
        }
    }
}
#define float_to_byte(val)                                                                                             \
    (((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : uint8_t((255.0f * (val)) + 0.5f)))

// Uncompressed 24-bit TGA heat map of per-direction probe visit grids: 2x2 tiles (+X, +Y top row; +Z, diag bottom)
// Single mean+/-2 sigma window over all grids (mean = mid green), so equal visit counts get
// equal colors across tiles (root-miss rays look the same everywhere)
void write_probe_tga(const char *path, const probe_result &pr, const int res) {
    const int dim = 2 * res + 1; // 2x2 grids, 1px black gap

    float sum = 0.0f, count = 0.0f;
    for (const auto &g : pr.dir_visits) {
        for (const int v : g) {
            sum += float(v);
            count += 1.0f;
        }
    }
    float var = 0.0f;
    for (const auto &g : pr.dir_visits) {
        for (const int v : g) {
            const float dv = float(v) - sum / count;
            var += dv * dv;
        }
    }
    const float mid = sum / count;
    const float sigma = std::sqrt(var / count);
    const float inv_range = 4.0f * (sigma > 0.5f ? sigma : 0.5f);

    auto img = std::make_unique<uint8_t[]>(size_t(dim) * dim * 3); // BGR
    for (int y = 0; y < dim; ++y) {
        for (int x = 0; x < dim; ++x) {
            uint8_t *px = &img[(y * dim + x) * 3];
            if (x != res && y != res) {
                const int gx = x > res ? x - res - 1 : x, gy = y > res ? y - res - 1 : y;
                const int tile = (y > res ? 2 : 0) + (x > res ? 1 : 0); // dir_visits[tile] grid
                const int val = pr.dir_visits[size_t(tile)][size_t(gy * res + gx)];
                const float t = (float(val) - mid) / inv_range + 0.5f;
                const float cr = t > 0.5f ? (t - 0.5f) * 2.0f : 0.0f;
                const float cg = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
                const float cb = t < 0.5f ? (0.5f - t) * 2.0f : 0.0f;
                px[0] = float_to_byte(cb);
                px[1] = float_to_byte(cg);
                px[2] = float_to_byte(cr);
            }
        }
    }

    std::ofstream file(path, std::ios::binary);

    unsigned char header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    header[12] = dim & 0xFF;
    header[13] = (dim >> 8) & 0xFF;
    header[14] = (dim)&0xFF;
    header[15] = (dim >> 8) & 0xFF;
    header[16] = 3 * 8;
    header[17] |= (1 << 5); // set origin to upper left corner

    file.write((char *)&header[0], sizeof(header));
    file.write((const char *)&img[0], size_t(dim) * dim * 3);

    static const char footer[26] = "\0\0\0\0"         // no extension area
                                   "\0\0\0\0"         // no developer directory
                                   "TRUEVISION-XFILE" // yep, this is a TGA file
                                   ".";
    file.write(footer, sizeof(footer));
}
} // namespace DynBVHTests

void test_dyn_bvh() {
    using namespace DynBVHTests;
    printf("Test dyn_bvh            | ");

    { // node word accessors: round-trips and masks
        dyn_bvh_node_t n{};
        bvh_set_leaf(n, 42u, 1u);
        require(bvh_is_leaf(n));
        require(bvh_prim_id(n) == 42u);
        require(bvh_prim_count(n) == 1u);
        require(bvh_left_child(n) == 42u); // same bit field as prim_id, meaningless on leaves

        bvh_set_internal(n, 7u, 9u, 2u);
        require(!bvh_is_leaf(n));
        require(bvh_left_child(n) == 7u);
        require(bvh_right_child(n) == 9u);
        require(bvh_sep_axis(n) == 2u);

        bvh_set_left_child(n, 11u);
        require(bvh_left_child(n) == 11u);
        require(bvh_right_child(n) == 9u);
        require(bvh_sep_axis(n) == 2u);

        bvh_set_right_child(n, 13u);
        require(bvh_left_child(n) == 11u);
        require(bvh_right_child(n) == 13u);
        require(bvh_sep_axis(n) == 2u);

        bvh_set_leaf(n, BVH_INDEX_MASK, 1u);
        require(bvh_prim_id(n) == BVH_INDEX_MASK);
        bvh_set_internal(n, 0u, BVH_COUNT_MASK, 0u);
        require(bvh_right_child(n) == BVH_COUNT_MASK);
    }
    { // empty build
        DynBVH t;
        t.Build(Span<const dyn_bvh_prim_t>(), force_split_settings());
        require(t.root() == BVH_NO_NODE);
        require(t.ValidateBVH());
        require(t.insertions_since_build() == 0);
    }
    { // single-prim build: root is a leaf
        DynBVH t;
        const auto prims = make_coincident_prims(1, Vec3f{1.0f, 1.0f, 1.0f}, Vec3f{3.0f, 5.0f, 7.0f});
        t.Build(prims, force_split_settings());
        require(t.root() != BVH_NO_NODE);
        require(bvh_is_leaf(t.nodes()[t.root()]));
        require(bvh_prim_id(t.nodes()[t.root()]) == 0u);
        require(t.ValidateBVH());
        require_full_coverage(t, 1);
    }
    { // build invariants: grid / clusters / coincident / flat
        uint32_t seed = 0xBADC0DEu;
        DynBVH g;
        g.Build(make_grid_scene(5), force_split_settings());
        require(g.ValidateBVH());
        require_full_coverage(g, 125);

        DynBVH c;
        std::vector<dyn_bvh_prim_t> cluster_a = make_random_scene(500, seed, 20.0f, 0.2f, 1.0f);
        std::vector<dyn_bvh_prim_t> cluster_b = make_random_scene(500, seed, 20.0f, 0.2f, 1.0f);
        for (auto &p : cluster_b) {
            p.prim_id += 500u;  // prim_ids must be unique across the whole built set
            p.bbox_min = p.bbox_min + Vec3f{500.0f, 0.0f, 0.0f};
            p.bbox_max = p.bbox_max + Vec3f{500.0f, 0.0f, 0.0f};
        }
        for (auto &p : cluster_b) {
            cluster_a.push_back(p);
        }
        c.Build(cluster_a, force_split_settings());
        require(c.ValidateBVH());
        require_full_coverage(c, 1000);

        DynBVH x;
        x.Build(make_coincident_prims(1100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f}), force_split_settings());
        require(x.ValidateBVH());
        require_full_coverage(x, 1100);

        DynBVH f;
        auto flat = make_random_scene(1100, seed, 100.0f, 0.2f, 2.0f);
        for (auto &p : flat) {
            p.bbox_min[2] = 0.0f;
            p.bbox_max[2] = 0.0f;
        }
        f.Build(flat, force_split_settings());
        require(f.ValidateBVH());
        require_full_coverage(f, 1100);
    }
    { // root-leaf removal (emptying tree) + reinsert: free-list bookkeeping
        DynBVH t;
        auto prims = make_coincident_prims(1, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{1.0f, 1.0f, 1.0f});
        t.Build(prims, force_split_settings());
        const uint32_t root_leaf = t.root();

        prims[0].bbox_min = prims[0].bbox_min + Vec3f{100.0f, 0.0f, 0.0f};
        prims[0].bbox_max = prims[0].bbox_max + Vec3f{100.0f, 0.0f, 0.0f};
        require(!t.LeafFits(root_leaf, prims[0].bbox_min, prims[0].bbox_max));

        t.RemoveLeaf(root_leaf);
        require(t.root() == BVH_NO_NODE);
        require(t.free_count() == 1);
        require(t.ValidateBVH());

        const uint32_t new_leaf = t.InsertPrim(prims[0].bbox_min, prims[0].bbox_max, 0u);
        require(t.root() == new_leaf);
        require(bvh_is_leaf(t.nodes()[new_leaf]));
        require(t.free_count() == 0);
        require(t.ValidateBVH());

        const uint32_t second = t.InsertPrim(prims[0].bbox_max + Vec3f{50.0f, 0.0f, 0.0f},
                                             prims[0].bbox_max + Vec3f{51.0f, 1.0f, 1.0f}, 1u);
        require(!bvh_is_leaf(t.nodes()[t.root()]));
        require(t.InsertPrim(prims[0].bbox_max + Vec3f{0.0f, 80.0f, 0.0f}, prims[0].bbox_max + Vec3f{1.0f, 81.0f, 1.0f},
                             2u) != BVH_NO_NODE);
        (void)second;
        require(t.ValidateBVH());
        require_full_coverage(t, 3);
    }
    { // RefitFromLeaf: exact translated bounds + consistent hierarchy (world scrolling)
        DynBVH t;
        const auto prims = make_grid_scene(4);
        t.Build(prims, force_split_settings());
        const auto map = leaf_map(t, int(prims.size()));

        const Vec3f shift{5.0f, -3.0f, 7.0f};
        for (const dyn_bvh_prim_t &p : prims) {
            t.RefitFromLeaf(map[p.prim_id], p.bbox_min + shift, p.bbox_max + shift);
        }
        require(t.ValidateBVH());

        const auto nodes = t.nodes();
        for (const dyn_bvh_prim_t &p : prims) {
            const dyn_bvh_node_t &leaf = nodes[map[p.prim_id]];
            const Vec3f d = ((p.bbox_max + shift) - (p.bbox_min + shift)) * Vec3f{0.2f};
            require(leaf.bbox_min[0] == (p.bbox_min + shift)[0] - d[0]);
            require(leaf.bbox_max[2] == (p.bbox_max + shift)[2] + d[2]);
        }
    }
    { // max_depth: empty tree, root leaf, balanced bulk build, degenerate cluster, churn bound
        DynBVH empty;
        require(empty.max_depth() == 0);

        DynBVH single;
        const auto one_prim = make_coincident_prims(1, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{1.0f, 1.0f, 1.0f});
        single.Build(one_prim, force_split_settings());
        require(single.max_depth() == 1);

        const auto grid = make_grid_scene(4);  // 64 well-separated prims
        DynBVH grid_tree;
        grid_tree.Build(grid, force_split_settings());
        require(grid_tree.ValidateBVH());

        const auto degenerate = make_coincident_prims(64, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{1.0f, 1.0f, 1.0f});
        DynBVH deg_tree;
        deg_tree.Build(degenerate, force_split_settings());
        require(deg_tree.ValidateBVH());

        // Measured: 64 separated prims -> 7 levels (ideal 6); 64 coincident prims -> 39 (the
        // degenerate-cluster fallback path, still far below the 64-level linear chain)
        require(grid_tree.max_depth() >= 6 && grid_tree.max_depth() <= 10);
        require(deg_tree.max_depth() >= 6 && deg_tree.max_depth() < uint32_t(degenerate.size()));

        // Churn can lengthen the longest path by at most one level per insertion
        const uint32_t grid_depth0 = grid_tree.max_depth();
        const auto gmap = leaf_map(grid_tree, int(grid.size()));
        for (int i = 0; i < 8; i++) {
            const dyn_bvh_prim_t &p = grid[size_t(i)];
            grid_tree.RemoveLeaf(gmap[p.prim_id]);
            grid_tree.InsertPrim(p.bbox_max + Vec3f{500.0f, 0.0f, 0.0f}, p.bbox_max + Vec3f{501.0f, 1.0f, 1.0f},
                                 p.prim_id);
        }
        require(grid_tree.ValidateBVH());
        require(grid_tree.max_depth() <= grid_depth0 + 8);
    }
    { // lifecycle fuzz: seeded moves, full validation after every step
        uint32_t seed = 0x1234ABCDu;
        const int n = 256;
        auto prims = make_random_scene(n, seed, 100.0f, 0.2f, 2.0f);
        DynBVH t;
        t.Build(prims, force_split_settings());
        require(t.ValidateBVH());

        std::vector<Vec3f> cur_min, cur_max;
        cur_min.resize(size_t(n));
        cur_max.resize(size_t(n));
        for (int i = 0; i < n; i++) {
            cur_min[size_t(i)] = prims[size_t(i)].bbox_min;
            cur_max[size_t(i)] = prims[size_t(i)].bbox_max;
        }
        std::vector<uint32_t> leaf_of = leaf_map(t, n);

        for (int step = 0; step < 400; step++) {
            for (int m = 0; m < 8; m++) {
                const int pid = int(rand_int(seed, n));
                const Vec3f delta{rand_range(seed, -30.0f, 30.0f), rand_range(seed, -30.0f, 30.0f),
                                  rand_range(seed, -30.0f, 30.0f)};
                cur_min[size_t(pid)] = cur_min[size_t(pid)] + delta;
                cur_max[size_t(pid)] = cur_max[size_t(pid)] + delta;

                if (!t.LeafFits(leaf_of[size_t(pid)], cur_min[size_t(pid)], cur_max[size_t(pid)])) {
                    t.RemoveLeaf(leaf_of[size_t(pid)]);
                    leaf_of[size_t(pid)] = t.InsertPrim(cur_min[size_t(pid)], cur_max[size_t(pid)], uint32_t(pid));
                }
            }
            require(t.ValidateBVH());
        }
        require(t.insertions_since_build() > 0);
        require(t.bnb_expansions() > 0);    // BnB must actually descend (D1 regression anchor)
        require(t.rotations_applied() > 0); // refit rotations must actually run (D1 regression anchor)

        { // probe the fuzzed tree: stack bound, quality vs fresh rebuild, completeness
            std::vector<dyn_bvh_prim_t> current;
            current.resize(size_t(n));
            for (int i = 0; i < n; i++) {
                current[size_t(i)] = {cur_min[size_t(i)], cur_max[size_t(i)], uint32_t(i)};
            }
            const Bounds sb = scene_bounds(current);

            const int probe_res = 64; // diag footprint is ~sqrt(3) wider than axis ones, same res keeps its detail
            const probe_result inc = probe_all_axes(t, sb, probe_res);
            DynBVH fresh;
            fresh.Build(current, force_split_settings());
            const probe_result ref = probe_all_axes(fresh, sb, probe_res);

            require(inc.stack_hwm <= 40);
            require(ref.stack_hwm <= 40);
            // Measured (this seed, 400 churn steps, 64x64 grids): ratio 1.019, stack HWM 8.
            // The 1.25 cap fails hard on the old always-insert-at-root behavior (ratio grew ~linearly).
            require(float(inc.total_visits) <= 1.25f * float(ref.total_visits));

            require_probe_completeness(t, current, sb);

            write_probe_tga("dyn_bvh_probe_churned.tga", inc, probe_res);
            write_probe_tga("dyn_bvh_probe_fresh.tga", ref, probe_res);
        }
    }
    { // dense overlapping cluster insertions (exercises refit rotations under degenerate SAH)
        uint32_t seed = 0xFEEDFACEu;
        auto prims = make_random_scene(150, seed, 1.0f, 0.1f, 0.4f);
        DynBVH t;
        t.Build(prims, force_split_settings());
        require(t.ValidateBVH());

        for (uint32_t pid = 150; pid < 350; pid++) {
            auto extra = make_random_scene(1, seed, 1.0f, 0.1f, 0.4f);
            extra[0].prim_id = pid;
            t.InsertPrim(extra[0].bbox_min, extra[0].bbox_max, pid);
            require(t.ValidateBVH());
        }
        require(t.bnb_expansions() > 0);
        require_full_coverage(t, 350);
    }

    printf("OK\n");
}
