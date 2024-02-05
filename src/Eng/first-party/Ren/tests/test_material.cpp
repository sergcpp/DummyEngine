#include "test_common.h"

#include "../Context.h"
#include "../Material.h"
#include "../RenderPass.h"
#include "../VertexInput.h"

static Ren::ProgramRef OnProgramNeeded(const char *name, const char *arg1, const char *arg2) { return {}; }

static Ren::Tex2DRef OnTextureNeeded(const char *name) { return {}; }

void test_material() {
    printf("Test material           | ");

    { // Load material
        TestContext test;

        auto on_pipelines_needed = [&](const char *prog_name, const uint32_t flags, const char *arg1,
                                       const char *arg2, const char *arg3, const char *arg4,
                                       Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) {
            out_pipelines.emplace_back(nullptr, 0);
        };

        auto on_texture_needed = [&test](const char *name, const uint8_t color[4], const Ren::eTexFlags flags) {
            Ren::eTexLoadStatus status;
            Ren::Tex2DParams p;
            return test.LoadTexture2D(name, nullptr, 0, p, test.default_stage_bufs(), test.default_mem_allocs(),
                                      &status);
        };

        auto on_sampler_needed = [&test](Ren::SamplingParams params) {
            Ren::eSamplerLoadStatus status;
            return test.LoadSampler(params, &status);
        };

        const char *mat_src = "gl_program: constant constant.vs constant.fs\n"
                              "sw_program: constant\n"
                              "flag: alpha_test\n"
                              "texture: checker.tga\n"
                              "texture: checker.tga signed\n"
                              "texture: metal_01.tga\n"
                              "texture: checker.tga\n"
                              "param: 0 1 2 3\n"
                              "param: 0.5 1.2 11 15";

        Ren::eMatLoadStatus status;
        Ren::MaterialRef m_ref =
            test.LoadMaterial("mat1", nullptr, &status, on_pipelines_needed, on_texture_needed, on_sampler_needed);
        require(status == Ren::eMatLoadStatus::SetToDefault);

        { require(!m_ref->ready()); }

        test.LoadMaterial("mat1", mat_src, &status, on_pipelines_needed, on_texture_needed, on_sampler_needed);

        require(status == Ren::eMatLoadStatus::CreatedFromData);
        require(m_ref->flags() & uint32_t(Ren::eMatFlags::AlphaTest));
        require(m_ref->ready());
        require(m_ref->name() == "mat1");

        Ren::Tex2DRef t0 = m_ref->textures[0];
        Ren::Tex2DRef t1 = m_ref->textures[1];
        Ren::Tex2DRef t2 = m_ref->textures[2];
        Ren::Tex2DRef t3 = m_ref->textures[3];

        require(t0 == t1);
        require(t0 == t3);

        require(t0->name() == "checker.tga");
        require(t1->name() == "checker.tga");
        require(t2->name() == "metal_01.tga");
        require(t3->name() == "checker.tga");

        require(m_ref->params[0] == Ren::Vec4f(0, 1, 2, 3));
        require(m_ref->params[1] == Ren::Vec4f(0.5f, 1.2f, 11, 15));
    }

    printf("OK\n");
}
