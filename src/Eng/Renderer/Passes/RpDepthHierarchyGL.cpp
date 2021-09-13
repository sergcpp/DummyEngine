#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/depth_hierarchy_interface.glsl"

void RpDepthHierarchy::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh());

    glUseProgram(pi_depth_hierarchy_.prog()->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, DepthHierarchy::DEPTH_TEX_SLOT, input_tex.ref->id());
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 0, output_tex.ref->id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 1, output_tex.ref->id(), 1, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 2, output_tex.ref->id(), 2, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 3, output_tex.ref->id(), 3, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 4, output_tex.ref->id(), 4, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + 5, output_tex.ref->id(), 5, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    Ren::Buffer temp_unif_buffer =
        Ren::Buffer("Temp uniform buf", nullptr, Ren::eBufType::Uniform, sizeof(DepthHierarchy::Params), 16);
    Ren::Buffer temp_stage_buffer =
        Ren::Buffer("Temp stage buf", nullptr, Ren::eBufType::Stage, sizeof(DepthHierarchy::Params), 16);
    {
        DepthHierarchy::Params *stage_data =
            reinterpret_cast<DepthHierarchy::Params *>(temp_stage_buffer.Map(Ren::BufMapWrite));
        stage_data->depth_size = Ren::Vec4i{view_state_->scr_res[0], view_state_->scr_res[1], 0, 0};
        stage_data->clip_info = view_state_->clip_info;

        temp_stage_buffer.FlushMappedRange(0, sizeof(DepthHierarchy::Params));
        temp_stage_buffer.Unmap();
    }
    Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, sizeof(DepthHierarchy::Params), nullptr);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_UNIF_PARAM_LOC, temp_unif_buffer.id());

    glDispatchCompute(
        (view_state_->scr_res[0] + DepthHierarchy::LOCAL_GROUP_SIZE_X - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_X,
        (view_state_->scr_res[1] + DepthHierarchy::LOCAL_GROUP_SIZE_Y - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_Y, 1);
}
