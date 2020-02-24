#include "RpDebugEllipsoids.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDebugEllipsoids::Setup(Graph::RpBuilder &builder, const DrawList &list,
                              const ViewState *view_state,
                              Graph::ResourceHandle in_shared_data_buf,
                              Ren::TexHandle output_tex) {

    view_state_ = view_state;
    output_tex_ = output_tex;

    ellipsoids_ = list.ellipsoids;

    input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpDebugEllipsoids::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    DrawProbes(builder);
}

void RpDebugEllipsoids::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        ellipsoid_prog_ =
            sh.LoadProgram(ctx, "ellipsoid_prog", "internal/ellipsoid.vert.glsl",
                           "internal/ellipsoid.frag.glsl");
        assert(ellipsoid_prog_->ready());

        initialized = true;
    }

    if (!draw_fb_.Setup(&output_tex_, 1, {}, {}, view_state_->is_multisampled)) {
        ctx.log()->Error("RpDebugEllipsoids: draw_fb_ init failed!");
    }
}

void RpDebugEllipsoids::DrawProbes(Graph::RpBuilder &builder) {
    Ren::RastState rast_state;
    rast_state.polygon_mode = Ren::ePolygonMode::Line;
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

    const PrimDraw::Binding bindings[] = {{Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                                           unif_shared_data_buf.ref->handle()}};

    for (int i = 0; i < int(ellipsoids_.count); i++) {
        const EllipsItem &e = ellipsoids_.data[i];

        auto sph_ls = Ren::Mat3f{Ren::Uninitialize};
        sph_ls[0] = Ren::Vec3f{0.0f};
        sph_ls[0][e.perp] = 1.0f;
        sph_ls[1] = Ren::MakeVec3(e.axis);
        sph_ls[2] = Ren::Normalize(Ren::Cross(sph_ls[0], sph_ls[1]));
        sph_ls[0] = Ren::Normalize(Ren::Cross(sph_ls[1], sph_ls[2]));

        sph_ls *= e.radius;

        Ren::Mat4f world_from_object = Ren::Translate(
            Ren::Mat4f{}, Ren::Vec3f{e.position[0], e.position[1], e.position[2]});

        world_from_object[0] = Ren::Vec4f{sph_ls[0]};
        world_from_object[1] = Ren::Vec4f{sph_ls[1]};
        world_from_object[2] = Ren::Vec4f{sph_ls[2]};

        const PrimDraw::Uniform uniforms[] = {{REN_U_M_MATRIX_LOC, &world_from_object}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, {draw_fb_.id(), 0},
                            ellipsoid_prog_.get(), bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms,
                            sizeof(uniforms) / sizeof(uniforms[0]));
    }
}
