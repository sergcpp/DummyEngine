#include "RpDebugTextures.h"
#if 0
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

namespace PrimDrawInternal {
extern const uint16_t fs_quad_indices[];
} // namespace PrimDrawInternal

void RpDebugTextures::DrawShadowMaps(Ren::Context &ctx, RpAllocTex &shadowmap_tex) {
    using namespace PrimDrawInternal;

#ifndef NDEBUG
    Ren::ResetGLState();
#endif

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    glBindVertexArray(temp_vtx_input_.gl_vao());
    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf_ndx_offset()), 6 * sizeof(uint16_t),
                    PrimDrawInternal::fs_quad_indices);

    const auto &p = shadowmap_tex.ref->params;
    const Ren::eTexCompare comp_before = p.sampling.compare;

    Ren::SamplingParams tmp_params = p.sampling;
    tmp_params.compare = Ren::eTexCompare::None;

    shadowmap_tex.ref->ApplySampling(tmp_params, ctx.log());

    const float k = (float(p.h) / float(p.w)) * (float(view_state_->scr_res[0]) / float(view_state_->scr_res[1]));

    glBindFramebuffer(GL_FRAMEBUFFER, output_fb_.id());

    { // Clear region
        glEnable(GL_SCISSOR_TEST);

        glScissor(0, 0, view_state_->scr_res[0] / 2, int(k * float(view_state_->scr_res[1]) / 2));
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_SCISSOR_TEST);
    }

    glUseProgram(blit_depth_prog_->id());
    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, shadowmap_tex.ref->id());

    // Draw visible shadow regions
    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        const ShadowMapRegion &sh_reg = shadow_regions_.data[i];

        const float positions[] = {-1.0f + sh_reg.transform[0],
                                   -1.0f + sh_reg.transform[1] * k,
                                   -1.0f + sh_reg.transform[0] + sh_reg.transform[2],
                                   -1.0f + sh_reg.transform[1] * k,
                                   -1.0f + sh_reg.transform[0] + sh_reg.transform[2],
                                   -1.0f + (sh_reg.transform[1] + sh_reg.transform[3]) * k,
                                   -1.0f + sh_reg.transform[0],
                                   -1.0f + (sh_reg.transform[1] + sh_reg.transform[3]) * k};

        const float uvs[] = {float(sh_list.shadow_map_pos[0]),
                             float(sh_list.shadow_map_pos[1]),
                             float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                             float(sh_list.shadow_map_pos[1]),
                             float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                             float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1]),
                             float(sh_list.shadow_map_pos[0]),
                             float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1])};

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf1_vtx_offset()), sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf1_vtx_offset() + sizeof(positions)), sizeof(uvs),
                        uvs);

        glUniform1f(1, sh_list.cam_near);
        glUniform1f(2, sh_list.cam_far);

        if (sh_list.shadow_batch_count) {
            // mark updated region with red
            glUniform3f(3, 1.0f, 0.5f, 0.5f);
        } else {
            // mark cached region with green
            glUniform3f(3, 0.5f, 1.0f, 0.5f);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(prim_draw_.temp_buf_ndx_offset()));
    }

    // Draw invisible cached shadow regions
    for (int i = 0; i < int(cached_shadow_regions_.count); i++) {
        const ShadReg &sh_reg = cached_shadow_regions_.data[i];

        const float positions[] = {-1.0f + float(sh_reg.pos[0]) / float(p.w),
                                   -1.0f + k * float(sh_reg.pos[1]) / float(p.h),
                                   -1.0f + float(sh_reg.pos[0] + sh_reg.size[0]) / float(p.w),
                                   -1.0f + k * float(sh_reg.pos[1]) / float(p.h),
                                   -1.0f + float(sh_reg.pos[0] + sh_reg.size[0]) / float(p.w),
                                   -1.0f + k * float(sh_reg.pos[1] + sh_reg.size[1]) / float(p.h),
                                   -1.0f + float(sh_reg.pos[0]) / float(p.w),
                                   -1.0f + k * float(sh_reg.pos[1] + sh_reg.size[1]) / float(p.h)};

        const float uvs[] = {float(sh_reg.pos[0]),
                             float(sh_reg.pos[1]),
                             float(sh_reg.pos[0] + sh_reg.size[0]),
                             float(sh_reg.pos[1]),
                             float(sh_reg.pos[0] + sh_reg.size[0]),
                             float(sh_reg.pos[1] + sh_reg.size[1]),
                             float(sh_reg.pos[0]),
                             float(sh_reg.pos[1] + sh_reg.size[1])};

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf1_vtx_offset()), sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf1_vtx_offset() + sizeof(positions)), sizeof(uvs),
                        uvs);

        glUniform1f(1, sh_reg.cam_near);
        glUniform1f(2, sh_reg.cam_far);

        // mark cached region with blue
        glUniform3f(3, 0.5f, 0.5f, 1.0f);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(prim_draw_.temp_buf_ndx_offset()));
    }

    // Draw view frustum edges
    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        const ShadowMapRegion &sh_reg = shadow_regions_.data[i];

        if (!sh_list.view_frustum_outline_count) {
            continue;
        }

        for (int j = 0; j < sh_list.view_frustum_outline_count; j += 2) {
            const Ren::Vec2f &p1 = sh_list.view_frustum_outline[j], &p2 = sh_list.view_frustum_outline[j + 1];

            const float positions[] = {
                -1.0f + sh_reg.transform[0] + (p1[0] * 0.5f + 0.5f) * sh_reg.transform[2],
                -1.0f + (sh_reg.transform[1] + (p1[1] * 0.5f + 0.5f) * sh_reg.transform[3]) * k,
                -1.0f + sh_reg.transform[0] + (p2[0] * 0.5f + 0.5f) * sh_reg.transform[2],
                -1.0f + (sh_reg.transform[1] + (p2[1] * 0.5f + 0.5f) * sh_reg.transform[3]) * k,
            };

            glBufferSubData(GL_ARRAY_BUFFER, GLintptr(prim_draw_.temp_buf1_vtx_offset()), sizeof(positions), positions);

            // draw line with black color
            glUniform3f(3, 0.0f, 0.0f, 0.0f);

            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(prim_draw_.temp_buf_ndx_offset()));
        }
    }

    glBindVertexArray(0);

    // Restore compare mode
    tmp_params.compare = comp_before;

    shadowmap_tex.ref->ApplySampling(tmp_params, ctx.log());
}
#endif