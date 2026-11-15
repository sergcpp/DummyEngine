#include <cmath>
#include <cstdio>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(REN_VK_BACKEND)
#include <Ren/Vk/VKCtx.h>
#elif defined(REN_GL_BACKEND)
#include <Ren/Gl/GLCtx.h>
#endif
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/ThreadPool.h>

#include "../scene/SceneManager.h"
#include "../scene/components/Physics.h"
#include "../scene/components/Transform.h"
#include "../utils/ShaderLoader.h"

#include "test_common.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

namespace {
using namespace Eng;

bool load_test_scene(const std::string &path, Sys::MultiPoolAllocator<char> &alloc, Sys::JsObjectP &out_js) {
    Sys::AssetFile in_scene(path);
    if (!in_scene) {
        return false;
    }
    const size_t scene_size = in_scene.size();
    std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
    in_scene.Read((char *)&scene_data[0], scene_size);

    Sys::MemBuf mem(&scene_data[0], scene_size);
    std::istream in_stream(&mem);
    return out_js.Read(in_stream);
}

Transform *scene_transforms(SceneData &sd) {
    return (Transform *)sd.comp_store[CompTransform]->SequentialData();
}

// Adapter bijection gate: every transform object's node_index points to a leaf whose prim_id
// is the object index, and the leaves cover exactly the transform-object set (D4).
bool check_leaf_mapping(SceneData &sd) {
    const auto *transforms = scene_transforms(sd);
    const auto nodes = sd.bvh.nodes();

    uint32_t transform_count = 0, leaf_count = 0;
    bool ok = true;

    for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
        if (!(sd.objects[i].comp_mask & CompTransformBit)) {
            continue;
        }
        transform_count++;
        const Transform &tr = transforms[sd.objects[i].components[CompTransform]];
        if (tr.node_index >= uint32_t(nodes.size()) || !Phy::bvh_is_leaf(nodes[tr.node_index]) ||
            Phy::bvh_prim_id(nodes[tr.node_index]) != i) {
            ok = false;
        }
    }
    for (size_t n = 0; n < size_t(nodes.size()); n++) {
        leaf_count += Phy::bvh_is_leaf(nodes[n]) ? 1 : 0;
    }
    return ok && leaf_count == transform_count;
}

bool run_bvh_glue_tests(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    ShaderLoader shader_loader(ren_ctx);
    shader_loader.LoadPipelineCache("assets_pc/");

    path_config_t paths;
    SceneManager scene_manager(ren_ctx, shader_loader, nullptr, threads, paths);

    Sys::MultiPoolAllocator<char> alloc(32, 512);
    Sys::JsObjectP js_scene(alloc);
    if (!load_test_scene("assets_pc/scenes/complex_mat2.json", alloc, js_scene)) {
        return false;
    }
    scene_manager.LoadScene(js_scene);

    // Frame pump boilerplate, same as run_image_test
    auto begin_frame = [&ren_ctx]() {
        Ren::ApiContext &api = ren_ctx.api();

#if defined(REN_VK_BACKEND)
        api.vkWaitForFences(api.device, 1, &api.in_flight_fences[api.backend_frame], VK_TRUE, UINT64_MAX);
        api.vkResetFences(api.device, 1, &api.in_flight_fences[api.backend_frame]);

        ReadbackTimestampQueries(api, api.backend_frame);
        DestroyDeferredResources(api, api.backend_frame);

        const bool reset_result = ren_ctx.default_descr_alloc().Reset();
        require_fatal(reset_result);

        ///////////////////////////////////////////////////////////////

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        api.vkBeginCommandBuffer(api.draw_cmd_buf[api.backend_frame], &begin_info);
        api.curr_cmd_buf = api.draw_cmd_buf[api.backend_frame];

        api.vkCmdResetQueryPool(api.draw_cmd_buf[api.backend_frame], api.query_pools[api.backend_frame], 0,
                                Ren::MaxTimestampQueries);
#elif defined(REN_GL_BACKEND)
        // Make sure all operations have finished
        api.in_flight_fences[api.backend_frame].ClientWaitSync();
        api.in_flight_fences[api.backend_frame] = {};

        ReadbackTimestampQueries(api, api.backend_frame);
#endif
    };

    auto end_frame = [&ren_ctx]() {
        Ren::ApiContext &api = ren_ctx.api();

#if defined(REN_VK_BACKEND)
        api.vkEndCommandBuffer(api.draw_cmd_buf[api.backend_frame]);

        const int prev_frame = (api.backend_frame + Ren::MaxFramesInFlight - 1) % Ren::MaxFramesInFlight;

        //////////////////////////////////////////////////////////////////////////////////////////////////

        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

        const VkSemaphore wait_semaphores[] = {api.render_finished_semaphores[prev_frame]};
        const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        if (api.render_finished_semaphore_is_set[prev_frame]) {
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = wait_semaphores;
            submit_info.pWaitDstStageMask = wait_stages;
        }

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &api.draw_cmd_buf[api.backend_frame];

        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &api.render_finished_semaphores[api.backend_frame];

        VkResult res = api.vkQueueSubmit(api.graphics_queue, 1, &submit_info, api.in_flight_fences[api.backend_frame]);
        require_fatal(res == VK_SUCCESS);

        api.render_finished_semaphore_is_set[api.backend_frame] = true;
        api.render_finished_semaphore_is_set[prev_frame] = false;

#elif defined(REN_GL_BACKEND)
        api.in_flight_fences[api.backend_frame] = Ren::MakeFence();
#endif
        api.backend_frame = (api.backend_frame + 1) % Ren::MaxFramesInFlight;
    };

    { // Make sure all scene data is loaded, same pump as run_image_test (budget 1 + 1ms sleep:
        // a hot pump loop starves the texture loader thread on busy CI machines)
        bool finished = false;
        for (int frame = 0; frame < 100000 && !finished; frame++) {
            begin_frame();
            finished = scene_manager.Serve();
            end_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!require(finished)) {
            return false;
        }
    }

    size_t nodes_at_load = 0;
    { // E1: post-load tree structure and leaf <-> object bijection
        SceneData &sd = scene_manager.scene_data();
        nodes_at_load = sd.bvh.node_capacity();
        require(sd.bvh.ValidateBVH());

        // Depth feeds the debug BVH view's normalization; keep it inside the combinatorial band
        // for a full binary tree (a chain of leaves would fail the upper bound)
        const uint32_t leaf_count = uint32_t((nodes_at_load + 1) / 2);
        uint32_t ideal_levels = 0, pow2 = 1;
        while (pow2 < leaf_count) {
            pow2 *= 2;
            ideal_levels++;
        }
        const uint32_t tree_depth = sd.bvh.max_depth();
        require(tree_depth >= ideal_levels && tree_depth <= 2 * ideal_levels);
        require(check_leaf_mapping(sd));
        require(sd.bvh.insertions_since_build() == 0);
        if (!require(sd.bvh.root() != Phy::BVH_NO_NODE)) {
            return false;
        }
    }

    { // E2: CompTransformBit churn through InvalidateObjects + UpdateObjects
        SceneData &sd = scene_manager.scene_data();
        auto *transforms = scene_transforms(sd);

        // Largest ws-bbox object: widest hysteresis slack in its leaf
        uint32_t obj_index = 0xffffffff;
        float best_ext = -1.0f;
        for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
            if (!(sd.objects[i].comp_mask & CompTransformBit)) {
                continue;
            }
            const Transform &tr = transforms[sd.objects[i].components[CompTransform]];
            const float ext = Ren::Length(tr.bbox_max_ws - tr.bbox_min_ws);
            if (ext > best_ext) {
                best_ext = ext;
                obj_index = i;
            }
        }
        if (!require(obj_index != 0xffffffff)) {
            return false;
        }

        Transform &tr = transforms[sd.objects[obj_index].components[CompTransform]];
        if (!require(tr.node_index != 0xffffffff && tr.node_index < uint32_t(sd.bvh.node_capacity()))) {
            return false;
        }
        const uint32_t leaf_index = tr.node_index;
        const Phy::dyn_bvh_node_t old_leaf = sd.bvh.nodes()[leaf_index];

        { // Small move inside the padded leaf bounds: LeafFits keeps the leaf, no refit
            int move_axis = 0;
            float best_gap = old_leaf.bbox_max[0] - tr.bbox_max_ws[0];
            for (int a = 1; a < 3; a++) {
                const float gap = old_leaf.bbox_max[a] - tr.bbox_max_ws[a];
                if (gap > best_gap) {
                    best_gap = gap;
                    move_axis = a;
                }
            }
            if (!require(best_gap > 0.0f)) {
                return false;
            }

            tr.world_from_object[3][move_axis] += 0.5f * best_gap;
            tr.position[move_axis] += double(0.5f * best_gap);

            scene_manager.InvalidateObjects({&obj_index, 1}, CompTransformBit);
            scene_manager.UpdateObjects();

            require(tr.node_index == leaf_index);
            const Phy::dyn_bvh_node_t &fitted = sd.bvh.nodes()[leaf_index];
            require(fitted.bbox_min[0] == old_leaf.bbox_min[0] && fitted.bbox_min[1] == old_leaf.bbox_min[1] &&
                    fitted.bbox_min[2] == old_leaf.bbox_min[2]);
            require(fitted.bbox_max[0] == old_leaf.bbox_max[0] && fitted.bbox_max[1] == old_leaf.bbox_max[1] &&
                    fitted.bbox_max[2] == old_leaf.bbox_max[2]);
            require(sd.objects[obj_index].change_mask == 0);
            require(sd.bvh.insertions_since_build() == 0);
            require(sd.bvh.ValidateBVH());
            require(check_leaf_mapping(sd));
        }
        { // Teleport far outside the tree: remove + re-insert with real branch & bound
            const Phy::dyn_bvh_node_t &root = sd.bvh.nodes()[sd.bvh.root()];
            for (int a = 0; a < 3; a++) {
                tr.world_from_object[3][a] = root.bbox_max[a] + 100.0f;
                tr.position[a] = double(root.bbox_max[a] + 100.0f);
            }

            scene_manager.InvalidateObjects({&obj_index, 1}, CompTransformBit);
            scene_manager.UpdateObjects();

            require(sd.bvh.insertions_since_build() == 1);
            require(sd.bvh.bnb_expansions() > 0);
            require(tr.node_index != 0xffffffff && tr.node_index < uint32_t(sd.bvh.node_capacity()));
            if (tr.node_index < uint32_t(sd.bvh.node_capacity())) {
                const Phy::dyn_bvh_node_t &leaf = sd.bvh.nodes()[tr.node_index];
                bool covers = Phy::bvh_is_leaf(leaf) && Phy::bvh_prim_id(leaf) == obj_index;
                for (int a = 0; a < 3 && covers; a++) {
                    covers = covers && leaf.bbox_min[a] <= tr.bbox_min_ws[a] && leaf.bbox_max[a] >= tr.bbox_max_ws[a];
                }
                require(covers);
            }
            require(sd.bvh.ValidateBVH());
            require(check_leaf_mapping(sd));
        }
    }

    { // E4: world scrolling (SetupView beyond SCROLLING_DISTANCE) translates all bboxes exactly
        SceneData &sd = scene_manager.scene_data();
        auto *transforms = scene_transforms(sd);

        // The node-array diff below compares every slot: requires no free-list holes (E2's
        // remove + re-insert must have reused the freed leaf slot)
        if (!require(sd.bvh.free_count() == 0)) {
            return false;
        }

        const auto nodes_span = sd.bvh.nodes();
        const std::vector<Phy::dyn_bvh_node_t> nodes_before(nodes_span.begin(), nodes_span.end());

        std::vector<uint32_t> node_index_before(sd.objects.size(), Phy::BVH_NO_NODE);
        for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
            if (sd.objects[i].comp_mask & CompTransformBit) {
                node_index_before[i] = transforms[sd.objects[i].components[CompTransform]].node_index;
            }
        }

        const Ren::Vec3d origin_before = sd.origin;
        const Ren::Vec3d far_origin = origin_before + Ren::Vec3d{1234.0, 567.0, -890.0};
        scene_manager.SetupView(far_origin, far_origin + Ren::Vec3d{0.0, -1.0, 0.0}, Ren::Vec3f{0.0f, 1.0f, 0.0f},
                                45.0f, Ren::Vec2f{0.0f}, 1.0f, 0.0f, 0.0f);

        begin_frame();
        scene_manager.Serve(); // Flushes instance-buffer updates queued by world scrolling
        end_frame();

        const Ren::Vec3d origin_delta = sd.origin - origin_before;
        require(std::abs(origin_delta[0]) + std::abs(origin_delta[1]) + std::abs(origin_delta[2]) > 0.5);

        const auto nodes_after = sd.bvh.nodes();
        require(size_t(nodes_after.size()) == nodes_before.size());
        for (size_t n = 0; n < nodes_before.size() && n < size_t(nodes_after.size()); n++) {
            require(Phy::bvh_is_leaf(nodes_before[n]) == Phy::bvh_is_leaf(nodes_after[n]));
            if (Phy::bvh_is_leaf(nodes_after[n])) {
                require(Phy::bvh_prim_id(nodes_after[n]) == Phy::bvh_prim_id(nodes_before[n]));
            }
            for (int c = 0; c < 2; c++) {
                const Phy::Vec3f &b = c ? nodes_before[n].bbox_max : nodes_before[n].bbox_min;
                const Phy::Vec3f &e = c ? nodes_after[n].bbox_max : nodes_after[n].bbox_min;
                for (int a = 0; a < 3; a++) {
                    require(std::abs(e[a] - (b[a] - float(origin_delta[a]))) < 5e-3f);
                }
            }
        }
        for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
            if (!(sd.objects[i].comp_mask & CompTransformBit)) {
                continue;
            }
            require(transforms[sd.objects[i].components[CompTransform]].node_index == node_index_before[i]);
        }
        require(sd.bvh.ValidateBVH());
        require(check_leaf_mapping(sd));
    }

    // Runs after E4: the physics path intentionally leaves Transform::position desynced from the
    // matrix (bodies are never origin-shifted), which the scrolling case above cannot tolerate
    { // E3: CompPhysicsBit invalidation copies the body pose into the transform matrix
        SceneData &sd = scene_manager.scene_data();
        auto *transforms = scene_transforms(sd);

        uint32_t obj_index = 0xffffffff;
        for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
            if (sd.objects[i].comp_mask & CompTransformBit) {
                obj_index = i;
                break;
            }
        }
        if (!require(obj_index != 0xffffffff)) {
            return false;
        }

        // No fixture scene uses physics components, attach one the way the loader does.
        // Create before taking SequentialData: storage may reallocate.
        SceneObject &obj = sd.objects[obj_index];
        const uint32_t phys_comp = sd.comp_store[CompPhysics]->Create();
        obj.components[CompPhysics] = phys_comp;
        obj.comp_mask |= CompPhysicsBit;

        auto *physes = (Physics *)sd.comp_store[CompPhysics]->SequentialData();
        Physics &ph = physes[phys_comp];
        Transform &tr = transforms[obj.components[CompTransform]];

        ph.body.pos = Phy::Vec3{Phy::real(tr.position[0]) + Phy::real(0.25), Phy::real(tr.position[1]) + Phy::real(0.5),
                                Phy::real(tr.position[2])};
        ph.body.rot = Phy::Quat();
        ph.body.vel_lin = ph.body.vel_ang = Phy::Vec3{Phy::real(0), Phy::real(0), Phy::real(0)};
        ph.body.inv_mass = Phy::real(1);
        ph.body.elasticity = ph.body.friction = Phy::real(0);

        scene_manager.InvalidateObjects({&obj_index, 1}, CompPhysicsBit);
        scene_manager.UpdateObjects();

        require(obj.last_change_mask & CompPhysicsBit);
        require(obj.change_mask == 0);
        for (int a = 0; a < 3; a++) {
            require(tr.world_from_object[3][a] == float(ph.body.pos[a]));
        }
        const Phy::Mat3 ph_rot = Phy::ToMat3(ph.body.rot);
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                require(tr.world_from_object[j][i] == float(ph_rot[j][i]) * tr.scale[j]);
            }
        }
        require(sd.bvh.ValidateBVH());
        require(check_leaf_mapping(sd));
    }

    { // E5: churn-triggered rebuild policy (insertions_since_build > node_capacity * frac)
        SceneData &sd = scene_manager.scene_data();
        auto *transforms = scene_transforms(sd);

        require(scene_manager.bvh_rebuild_frac() == 0.5f);

        uint32_t obj_index = 0xffffffff;
        float best_ext = -1.0f;
        for (uint32_t i = 0; i < uint32_t(sd.objects.size()); i++) {
            if (!(sd.objects[i].comp_mask & CompTransformBit)) {
                continue;
            }
            const Transform &tr = transforms[sd.objects[i].components[CompTransform]];
            const float ext = Ren::Length(tr.bbox_max_ws - tr.bbox_min_ws);
            if (ext > best_ext) {
                best_ext = ext;
                obj_index = i;
            }
        }
        if (!require(obj_index != 0xffffffff)) {
            return false;
        }
        Transform &tr = transforms[sd.objects[obj_index].components[CompTransform]];

        const Phy::dyn_bvh_node_t &root = sd.bvh.nodes()[sd.bvh.root()];
        const Ren::Vec3f pos_a{root.bbox_max[0] + 50.0f, root.bbox_max[1] + 50.0f, root.bbox_max[2] + 50.0f};
        const Ren::Vec3f pos_b{root.bbox_min[0] - 50.0f, root.bbox_min[1] - 50.0f, root.bbox_min[2] - 50.0f};

        bool rebuilt = false;
        bool parked_at_b = false;
        int steps_to_rebuild = 0;
        for (int step = 0; step < 64 && !rebuilt; step++) {
            parked_at_b = (step & 1) != 0;
            const Ren::Vec3f &np = parked_at_b ? pos_b : pos_a;
            for (int a = 0; a < 3; a++) {
                tr.world_from_object[3][a] = np[a];
                tr.position[a] = double(np[a]);
            }
            scene_manager.InvalidateObjects({&obj_index, 1}, CompTransformBit);
            scene_manager.UpdateObjects();
            rebuilt = sd.bvh.insertions_since_build() == 0 && sd.bvh.node_capacity() <= nodes_at_load;
            steps_to_rebuild = step + 1;
        }
        require(rebuilt);
        // Churn stays under the node_capacity * frac budget for several frames before paying for
        // a bulk rebuild instead of degrading the tree indefinitely. Measured: 7 churn frames on
        // complex_mat2 (17 nodes, frac 0.5), capacity back to 17 after the rebuild.
        require(steps_to_rebuild > 1);
        // Fresh bulk rebuild from the surviving objects: back to the load-time node array size
        require(sd.bvh.node_capacity() == nodes_at_load);
        require(sd.bvh.free_count() == 0);
        require(sd.bvh.ValidateBVH());
        require(check_leaf_mapping(sd));

        // frac = 0 disables the policy: churn accumulates without a rebuild (move to the far
        // side from wherever the last loop step parked the object, else LeafFits absorbs it)
        scene_manager.set_bvh_rebuild_frac(0.0f);
        const Ren::Vec3f &far_np = parked_at_b ? pos_a : pos_b;
        for (int a = 0; a < 3; a++) {
            tr.world_from_object[3][a] = far_np[a];
            tr.position[a] = double(far_np[a]);
        }
        scene_manager.InvalidateObjects({&obj_index, 1}, CompTransformBit);
        scene_manager.UpdateObjects();
        require(sd.bvh.insertions_since_build() == 1);
        // Remove + re-insert reuses the freed leaf/parent slots, so only the churn counter grows
        require(sd.bvh.node_capacity() == nodes_at_load);
        require(sd.bvh.ValidateBVH());
        require(check_leaf_mapping(sd));
    }

    return true;
}
} // namespace

void test_bvh(Sys::ThreadPool &threads) {
    using namespace std::chrono;
    const auto start_time = high_resolution_clock::now();

    const bool ok = run_bvh_glue_tests(threads);

    const double elapsed_ms = duration<double>(high_resolution_clock::now() - start_time).count() * 1000.0;
    printf("Test %-36s (%s, Time: %.2fms)\n", "bvh", ok ? "OK" : "FAIL", elapsed_ms);
    if (!ok) {
        g_tests_success = false;
    }
}
