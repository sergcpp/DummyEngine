#include "RpSSRTraceHQ.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_trace_hq_interface.glsl"

void RpSSRTraceHQ::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh());

    glUseProgram(pi_ssr_trace_hq_.prog()->id());

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, GLuint(unif_sh_data_buf.ref->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, SSRTraceHQ::DEPTH_TEX_SLOT, depth_down_2x_tex.ref->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, SSRTraceHQ::NORM_TEX_SLOT, normal_tex.ref->id());
    glBindImageTexture(SSRTraceHQ::OUTPUT_TEX_SLOT, output_tex.ref->id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGB10_A2);

    Ren::Buffer temp_unif_buffer =
        Ren::Buffer("Temp uniform buf", nullptr, Ren::eBufType::Uniform, sizeof(SSRTraceHQ::Params), 16);
    Ren::Buffer temp_stage_buffer =
        Ren::Buffer("Temp stage buf", nullptr, Ren::eBufType::Stage, sizeof(SSRTraceHQ::Params), 16);
    {
        auto *stage_data = reinterpret_cast<SSRTraceHQ::Params *>(temp_stage_buffer.Map(Ren::BufMapWrite));
        stage_data->resolution = Ren::Vec4u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1]), 0, 0};

        temp_stage_buffer.FlushMappedRange(0, sizeof(SSRTraceHQ::Params));
        temp_stage_buffer.Unmap();
    }
    Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, sizeof(SSRTraceHQ::Params), nullptr);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_UNIF_PARAM_LOC, temp_unif_buffer.id());

    glDispatchCompute((view_state_->act_res[0] + SSRTraceHQ::LOCAL_GROUP_SIZE_X - 1) / SSRTraceHQ::LOCAL_GROUP_SIZE_X,
                      (view_state_->act_res[1] + SSRTraceHQ::LOCAL_GROUP_SIZE_Y - 1) / SSRTraceHQ::LOCAL_GROUP_SIZE_Y,
                      1);
}
