#include "test_common.h"

#include "../Context.h"
#include "../Material.h"
#include "../RenderPass.h"
#include "../VertexInput.h"

void test_material() {
    using namespace Ren;

    printf("Test material           | ");

    { // Load material
        TestContext test;

        auto on_pipelines_needed = [&](const uint32_t flags, std::string_view arg1, std::string_view arg2,
                                       std::string_view arg3, std::string_view arg4,
                                       SmallVectorImpl<PipelineRef> &out_pipelines) {
            out_pipelines.emplace_back(nullptr, 0);
        };

        auto on_texture_needed = [&test](std::string_view name, const uint8_t color[4],
                                         const Bitmask<eImgFlags> flags) {
            eImgLoadStatus status;
            ImgParams p;
            p.w = p.h = 1;
            p.format = eFormat::RGBA8;
            p.usage = eImgUsage::Transfer;
            p.flags = eImgFlags::Stub;
            return test.LoadImage(name, Span{color, color + 4}, p, test.default_mem_allocs(), &status);
        };

        auto on_sampler_needed = [&test](SamplingParams params) {
            eSamplerLoadStatus status;
            return test.LoadSampler(params, &status);
        };

        const char mat_src[] = "pipelines:\n"
                               "    - constant.vert.glsl constant.frag.glsl\n"
                               "    - constant2.vert.glsl constant2.frag.glsl\n"
                               "flags:\n"
                               "    - alpha_test\n"
                               "textures:\n"
                               "    - checker.tga\n"
                               "    - checker.tga\n"
                               "    - metal_01.tga\n"
                               "    - checker.tga\n"
                               "params:\n"
                               "    - 0 1 2 3\n"
                               "    - 0.5 1.2 11 15";

        eMatLoadStatus status;
        MaterialRef m_ref =
            test.LoadMaterial("mat1.mat", {}, &status, on_pipelines_needed, on_texture_needed, on_sampler_needed);
        require(status == eMatLoadStatus::SetToDefault);
        require(!m_ref->ready());

        test.LoadMaterial("mat1.mat", mat_src, &status, on_pipelines_needed, on_texture_needed, on_sampler_needed);

        require(status == eMatLoadStatus::CreatedFromData);
        require(m_ref->flags & eMatFlags::AlphaTest);
        require(m_ref->ready());
        require(m_ref->name() == "mat1.mat");

        ImgRef t0 = m_ref->textures[0];
        ImgRef t1 = m_ref->textures[1];
        ImgRef t2 = m_ref->textures[2];
        ImgRef t3 = m_ref->textures[3];

        require(t0 == t1);
        require(t0 == t3);

        require(t0->name() == "checker.tga");
        require(t1->name() == "checker.tga");
        require(t2->name() == "metal_01.tga");
        require(t3->name() == "checker.tga");

        require(m_ref->params[0] == Vec4f(0, 1, 2, 3));
        require(m_ref->params[1] == Vec4f(0.5f, 1.2f, 11, 15));
    }

    printf("OK\n");
}
