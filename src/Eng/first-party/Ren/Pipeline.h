#pragma once

#include "Buffer.h"
#include "Fwd.h"
#include "ImageParams.h"
#include "Program.h"
#include "RastState.h"
#include "RenderPass.h"
#include "VertexInput.h"
#include "utils/Span.h"

namespace Ren {
struct ApiContext;

enum class ePipelineType : uint8_t { Undefined, Graphics, Compute, Raytracing };

struct DispatchIndirectCommand {
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t num_groups_z;
};

struct TraceRaysIndirectCommand {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct alignas(8) PipelineMain {
#if defined(REN_VK_BACKEND)
    VkPipelineLayout layout = {};
    VkPipeline pipeline = {};
#endif
    RastState rast_state;
    ProgramROHandle prog;
    VertexInputROHandle vtx_input;
    RenderPassROHandle render_pass;

    bool Equals(const RastState &_rast_state, const ProgramROHandle _prog, const VertexInputROHandle _vtx_input,
                const RenderPassROHandle _render_pass) const {
        return vtx_input == _vtx_input && prog == _prog && render_pass == _render_pass && rast_state == _rast_state;
    }
    bool LessThan(const RastState &_rast_state, const ProgramROHandle _prog, const VertexInputROHandle _vtx_input,
                  const RenderPassROHandle _render_pass) const {
        if (vtx_input < _vtx_input) {
            return true;
        } else if (vtx_input == _vtx_input) {
            if (prog < _prog) {
                return true;
            } else if (prog == _prog) {
                if (render_pass < _render_pass) {
                    return true;
                } else if (render_pass == _render_pass) {
                    return rast_state < _rast_state;
                }
            }
        }
        return false;
    }

    bool operator==(const PipelineMain &rhs) const {
        return vtx_input == rhs.vtx_input && prog == rhs.prog && render_pass == rhs.render_pass &&
               rast_state == rhs.rast_state;
    }
    bool operator!=(const PipelineMain &rhs) const {
        return vtx_input != rhs.vtx_input || prog != rhs.prog || render_pass != rhs.render_pass ||
               rast_state != rhs.rast_state;
    }
    bool operator<(const PipelineMain &rhs) const {
        return LessThan(rhs.rast_state, rhs.prog, rhs.vtx_input, rhs.render_pass);
    }
};

struct PipelineCold {
    ePipelineType type = ePipelineType::Undefined;
#if defined(REN_VK_BACKEND)
    SmallVector<VkRayTracingShaderGroupCreateInfoKHR, 4> rt_shader_groups;

    VkStridedDeviceAddressRegionKHR rgen_region = {};
    VkStridedDeviceAddressRegionKHR miss_region = {};
    VkStridedDeviceAddressRegionKHR hit_region = {};
    VkStridedDeviceAddressRegionKHR call_region = {};

    BufferHandle rt_sbt_buf;
#endif
};

bool Pipeline_Init(const ApiContext &api, const SparseDualStorage<ShaderMain, ShaderCold> &shaders,
                   const SparseDualStorage<ProgramMain, ProgramCold> &programs,
                   SparseDualStorage<BufferMain, BufferCold> &buffers, PipelineMain &pipeline_main,
                   PipelineCold &pipeline_cold, ProgramROHandle prog, ILog *log, int subgroup_size = -1);
bool Pipeline_Init(const ApiContext &api, const StoragesRef &storages, PipelineMain &pipeline_main,
                   PipelineCold &pipeline_cold, const RastState &rast_state, ProgramROHandle prog,
                   VertexInputROHandle vtx_input, RenderPassROHandle render_pass, uint32_t subpass_index, ILog *log);
void Pipeline_Destroy(const ApiContext &api, SparseDualStorage<BufferMain, BufferCold> &buffers,
                      PipelineMain &pipeline_main, PipelineCold &pipeline_cold);
void Pipeline_DestroyImmediately(const ApiContext &api, SparseDualStorage<BufferMain, BufferCold> &buffers,
                                 PipelineMain &pipeline_main, PipelineCold &pipeline_cold);
} // namespace Ren
