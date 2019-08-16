#include "RpDebugEllipsoids.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>


#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"
#include "../assets/shaders/internal/ellipsoid_interface.glsl"

void RpDebugEllipsoids::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
                              const char shared_data_buf_name[], const char output_tex_name[]) {

    view_state_ = view_state;
    ellipsoids_ = list.ellipsoids;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::VertexShader, *this);
    output_tex_ =
        builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpDebugEllipsoids::Execute(RpBuilder &builder) {
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);
    DrawProbes(builder);
}

void RpDebugEllipsoids::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        ellipsoid_prog_ =
            sh.LoadProgram(ctx, "ellipsoid_prog", "internal/ellipsoid.vert.glsl", "internal/ellipsoid.frag.glsl");
        assert(ellipsoid_prog_->ready());

        initialized = true;
    }

    if (!draw_fb_.Setup(ctx.api_ctx(), {}, output_tex.desc.w, output_tex.desc.h, output_tex.ref, {}, {},
                        view_state_->is_multisampled)) {
        ctx.log()->Error("RpDebugEllipsoids: draw_fb_ init failed!");
    }
}

void RpDebugEllipsoids::DrawProbes(RpBuilder &builder) {
    Ren::RastState rast_state;
    rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

    for (int i = 0; i < int(ellipsoids_.count); i++) {
        const EllipsItem &e = ellipsoids_.data[i];

        auto sph_ls = Ren::Mat3f{Ren::Uninitialize};
        sph_ls[0] = Ren::Vec3f{0.0f};
        sph_ls[0][e.perp] = 1.0f;
        sph_ls[1] = Ren::MakeVec3(e.axis);
        sph_ls[2] = Ren::Normalize(Ren::Cross(sph_ls[0], sph_ls[1]));
        sph_ls[0] = Ren::Normalize(Ren::Cross(sph_ls[1], sph_ls[2]));

        sph_ls *= e.radius;

        Ren::Mat4f world_from_object =
            Ren::Translate(Ren::Mat4f{}, Ren::Vec3f{e.position[0], e.position[1], e.position[2]});

        world_from_object[0] = Ren::Vec4f{sph_ls[0]};
        world_from_object[1] = Ren::Vec4f{sph_ls[1]};
        world_from_object[2] = Ren::Vec4f{sph_ls[2]};

        const PrimDraw::Uniform uniforms[] = {{Ellipsoid::U_M_MATRIX_LOC, &world_from_object}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, {&draw_fb_, 0}, ellipsoid_prog_.get(), bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms, sizeof(uniforms) / sizeof(uniforms[0]));
    }
}
