#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/GL.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../Shaders/depth_hierarchy_interface.h"

void RpDepthHierarchy::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh());

    glUseProgram(pi_depth_hierarchy_.prog()->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, DepthHierarchy::DEPTH_TEX_SLOT, depth_tex.ref->id());

    int i = 0;
    for (; i < output_tex.ref->params.mip_count; ++i) {
        glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + i, output_tex.ref->id(), i, GL_FALSE, 0, GL_WRITE_ONLY,
                           GL_R32F);
    }
    for (; i < 7; ++i) {
        glBindImageTexture(DepthHierarchy::DEPTH_IMG_SLOT + i, 0, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    }

    const int grp_x =
        (output_tex.ref->params.w + DepthHierarchy::LOCAL_GROUP_SIZE_X - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_X;
    const int grp_y =
        (output_tex.ref->params.h + DepthHierarchy::LOCAL_GROUP_SIZE_Y - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_Y;

    Ren::Buffer temp_unif_buffer =
        Ren::Buffer("Temp uniform buf", nullptr, Ren::eBufType::Uniform, sizeof(DepthHierarchy::Params), 16);
    Ren::Buffer temp_stage_buffer =
        Ren::Buffer("Temp stage buf", nullptr, Ren::eBufType::Stage, sizeof(DepthHierarchy::Params), 16);
    {
        DepthHierarchy::Params *stage_data =
            reinterpret_cast<DepthHierarchy::Params *>(temp_stage_buffer.Map(Ren::BufMapWrite));
        stage_data->depth_size = Ren::Vec4i{view_state_->scr_res[0], view_state_->scr_res[1],
                                            output_tex.ref->params.mip_count, grp_x * grp_y};
        stage_data->clip_info = view_state_->clip_info;

        temp_stage_buffer.FlushMappedRange(0, sizeof(DepthHierarchy::Params));
        temp_stage_buffer.Unmap();
    }
    Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, sizeof(DepthHierarchy::Params), nullptr);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_UNIF_PARAM_LOC, temp_unif_buffer.id());

    glDispatchCompute(grp_x, grp_y, 1);
}
