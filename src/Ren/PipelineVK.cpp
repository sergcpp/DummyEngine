#include "Pipeline.h"

#include "Program.h"
#include "RastState.h"
#include "VertexInput.h"

namespace Ren {
extern const VkShaderStageFlagBits g_shader_stages_vk[];

const VkCullModeFlagBits g_cull_modes_vk[] = {
    VK_CULL_MODE_NONE,      // None
    VK_CULL_MODE_FRONT_BIT, // Front
    VK_CULL_MODE_BACK_BIT   // Back
};
static_assert(COUNT_OF(g_cull_modes_vk) == int(eCullFace::_Count), "!");

const VkCompareOp g_compare_op_vk[] = {
    VK_COMPARE_OP_ALWAYS,          // Always
    VK_COMPARE_OP_NEVER,           // Never
    VK_COMPARE_OP_LESS,            // Less
    VK_COMPARE_OP_EQUAL,           // Equal
    VK_COMPARE_OP_GREATER,         // Greater
    VK_COMPARE_OP_LESS_OR_EQUAL,   // LEqual
    VK_COMPARE_OP_NOT_EQUAL,       // NotEqual
    VK_COMPARE_OP_GREATER_OR_EQUAL // GEqual
};
static_assert(COUNT_OF(g_compare_op_vk) == int(eCompareOp::_Count), "!");

const VkStencilOp g_stencil_op_vk[] = {
    VK_STENCIL_OP_KEEP,                // Keep
    VK_STENCIL_OP_ZERO,                // Zero
    VK_STENCIL_OP_REPLACE,             // Replace
    VK_STENCIL_OP_INCREMENT_AND_CLAMP, // Incr
    VK_STENCIL_OP_DECREMENT_AND_CLAMP, // Decr
    VK_STENCIL_OP_INVERT               // Invert
};
static_assert(COUNT_OF(g_stencil_op_vk) == int(eStencilOp::_Count), "!");

const VkPolygonMode g_poly_mode_vk[] = {
    VK_POLYGON_MODE_FILL, // Fill
    VK_POLYGON_MODE_LINE  // Line
};
static_assert(COUNT_OF(g_poly_mode_vk) == int(ePolygonMode::_Count), "!");

const VkBlendFactor g_blend_factor_vk[] = {
    VK_BLEND_FACTOR_ZERO,                // Zero
    VK_BLEND_FACTOR_ONE,                 // One
    VK_BLEND_FACTOR_SRC_COLOR,           // SrcColor
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, // OneMinusSrcColor
    VK_BLEND_FACTOR_DST_COLOR,           // DstColor
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, // OneMinusDstColor
    VK_BLEND_FACTOR_SRC_ALPHA,           // SrcAlpha
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // OneMinusSrcAlpha
    VK_BLEND_FACTOR_DST_ALPHA,           // DstAlpha
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  // OneMinusDstAlpha
};
static_assert(COUNT_OF(g_blend_factor_vk) == int(eBlendFactor::_Count), "!");

} // namespace Ren

Ren::Pipeline &Ren::Pipeline::operator=(Pipeline &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    rast_state_ = exchange(rhs.rast_state_, {});
    render_pass_ = exchange(rhs.render_pass_, nullptr);
    prog_ = std::move(rhs.prog_);
    vtx_input_ = exchange(rhs.vtx_input_, nullptr);
    layout_ = exchange(rhs.layout_, {});
    handle_ = exchange(rhs.handle_, {});

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

Ren::Pipeline::~Pipeline() { Destroy(); }

void Ren::Pipeline::Destroy() {
    if (layout_ != VK_NULL_HANDLE) {
        api_ctx_->pipeline_layouts_to_destroy[api_ctx_->backend_frame].emplace_back(layout_);
        layout_ = VK_NULL_HANDLE;
    }
    if (handle_ != VK_NULL_HANDLE) {
        api_ctx_->pipelines_to_destroy[api_ctx_->backend_frame].emplace_back(handle_);
        handle_ = VK_NULL_HANDLE;
    }
}

bool Ren::Pipeline::Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog,
                         const VertexInput *vtx_input, const RenderPass *render_pass, ILog *log) {
    Destroy();

    SmallVector<VkPipelineShaderStageCreateInfo, int(eShaderType::_Count)> shader_stage_create_info;
    for (int i = 0; i < int(eShaderType::_Count); ++i) {
        const ShaderRef &sh = prog->shader(eShaderType(i));
        if (!sh) {
            continue;
        }

        auto &stage_info = shader_stage_create_info.emplace_back();
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = g_shader_stages_vk[i];
        stage_info.module = prog->shader(eShaderType(i))->module();
        stage_info.pName = "main";
        stage_info.pSpecializationInfo = nullptr;
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = prog->descr_set_layouts_count();
        layout_create_info.pSetLayouts = prog->descr_set_layouts();
        layout_create_info.pushConstantRangeCount = prog->pc_range_count();
        layout_create_info.pPushConstantRanges = prog->pc_ranges();

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &layout_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create pipeline layout!");
            return false;
        }
    }

    { // create graphics pipeline
        SmallVector<VkVertexInputBindingDescription, 8> bindings;
        SmallVector<VkVertexInputAttributeDescription, 8> attribs;
        vtx_input->FillVKDescriptions(bindings, attribs);

        VkPipelineVertexInputStateCreateInfo vtx_input_state_create_info = {};
        vtx_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vtx_input_state_create_info.vertexBindingDescriptionCount = uint32_t(bindings.size());
        vtx_input_state_create_info.pVertexBindingDescriptions = bindings.cdata();
        vtx_input_state_create_info.vertexAttributeDescriptionCount = uint32_t(attribs.size());
        vtx_input_state_create_info.pVertexAttributeDescriptions = attribs.cdata();

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 1.0f;
        viewport.height = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissors = {};
        scissors.offset = {0, 0};
        scissors.extent = {1, 1};

        VkPipelineViewportStateCreateInfo viewport_state_ci = {};
        viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_ci.viewportCount = 1;
        viewport_state_ci.pViewports = &viewport;
        viewport_state_ci.scissorCount = 1;
        viewport_state_ci.pScissors = &scissors;

        VkPipelineRasterizationStateCreateInfo rasterization_state_ci = {};
        rasterization_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_ci.depthClampEnable = VK_FALSE;
        rasterization_state_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_ci.polygonMode = g_poly_mode_vk[rast_state.poly.mode];
        rasterization_state_ci.cullMode = g_cull_modes_vk[rast_state.poly.cull];

        rasterization_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state_ci.depthBiasEnable =
            (eDepthBiasMode(rast_state.poly.depth_bias_mode) != eDepthBiasMode::Disabled) ? VK_TRUE : VK_FALSE;
        rasterization_state_ci.depthBiasConstantFactor = rast_state.depth_bias.constant_offset;
        rasterization_state_ci.depthBiasClamp = 0.0f;
        rasterization_state_ci.depthBiasSlopeFactor = rast_state.depth_bias.slope_factor;
        rasterization_state_ci.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample_state_ci = {};
        multisample_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_ci.sampleShadingEnable = VK_FALSE;
        multisample_state_ci.minSampleShading = 0;
        multisample_state_ci.pSampleMask = nullptr;
        multisample_state_ci.alphaToCoverageEnable = VK_FALSE;
        multisample_state_ci.alphaToOneEnable = VK_FALSE;

        VkStencilOpState stencil_state = {};
        stencil_state.failOp = g_stencil_op_vk[int(rast_state.stencil.stencil_fail)];
        stencil_state.passOp = g_stencil_op_vk[int(rast_state.stencil.pass)];
        stencil_state.depthFailOp = g_stencil_op_vk[int(rast_state.stencil.depth_fail)];
        stencil_state.compareOp = g_compare_op_vk[int(rast_state.stencil.compare_op)];
        stencil_state.compareMask = rast_state.stencil.compare_mask;
        stencil_state.writeMask = rast_state.stencil.write_mask;
        stencil_state.reference = rast_state.stencil.reference;

        VkPipelineDepthStencilStateCreateInfo depth_state_ci = {};
        depth_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_state_ci.depthTestEnable = rast_state.depth.test_enabled ? VK_TRUE : VK_FALSE;
        depth_state_ci.depthWriteEnable = rast_state.depth.write_enabled ? VK_TRUE : VK_FALSE;
        depth_state_ci.depthCompareOp = g_compare_op_vk[int(rast_state.depth.compare_op)];
        depth_state_ci.depthBoundsTestEnable = VK_FALSE;
        depth_state_ci.stencilTestEnable = rast_state.stencil.enabled ? VK_TRUE : VK_FALSE;
        depth_state_ci.front = stencil_state;
        depth_state_ci.back = stencil_state;
        depth_state_ci.minDepthBounds = 0.0f;
        depth_state_ci.maxDepthBounds = 1.0f;

        VkPipelineColorBlendAttachmentState color_blend_attachment_states[Ren::MaxRTAttachments] = {};
        for (size_t i = 0; i < render_pass->color_rts.size(); ++i) {
            color_blend_attachment_states[i].blendEnable = rast_state.blend.enabled ? VK_TRUE : VK_FALSE;
            color_blend_attachment_states[i].colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment_states[i].srcColorBlendFactor = g_blend_factor_vk[int(rast_state.blend.src)];
            color_blend_attachment_states[i].dstColorBlendFactor = g_blend_factor_vk[int(rast_state.blend.dst)];
            color_blend_attachment_states[i].alphaBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment_states[i].srcAlphaBlendFactor = g_blend_factor_vk[int(rast_state.blend.src)];
            color_blend_attachment_states[i].dstAlphaBlendFactor = g_blend_factor_vk[int(rast_state.blend.dst)];
            color_blend_attachment_states[i].colorWriteMask = 0xf;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_CLEAR;
        color_blend_state_ci.attachmentCount = uint32_t(render_pass->color_rts.size());
        color_blend_state_ci.pAttachments = color_blend_attachment_states;
        color_blend_state_ci.blendConstants[0] = 0.0f;
        color_blend_state_ci.blendConstants[1] = 0.0f;
        color_blend_state_ci.blendConstants[2] = 0.0f;
        color_blend_state_ci.blendConstants[3] = 0.0f;

        SmallVector<VkDynamicState, 8> dynamic_states;
        dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
        if (eDepthBiasMode(rast_state.poly.depth_bias_mode) == eDepthBiasMode::Dynamic) {
            dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        }

        VkPipelineDynamicStateCreateInfo dynamic_state_ci = {};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = uint32_t(dynamic_states.size());
        dynamic_state_ci.pDynamicStates = dynamic_states.cdata();

        VkGraphicsPipelineCreateInfo pipeline_create_info = {};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = uint32_t(shader_stage_create_info.size());
        pipeline_create_info.pStages = shader_stage_create_info.cdata();
        pipeline_create_info.pVertexInputState = &vtx_input_state_create_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
        pipeline_create_info.pTessellationState = nullptr;
        pipeline_create_info.pViewportState = &viewport_state_ci;
        pipeline_create_info.pRasterizationState = &rasterization_state_ci;
        pipeline_create_info.pMultisampleState = &multisample_state_ci;
        pipeline_create_info.pDepthStencilState = &depth_state_ci;
        pipeline_create_info.pColorBlendState = &color_blend_state_ci;
        pipeline_create_info.pDynamicState = &dynamic_state_ci;
        pipeline_create_info.layout = layout_;
        pipeline_create_info.renderPass = render_pass->handle();
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = 0;

        const VkResult res =
            vkCreateGraphicsPipelines(api_ctx->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &handle_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create graphics pipeline!");
            return false;
        }
    }

    api_ctx_ = api_ctx;
    type_ = ePipelineType::Graphics;
    rast_state_ = rast_state;
    render_pass_ = render_pass;
    prog_ = std::move(prog);
    vtx_input_ = vtx_input;

    return true;
}

bool Ren::Pipeline::Init(ApiContext* api_ctx, ProgramRef prog, ILog* log) {
    Destroy();

    SmallVector<VkPipelineShaderStageCreateInfo, int(eShaderType::_Count)> shader_stage_create_info;
    for (int i = 0; i < int(eShaderType::_Count); ++i) {
        const ShaderRef &sh = prog->shader(eShaderType(i));
        if (!sh) {
            continue;
        }

        auto &stage_info = shader_stage_create_info.emplace_back();
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = g_shader_stages_vk[i];
        stage_info.module = prog->shader(eShaderType(i))->module();
        stage_info.pName = "main";
        stage_info.pSpecializationInfo = nullptr;
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = prog->descr_set_layouts_count();
        layout_create_info.pSetLayouts = prog->descr_set_layouts();
        layout_create_info.pushConstantRangeCount = prog->pc_range_count();
        layout_create_info.pPushConstantRanges = prog->pc_ranges();

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &layout_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create pipeline layout!");
            return false;
        }
    }

    { // create compute pipeline
        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = shader_stage_create_info[0];
        info.layout = layout_;

        const VkResult res = vkCreateComputePipelines(api_ctx->device, VK_NULL_HANDLE, 1, &info, nullptr, &handle_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create pipeline!");
            return false;
        }

        type_ = ePipelineType::Compute;
    }

    api_ctx_ = api_ctx;
    prog_ = std::move(prog);

    return true;
}