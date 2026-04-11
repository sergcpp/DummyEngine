#include "ExDepthFill.h"

#include <Ren/Context.h>
#include <Ren/utils/Span.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                     uint32_t i, const uint64_t mask) {
    for (; i < batch_indices.size(); ++i) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
            break;
        }
    }
    return i;
}
} // namespace ExSharedInternal

void Eng::ExDepthFill::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle depth = fg.AccessRWImage(depth_);
    const Ren::ImageRWHandle velocity = fg.AccessRWImage(velocity_);

    LazyInit(fg.ren_ctx(), fg.sh(), depth, velocity);
    DrawDepth(fg, depth, velocity);
}

void Eng::ExDepthFill::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::ImageRWHandle depth,
                                const Ren::ImageRWHandle velocity) {
    const Ren::RenderTarget velocity_target = {velocity, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
    const Ren::RenderTarget depth_clear_target = {depth, Ren::eLoadOp::Clear, Ren::eStoreOp::Store, Ren::eLoadOp::Clear,
                                                  Ren::eStoreOp::Store};
    const Ren::RenderTarget depth_load_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                 Ren::eStoreOp::Store};

    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif
        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_solid, vi_vege_solid, vi_transp, vi_vege_transp, vi_skin_solid, vi_skin_transp;
        { // VertexInput for solid depth-fill pass (uses position attribute only)
            const Ren::VtxAttribDesc attribs[] = {{0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0}};
            vi_solid = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for solid depth-fill pass of vegetation (uses position and color attributes only)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_vege_solid = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for alpha-tested depth-fill pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)}};
            vi_transp = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for alpha-tested depth-fill pass of vegetation (uses position, uvs and color attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_vege_transp = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for depth-fill pass of skinned solid meshes (with velocity output)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_PRE_LOC, 3, Ren::eType::Float32, buf1_stride, MAX_SKIN_VERTICES_TOTAL * 16, 0}};
            vi_skin_solid = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for depth-fill pass of skinned transparent meshes (with velocity output)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {0, VTX_PRE_LOC, 3, Ren::eType::Float32, buf1_stride, MAX_SKIN_VERTICES_TOTAL * 16, 0}};
            vi_skin_transp = sh.FindOrCreateVertexInput(attribs);
        }

        const Ren::ProgramHandle fillz_solid_prog =
            sh.FindOrCreateProgram("internal/fillz.vert.glsl", "internal/fillz.frag.glsl");
        const Ren::ProgramHandle fillz_solid_mov_prog =
            sh.FindOrCreateProgram("internal/fillz@MOVING.vert.glsl", "internal/fillz@OUTPUT_VELOCITY.frag.glsl");
        const Ren::ProgramHandle fillz_vege_solid_vel_prog =
            sh.FindOrCreateProgram(bindless ? "internal/fillz_vege@OUTPUT_VELOCITY.vert.glsl"
                                            : "internal/fillz_vege@OUTPUT_VELOCITY;NO_BINDLESS.vert.glsl",
                                   "internal/fillz@OUTPUT_VELOCITY.frag.glsl");
        const Ren::ProgramHandle fillz_vege_solid_vel_mov_prog =
            sh.FindOrCreateProgram(bindless ? "internal/fillz_vege@MOVING;OUTPUT_VELOCITY.vert.glsl"
                                            : "internal/fillz_vege@MOVING;OUTPUT_VELOCITY;NO_BINDLESS.vert.glsl",
                                   "internal/fillz@OUTPUT_VELOCITY.frag.glsl");
        const Ren::ProgramHandle fillz_transp_prog = sh.FindOrCreateProgram(
            bindless ? "internal/fillz@ALPHATEST.vert.glsl" : "internal/fillz@ALPHATEST;NO_BINDLESS.vert.glsl",
            bindless ? "internal/fillz@ALPHATEST.frag.glsl" : "internal/fillz@ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_transp_mov_prog =
            sh.FindOrCreateProgram(bindless ? "internal/fillz@MOVING;ALPHATEST.vert.glsl"
                                            : "internal/fillz@MOVING;ALPHATEST;NO_BINDLESS.vert.glsl",
                                   bindless ? "internal/fillz@OUTPUT_VELOCITY;ALPHATEST.frag.glsl"
                                            : "internal/fillz@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_vege_transp_vel_prog =
            sh.FindOrCreateProgram(bindless ? "internal/fillz_vege@OUTPUT_VELOCITY;ALPHATEST.vert.glsl"
                                            : "internal/fillz_vege@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.vert.glsl",
                                   bindless ? "internal/fillz@OUTPUT_VELOCITY;ALPHATEST.frag.glsl"
                                            : "internal/fillz@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_vege_transp_vel_mov_prog = sh.FindOrCreateProgram(
            bindless ? "internal/fillz_vege@MOVING;OUTPUT_VELOCITY;ALPHATEST.vert.glsl"
                     : "internal/fillz_vege@MOVING;OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.vert.glsl",
            bindless ? "internal/fillz@OUTPUT_VELOCITY;ALPHATEST.frag.glsl"
                     : "internal/fillz@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_skin_solid_vel_prog = sh.FindOrCreateProgram(
            "internal/fillz_skin@OUTPUT_VELOCITY.vert.glsl", "internal/fillz@OUTPUT_VELOCITY.frag.glsl");
        const Ren::ProgramHandle fillz_skin_solid_vel_mov_prog = sh.FindOrCreateProgram(
            "internal/fillz_skin@MOVING;OUTPUT_VELOCITY.vert.glsl", "internal/fillz@OUTPUT_VELOCITY.frag.glsl");
        // const Ren::ProgramHandle fillz_skin_transp_prog = sh.FindOrCreateProgram(
        //     bindless ? "internal/fillz_skin@ALPHATEST.vert.glsl"
        //              : "internal/fillz_skin@ALPHATEST;NO_BINDLESS.vert.glsl",
        //     bindless ? "internal/fillz@ALPHATEST.frag.glsl" : "internal/fillz@ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_skin_transp_vel_prog =
            sh.FindOrCreateProgram(bindless ? "internal/fillz_skin@OUTPUT_VELOCITY;ALPHATEST.vert.glsl"
                                            : "internal/fillz_skin@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.vert.glsl",
                                   bindless ? "internal/fillz@OUTPUT_VELOCITY;ALPHATEST.frag.glsl"
                                            : "internal/fillz@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle fillz_skin_transp_vel_mov_prog = sh.FindOrCreateProgram(
            bindless ? "internal/fillz_skin@MOVING;OUTPUT_VELOCITY;ALPHATEST.vert.glsl"
                     : "internal/fillz_skin@MOVING;OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.vert.glsl",
            bindless ? "internal/fillz@OUTPUT_VELOCITY;ALPHATEST.frag.glsl"
                     : "internal/fillz@OUTPUT_VELOCITY;ALPHATEST;NO_BINDLESS.frag.glsl");

        rp_depth_only_[0] = sh.FindOrCreateRenderPass(depth_clear_target, {});
        rp_depth_only_[1] = sh.FindOrCreateRenderPass(depth_load_target, {});

        rp_depth_velocity_[0] = sh.FindOrCreateRenderPass(depth_clear_target, {&velocity_target, 1});
        rp_depth_velocity_[1] = sh.FindOrCreateRenderPass(depth_load_target, {&velocity_target, 1});

        { // static solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = uint8_t(Ren::eStencilOp::Replace);

            pi_static_solid_[2] = sh.FindOrCreatePipeline(rast_state, fillz_solid_prog, vi_solid, rp_depth_only_[0], 0);
            pi_static_transp_[2] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_prog, vi_transp, rp_depth_only_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_static_solid_[0] = sh.FindOrCreatePipeline(rast_state, fillz_solid_prog, vi_solid, rp_depth_only_[0], 0);
            pi_static_transp_[0] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_prog, vi_transp, rp_depth_only_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_static_solid_[1] = sh.FindOrCreatePipeline(rast_state, fillz_solid_prog, vi_solid, rp_depth_only_[0], 0);
            pi_static_transp_[1] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_prog, vi_transp, rp_depth_only_[0], 0);
        }
        { // moving solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);
            rast_state.stencil.reference = STENCIL_DYNAMIC_BIT;

            pi_moving_solid_[2] =
                sh.FindOrCreatePipeline(rast_state, fillz_solid_mov_prog, vi_solid, rp_depth_velocity_[0], 0);
            pi_moving_transp_[2] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_mov_prog, vi_transp, rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_moving_solid_[0] =
                sh.FindOrCreatePipeline(rast_state, fillz_solid_mov_prog, vi_solid, rp_depth_velocity_[0], 0);
            pi_moving_transp_[0] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_mov_prog, vi_transp, rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_moving_solid_[1] =
                sh.FindOrCreatePipeline(rast_state, fillz_solid_mov_prog, vi_solid, rp_depth_velocity_[0], 0);
            pi_moving_transp_[1] =
                sh.FindOrCreatePipeline(rast_state, fillz_transp_mov_prog, vi_transp, rp_depth_velocity_[0], 0);
        }
        { // vege solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);
            rast_state.stencil.reference = STENCIL_DYNAMIC_BIT;

            pi_vege_static_solid_[1] =
                sh.FindOrCreatePipeline(rast_state, fillz_vege_solid_vel_prog, vi_vege_solid, rp_depth_velocity_[0], 0);
            pi_vege_static_transp_[1] = sh.FindOrCreatePipeline(rast_state, fillz_vege_transp_vel_prog, vi_vege_transp,
                                                                rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vege_static_solid_[0] =
                sh.FindOrCreatePipeline(rast_state, fillz_vege_solid_vel_prog, vi_vege_solid, rp_depth_velocity_[0], 0);
            pi_vege_static_transp_[0] = sh.FindOrCreatePipeline(rast_state, fillz_vege_transp_vel_prog, vi_vege_transp,
                                                                rp_depth_velocity_[0], 0);
        }
        { // vege moving solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);

            // mark dynamic objects
            rast_state.stencil.reference = STENCIL_DYNAMIC_BIT;

            pi_vege_moving_solid_[1] = sh.FindOrCreatePipeline(rast_state, fillz_vege_solid_vel_mov_prog, vi_vege_solid,
                                                               rp_depth_velocity_[0], 0);
            pi_vege_moving_transp_[1] = sh.FindOrCreatePipeline(rast_state, fillz_vege_transp_vel_mov_prog,
                                                                vi_vege_transp, rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vege_moving_solid_[0] = sh.FindOrCreatePipeline(rast_state, fillz_vege_solid_vel_mov_prog, vi_vege_solid,
                                                               rp_depth_velocity_[0], 0);
            pi_vege_moving_transp_[0] = sh.FindOrCreatePipeline(rast_state, fillz_vege_transp_vel_mov_prog,
                                                                vi_vege_transp, rp_depth_velocity_[0], 0);
        }
        { // skin solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);
            rast_state.stencil.reference = STENCIL_DYNAMIC_BIT;

            pi_skin_static_solid_[1] =
                sh.FindOrCreatePipeline(rast_state, fillz_skin_solid_vel_prog, vi_skin_solid, rp_depth_velocity_[0], 0);
            pi_skin_static_transp_[1] = sh.FindOrCreatePipeline(rast_state, fillz_skin_transp_vel_prog, vi_skin_transp,
                                                                rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_skin_static_solid_[0] =
                sh.FindOrCreatePipeline(rast_state, fillz_skin_solid_vel_prog, vi_skin_solid, rp_depth_velocity_[0], 0);
            pi_skin_static_transp_[0] = sh.FindOrCreatePipeline(rast_state, fillz_skin_transp_vel_prog, vi_skin_transp,
                                                                rp_depth_velocity_[0], 0);
        }
        { // skin moving solid/transp
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);
            rast_state.stencil.reference = STENCIL_DYNAMIC_BIT;

            pi_skin_moving_solid_[1] = sh.FindOrCreatePipeline(rast_state, fillz_skin_solid_vel_mov_prog, vi_skin_solid,
                                                               rp_depth_velocity_[0], 0);
            pi_skin_moving_transp_[1] = sh.FindOrCreatePipeline(rast_state, fillz_skin_transp_vel_mov_prog,
                                                                vi_skin_transp, rp_depth_velocity_[0], 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_skin_moving_solid_[0] = sh.FindOrCreatePipeline(rast_state, fillz_skin_solid_vel_mov_prog, vi_skin_solid,
                                                               rp_depth_velocity_[0], 0);
            pi_skin_moving_transp_[0] = sh.FindOrCreatePipeline(rast_state, fillz_skin_transp_vel_mov_prog,
                                                                vi_skin_transp, rp_depth_velocity_[0], 0);
        }

        initialized = true;
    }
}