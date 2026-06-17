#include "ExDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Gl/GL.h>
#include <Ren/Program.h>
#include <Sys/ScopeExit.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/depth_hierarchy_interface.h"

void Eng::ExDepthHierarchy::Execute(const FgContext &fg) {
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::ImageROHandle depth = fg.AccessROImage(depth_);
    const Ren::ImageRWHandle output = fg.AccessRWImage(output_);

    const auto &[depth_main, depth_cold] = storages.images[depth];
    const auto &[output_main, output_cold] = storages.images[output];

    const Ren::PipelineMain &pi = storages.pipelines[pi_depth_hierarchy_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    glUseProgram(pr.id);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, DepthHierarchy::DEPTH_TEX_SLOT, depth_main.img);

    int i = 0;
    for (; i < output_cold.params.mip_count; ++i) {
        glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + i, output_main.img, i, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
    }
    for (; i < 7; ++i) {
        glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + i, 0, i, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
    }

    const int grp_x = (output_cold.params.w + DepthHierarchy::GRP_SIZE_X - 1) / DepthHierarchy::GRP_SIZE_X;
    const int grp_y = (output_cold.params.h + DepthHierarchy::GRP_SIZE_Y - 1) / DepthHierarchy::GRP_SIZE_Y;

    const Ren::ApiContext &api = fg.ren_ctx().api();

    Ren::BufferMain temp_unif_buffer_main = {};
    Ren::BufferCold temp_unif_buffer_cold = {};
    if (!Buffer_Init(api, temp_unif_buffer_main, temp_unif_buffer_cold, Ren::String{"Temp uniform buf"},
                     Ren::eBufType::Uniform, sizeof(DepthHierarchy::Params), fg.log())) {
        fg.log()->Error("Failed to initialize temp uniform buffer");
        return;
    }
    SCOPE_EXIT({ Buffer_Destroy(api, temp_unif_buffer_main, temp_unif_buffer_cold); })

    Ren::BufferMain temp_stage_buffer_main = {};
    Ren::BufferCold temp_stage_buffer_cold = {};
    if (!Buffer_Init(api, temp_stage_buffer_main, temp_stage_buffer_cold, Ren::String{"Temp upload buf"},
                     Ren::eBufType::Upload, sizeof(DepthHierarchy::Params), fg.log())) {
        fg.log()->Error("Failed to initialize temp upload buffer");
        return;
    }
    SCOPE_EXIT({ Buffer_Destroy(api, temp_stage_buffer_main, temp_stage_buffer_cold); })
    
    { // Update parameters
        DepthHierarchy::Params *stage_data =
            reinterpret_cast<DepthHierarchy::Params *>(Buffer_Map(api, temp_stage_buffer_main, temp_stage_buffer_cold));
        stage_data->depth_size =
            Ren::Vec4u{view_state_->ren_res[0], view_state_->ren_res[1], output_cold.params.mip_count, grp_x * grp_y};
        stage_data->clip_info = view_state_->clip_info;

        Buffer_Unmap(api, temp_stage_buffer_main, temp_stage_buffer_cold);
    }
    CopyBufferToBuffer(api, temp_stage_buffer_main, 0, temp_unif_buffer_main, 0, sizeof(DepthHierarchy::Params),
                       nullptr);

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_PUSH_CONSTANT_BUF, temp_unif_buffer_main.buf);

    glDispatchCompute(grp_x, grp_y, 1);
}
