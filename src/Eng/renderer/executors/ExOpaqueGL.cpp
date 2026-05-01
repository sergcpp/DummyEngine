#include "ExOpaque.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/Gl/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
void _bind_textures_and_samplers(const Ren::StoragesRef &storages, const Ren::MaterialMain &mat) {
    assert(mat.textures.size() == mat.samplers.size());
    for (int j = 0; j < int(mat.textures.size()); ++j) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX0 + j,
                                   storages.images[mat.textures[j]].first.img);
        glBindSampler(Eng::BIND_MAT_TEX0 + j, storages.samplers[mat.samplers[j]].id);
    }
}
uint32_t _draw_list_range_full(const Eng::FgContext &fg, Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               Eng::backend_info_t &backend_info) {
    auto &ren_ctx = fg.ren_ctx();

    const Ren::StoragesRef &storages = fg.storages();
    const Ren::PipelineMain *pipelines = storages.pipelines.data_main();
    const Ren::ProgramMain *programs = storages.programs.data_main();

    GLenum cur_primitive;
    if (cur_prog_id != 0xffffffffffffffff) {
        const Ren::ProgramMain &p = programs[cur_prog_id];
        if (p.has_tessellation()) {
            cur_primitive = GL_PATCHES;
        } else {
            cur_primitive = GL_TRIANGLES;
        }
    }

    for (; i < main_batch_indices.size(); i++) {
        const auto &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            const uint32_t prog_id = pipelines[batch.pipe_id].prog.index;
            if (cur_prog_id != prog_id) {
                const Ren::ProgramMain &p = programs[prog_id];
                glUseProgram(p.id);

                if (p.has_tessellation()) {
                    cur_primitive = GL_PATCHES;
                } else {
                    cur_primitive = GL_TRIANGLES;
                }
                cur_prog_id = prog_id;
            }
        }

        if (!ren_ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::MaterialMain &mat = storages.materials.GetUnsafe(batch.mat_id).first;
            _bind_textures_and_samplers(storages, mat);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(cur_primitive, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(const Eng::FgContext &fg, Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   Eng::backend_info_t &backend_info) {
    auto &ren_ctx = fg.ren_ctx();

    const Ren::StoragesRef &storages = fg.storages();
    const Ren::PipelineMain *pipelines = storages.pipelines.data_main();
    const Ren::ProgramMain *programs = storages.programs.data_main();

    int i = int(ndx);
    for (; i >= 0; i--) {
        const auto &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            const uint32_t prog_id = pipelines[batch.pipe_id].prog.index;
            if (cur_prog_id != prog_id) {
                const Ren::ProgramMain &p = programs[prog_id];
                glUseProgram(p.id);

                cur_prog_id = prog_id;
            }
        }

        if (!ren_ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::MaterialMain &mat = storages.materials.GetUnsafe(batch.mat_id).first;
            _bind_textures_and_samplers(storages, mat);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return uint32_t(i);
}
} // namespace ExSharedInternal

void Eng::ExOpaque::DrawOpaque(const FgContext &fg, const Ren::ImageRWHandle color_tex,
                               const Ren::ImageRWHandle normal_tex, const Ren::ImageRWHandle spec_tex,
                               const Ren::ImageRWHandle depth_tex) {
    using namespace ExSharedInternal;

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    if ((*p_list_)->render_settings.debug_wireframe) {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    rast_state.depth.test_enabled = true;
    if (!(*p_list_)->render_settings.debug_wireframe) {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);
    } else {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::LEqual);
    }

    const Ren::ImageRWHandle color_targets[] = {color_tex, normal_tex, spec_tex};
    const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer(rp_opaque_, depth_tex, depth_tex, color_targets);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[fb].first.id);

    rast_state.viewport[2] = view_state_->ren_res[0];
    rast_state.viewport[3] = view_state_->ren_res[1];

    rast_state.ApplyChanged(fg.rast_state());
    fg.rast_state() = rast_state;

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //
    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(args_->decals);

    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle ssao = fg.AccessROImage(args_->ssao);
    const Ren::ImageROHandle brdf_lut = fg.AccessROImage(args_->brdf_lut);
    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    const Ren::ImageROHandle cone_rt_lut = fg.AccessROImage(args_->cone_rt_lut);

    const Ren::ImageROHandle dummy_black = fg.AccessROImage(args_->dummy_black);

    Ren::ImageROHandle lm_tex[4];
    for (int i = 0; i < 4; ++i) {
        // if (args_->lm_tex[i]) {
        // lm_tex[i] = &fg.AccessROImage(args_->lm_tex[i]);
        //} else {
        lm_tex[i] = dummy_black;
        //}
    }

    const Ren::VertexInput &vi = storages.vtx_inputs[draw_pass_vi_];
    VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);

    const Ren::BufferMain &materials_main = storages.buffers[materials].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_main.buf));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        const Ren::BufferMain &buf_main = storages.buffers[bindless_tex_->rt_inline_textures.buf].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(buf_main.buf));
    }

    const Ren::BufferMain &unif_shared_data_main = storages.buffers[unif_shared_data].first;
    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_main.buf);

    const Ren::ImageMain &shadow_depth_main = storages.images[shadow_depth].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SHAD_TEX, shadow_depth_main.img);

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    const Ren::ImageMain &ssao_main = storages.images[ssao].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SSAO_TEX_SLOT, ssao_main.img);

    const Ren::ImageMain &brdf_lut_main = storages.images[brdf_lut].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BRDF_LUT, brdf_lut_main.img);

    for (int sh_l = 0; sh_l < 4; sh_l++) {
        const Ren::ImageMain &lm_tex_main = storages.images[lm_tex[sh_l]].first;
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, lm_tex_main.img);
    }

    // ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, BIND_ENV_TEX,
    //                            (*p_list_)->probe_storage ? (*p_list_)->probe_storage->handle().id : 0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_LIGHT_BUF,
                               GLuint(storages.buffers[lights].first.views[0].second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_DECAL_BUF,
                               GLuint(storages.buffers[decals].first.views[0].second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_CELLS_BUF,
                               GLuint(storages.buffers[cells].first.views[0].second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_ITEMS_BUF,
                               GLuint(storages.buffers[items].first.views[0].second));

    const Ren::ImageMain &noise_main = storages.images[noise].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_main.img);
    // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_CONE_RT_LUT, cone_rt_lut.id());

    const Ren::BufferMain &instances_main = storages.buffers[instances].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_main.views[0].second));
    const Ren::BufferMain &instance_indices_main = storages.buffers[instance_indices].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_main.buf));

    const Ren::Span<const custom_draw_batch_t> batches = {(*p_list_)->custom_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->custom_batch_indices};

    static backend_info_t _dummy = {};

    { // actual drawing
        using CDB = custom_draw_batch_t;

        uint64_t cur_pipe_id = 0xffffffffffffffff;
        uint64_t cur_prog_id = 0xffffffffffffffff;
        uint64_t cur_mat_id = 0xffffffffffffffff;

        uint32_t i = 0;

        { // one-sided1
            Ren::DebugMarker _m(api, fg.cmd_buf(), "ONE-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_list_range_full(fg, batches, batch_indices, i, 0ull, cur_mat_id, cur_pipe_id, cur_prog_id,
                                      _dummy);
        }

        { // two-sided1
            Ren::DebugMarker _m(api, fg.cmd_buf(), "TWO-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_list_range_full(fg, batches, batch_indices, i, CDB::BitTwoSided, cur_mat_id, cur_pipe_id,
                                      cur_prog_id, _dummy);
        }

        { // one-sided2
            Ren::DebugMarker _m(api, fg.cmd_buf(), "ONE-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_list_range_full(fg, batches, batch_indices, i, CDB::BitAlphaTest, cur_mat_id, cur_pipe_id,
                                      cur_prog_id, _dummy);
        }

        { // two-sided2
            Ren::DebugMarker _m(api, fg.cmd_buf(), "TWO-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_list_range_full(fg, batches, batch_indices, i, CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id,
                                      cur_pipe_id, cur_prog_id, _dummy);
        }

        { // two-sided-tested-blended
            Ren::DebugMarker _m(api, fg.cmd_buf(), "TWO-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_list_range_full_rev(fg, batches, batch_indices, uint32_t(batch_indices.size() - 1),
                                          CDB::BitAlphaBlend | CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id,
                                          cur_pipe_id, cur_prog_id, _dummy);
        }

        { // one-sided-tested-blended
            Ren::DebugMarker _m(api, fg.cmd_buf(), "ONE-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            _draw_list_range_full_rev(fg, batches, batch_indices, i, CDB::BitAlphaBlend | CDB::BitAlphaTest, cur_mat_id,
                                      cur_pipe_id, cur_prog_id, _dummy);
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 8);
}

Eng::ExOpaque::~ExOpaque() = default;