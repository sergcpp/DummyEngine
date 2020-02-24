#include "RpOpaque.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpOpaque::Setup(Graph::RpBuilder &builder, const DrawList &list,
                     const ViewState *view_state, int orphan_index,
                     Ren::TexHandle color_tex, Ren::TexHandle normal_tex,
                     Ren::TexHandle spec_tex, Ren::TexHandle depth_tex,
                     Graph::ResourceHandle in_instances_buf,
                     Ren::Tex1DRef instances_tbo,
                     Graph::ResourceHandle in_shared_data_buf, Ren::TexHandle shadow_tex,
                     Ren::TexHandle ssao_tex, Ren::Tex1DRef lights_tbo,
                     Ren::Tex1DRef decals_tbo, Ren::Tex1DRef cells_tbo,
                     Ren::Tex1DRef items_tbo, Ren::Tex2DRef brdf_lut,
                     Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut) {
    orphan_index_ = orphan_index;

    color_tex_ = color_tex;
    normal_tex_ = normal_tex;
    spec_tex_ = spec_tex;
    depth_tex_ = depth_tex;
    view_state_ = view_state;

    instances_tbo_ = std::move(instances_tbo);
    lights_tbo_ = std::move(lights_tbo);
    decals_tbo_ = std::move(decals_tbo);
    cells_tbo_ = std::move(cells_tbo);
    items_tbo_ = std::move(items_tbo);

    brdf_lut_ = std::move(brdf_lut);
    noise_tex_ = std::move(noise_tex);
    cone_rt_lut_ = std::move(cone_rt_lut);

    shadow_tex_ = shadow_tex;
    ssao_tex_ = ssao_tex;
    env_ = &list.env;
    decals_atlas_ = list.decals_atlas;
    probe_storage_ = list.probe_storage;

    render_flags_ = list.render_flags;
    main_batches_ = list.main_batches;
    main_batch_indices_ = list.main_batch_indices;

    input_[0] = builder.ReadBuffer(in_instances_buf);
    input_[1] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 2;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpOpaque::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    DrawOpaque(builder);
}

void RpOpaque::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        static const uint8_t black[] = {0, 0, 0, 0}, white[] = {255, 255, 255, 255};

        Ren::Texture2DParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.filter = Ren::eTexFilter::NoFilter;
        p.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx.LoadTexture2D("dummy_black", black, sizeof(black), p, &status);
        assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
               status == Ren::eTexLoadStatus::TexFound);

        dummy_white_ = ctx.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
        assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
               status == Ren::eTexLoadStatus::TexFound);

        initialized = true;
    }

    Ren::BufHandle vtx_buf1 = ctx.default_vertex_buf1()->handle(),
                   vtx_buf2 = ctx.default_vertex_buf2()->handle(),
                   ndx_buf = ctx.default_indices_buf()->handle();

    const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride,
             uintptr_t(3 * sizeof(float))},
            // Attributes from buffer 2
            {vtx_buf2, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf1_stride, 0},
            {vtx_buf2, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf1_stride,
             uintptr_t(4 * sizeof(uint16_t))},
            {vtx_buf2, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride,
             uintptr_t(6 * sizeof(uint16_t))}};

        draw_pass_vao_.Setup(attribs, 5, ndx_buf);
    }

    const Ren::TexHandle attachments[] = {color_tex_, normal_tex_, spec_tex_};
    if (!opaque_draw_fb_.Setup(attachments, 3, depth_tex_, depth_tex_,
                               view_state_->is_multisampled)) {
        ctx.log()->Error("RpOpaque: opaque_draw_fb_ init failed!");
    }
}