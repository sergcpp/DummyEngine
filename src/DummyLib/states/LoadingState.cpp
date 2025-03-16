#include "LoadingState.h"

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/renderer/ParseJs.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/utils/ShaderLoader.h>
#include <Gui/BaseElement.h>
#include <Gui/BitmapFont.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/PoolAlloc.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../widgets/FontStorage.h"
#include "DrawTest.h"

#include "../../assets/shaders/blit_loading_interface.h"

LoadingState::LoadingState(Viewer *viewer) : viewer_(viewer), alloc_(32, 512), pipelines_to_init_(alloc_) {
    ren_ctx_ = viewer->ren_ctx();

    log_ = viewer->log();
    scene_manager_ = viewer_->scene_manager();
    sh_loader_ = viewer_->shader_loader();
    prim_draw_ = &viewer_->renderer()->prim_draw();
    threads_ = viewer_->threads();

    ui_renderer_ = viewer->ui_renderer();
    ui_root_ = viewer->ui_root();

    font_ = viewer->font_storage()->FindFont("large_font");
}

LoadingState::~LoadingState() = default;

void LoadingState::Enter() {
    Ren::TexParams params;
    params.w = params.h = 1;
    params.format = Ren::eTexFormat::RGBA8;

    Ren::eTexLoadStatus load_status;
    dummy_white_ = ren_ctx_->LoadTextureRegion("dummy_white", Gui::ColorWhite, params, &load_status);
    assert(load_status == Ren::eTexLoadStatus::CreatedFromData);

    { // Init loading program
        blit_loading_prog_ = sh_loader_->LoadProgram("blit_loading.vert.glsl", "blit_loading.frag.glsl");
    }

    { // Load pipelines description
        const std::string pipelines_file_name = std::string(ASSETS_BASE_PATH) + "/shaders/pipelines.json";
        Sys::AssetFile in_file(pipelines_file_name);
        if (!in_file) {
            log_->Error("Failed to open %s", pipelines_file_name.c_str());
            return;
        }

        const size_t file_size = in_file.size();

        std::unique_ptr<uint8_t[]> file_data(new uint8_t[file_size]);
        const size_t bytes_read = in_file.Read((char *)&file_data[0], file_size);
        if (bytes_read != file_size) {
            log_->Error("Failed to read %s", pipelines_file_name.c_str());
            return;
        }

        Sys::MemBuf mem(&file_data[0], file_size);
        std::istream in_stream(&mem);

        if (!pipelines_to_init_.Read(in_stream)) {
            log_->Error("Failed to parse %s", pipelines_file_name.c_str());
            return;
        }
    }

#if defined(REN_VK_BACKEND)
    try {
        InitPipelines(pipelines_to_init_, 0, int(pipelines_to_init_.Size()));
    } catch (std::exception &e) {
        log_->Error("Failed to initialize pipelines! (%s)", e.what());
    }
#else
    curr_rast_state_.Apply();
#endif

    loading_start_ = Sys::GetTimeUs();
}

void LoadingState::Exit() {}

void LoadingState::Draw() {
    { // Update loop using fixed timestep
        Eng::InputManager *input_manager = viewer_->input_manager();
        Eng::FrameInfo &fr = fr_info_;

        fr.cur_time_us = Sys::GetTimeUs();
        if (fr.cur_time_us < fr.prev_time_us) {
            fr.prev_time_us = 0;
        }
        fr.delta_time_us = fr.cur_time_us - fr.prev_time_us;
        if (fr.delta_time_us > 200000) {
            fr.delta_time_us = 200000;
        }
        fr.prev_time_us = fr.cur_time_us;
        fr.time_acc_us += fr.delta_time_us;

        uint64_t poll_time_point = fr.cur_time_us - fr.time_acc_us;

        while (fr.time_acc_us >= Eng::UPDATE_DELTA) {
            Eng::input_event_t evt;
            while (input_manager->PollEvent(poll_time_point, evt)) {
                this->HandleInput(evt, input_manager->keys_state());
            }

            this->UpdateFixed(Eng::UPDATE_DELTA);
            fr.time_acc_us -= Eng::UPDATE_DELTA;

            poll_time_point += Eng::UPDATE_DELTA;
        }

        fr.time_fract = double(fr.time_acc_us) / Eng::UPDATE_DELTA;
    }

    ren_ctx_->in_flight_frontend_frame[ren_ctx_->backend_frame()] = ren_ctx_->next_frontend_frame =
        ren_ctx_->backend_frame();

    { // Solid black background
        const Gui::Vec2f pos[2] = {Gui::Vec2f{-1.0f, -1.0f}, Gui::Vec2f{1.0f, 1.0f}};
        const Gui::Vec2f uvs[2] = {Gui::Vec2f{float(dummy_white_->pos(0)), float(dummy_white_->pos(1))},
                                   Gui::Vec2f{float(dummy_white_->pos(0)), float(dummy_white_->pos(1))}};
        ui_renderer_->PushImageQuad(Gui::eDrawMode::Passthrough, dummy_white_->pos(2), Gui::ColorBlack, pos, uvs);
    }

    std::string loading_text = "Loading";
    const float text_width = font_->GetWidth(loading_text, ui_root_);

    const uint64_t timing = (fr_info_.cur_time_us - loading_start_) % 1000000;
    if (timing > 256000) {
        loading_text += ".";
    }
    if (timing > 512000) {
        loading_text += ".";
    }
    if (timing > 768000) {
        loading_text += ".";
    }

    if (!prim_draw_->LazyInit(*ren_ctx_)) {
        ren_ctx_->log()->Error("Failed to initialize primitive drawing!");
    }

    { // Draw abstract loading art
        const Ren::RenderTarget render_targets[] = {
            {ren_ctx_->backbuffer_ref(), Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        Loading::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, ren_ctx_->w(), ren_ctx_->h()};
        uniform_params.tex_size = Ren::Vec2f{float(ren_ctx_->w()), float(ren_ctx_->h())};
        uniform_params.time = float(fr_info_.cur_time_us);
        uniform_params.fade = std::min(float((fr_info_.cur_time_us - loading_start_) / 1000000.0), 1.0f);

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.viewport[2] = ren_ctx_->w();
        rast_state.viewport[3] = ren_ctx_->h();

        prim_draw_->DrawPrim(Eng::PrimDraw::ePrim::Quad, blit_loading_prog_, {}, render_targets, rast_state,
                             curr_rast_state_, {}, &uniform_params, sizeof(Loading::Params), 0);
    }

    font_->DrawText(ui_renderer_, loading_text, Gui::Vec2f{-0.5f * text_width, 0.0f}, Gui::ColorWhite, 1.0f, ui_root_);

    ui_renderer_->Draw(ren_ctx_->w(), ren_ctx_->h());

#if defined(REN_VK_BACKEND)
    bool ready = true;
    while (!futures_.empty()) {
        ready &= futures_.back().wait_for(std::chrono::milliseconds(10)) == std::future_status::ready;
        if (ready) {
            futures_.pop_back();
        } else {
            break;
        }
    }
#else
    InitPipelines(pipelines_to_init_, pipeline_index_++, 1);
    const bool ready = pipeline_index_ > int(pipelines_to_init_.Size());
#endif

    if (ready) {
        viewer_->state_manager()->Switch(std::make_unique<DrawTest>(viewer_));
    }
}

bool LoadingState::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    const bool handled = ui_root_->HandleInput(evt, keys_state);
    if (handled) {
        return true;
    }
    return false;
}

void LoadingState::InitPipelines(const Sys::JsArrayP &js_pipelines, const int start, const int count) {
    for (int i = start; i < std::min(start + count, int(js_pipelines.elements.size())); ++i) {
        const Sys::JsObjectP &js_pipeline = js_pipelines.elements[i].as_obj();
        const Sys::JsStringP &js_pi_type = js_pipeline.at("type").as_str();
        const Sys::JsArrayP &js_pi_shaders = js_pipeline.at("shaders").as_arr();
        if (js_pipeline.Has("features")) {
            const Sys::JsObjectP &js_features = js_pipeline.at("features").as_obj();
            if (js_features.Has("bindless")) {
                const Sys::JsLiteral &js_bindless = js_features.at("bindless").as_lit();
#if defined(REN_VK_BACKEND)
                if (js_bindless.val != Sys::JsLiteralType::True) {
                    continue;
                }
#else
                if ((js_bindless.val == Sys::JsLiteralType::True) != ren_ctx_->capabilities.bindless_texture) {
                    continue;
                }
#endif
            }
            if (js_features.Has("subgroup")) {
                const Sys::JsLiteral &js_subgroup = js_features.at("subgroup").as_lit();
                if ((js_subgroup.val == Sys::JsLiteralType::True) != ren_ctx_->capabilities.subgroup) {
                    continue;
                }
            }
            if (js_features.Has("hwrt")) {
                const Sys::JsLiteral &js_hwrt = js_features.at("hwrt").as_lit();
                if ((js_hwrt.val == Sys::JsLiteralType::True) != ren_ctx_->capabilities.hwrt) {
                    continue;
                }
            }
            if (js_features.Has("swrt")) {
                const Sys::JsLiteral &js_swrt = js_features.at("swrt").as_lit();
                if ((js_swrt.val == Sys::JsLiteralType::True) != ren_ctx_->capabilities.swrt) {
                    continue;
                }
            }
        }
        if (js_pi_type.val == "compute") {
            int subgroup_size = -1;
            if (js_pipeline.Has("subgroup_size")) {
                subgroup_size = int(js_pipeline.at("subgroup_size").as_num().val);
            }
            const std::string shader_name = js_pi_shaders.at(0).as_str().val.c_str();
#if defined(REN_VK_BACKEND)
            futures_.push_back(threads_->Enqueue(
                [shader_name, subgroup_size, this]() { sh_loader_->LoadPipeline(shader_name, subgroup_size); }));
#else
            sh_loader_->LoadPipeline(shader_name, subgroup_size);
#endif
        } else if (js_pi_type.val == "graphics") {
            Ren::RastState rast_state;
            if (js_pipeline.Has("rast_state")) {
                const Sys::JsObjectP &js_rast_state = js_pipeline.at("rast_state").as_obj();
                rast_state = Eng::ParseRastState(js_rast_state);
            }

            Ren::ProgramRef prog =
                sh_loader_->LoadProgram(js_pi_shaders.at(0).as_str().val, js_pi_shaders.at(1).as_str().val);

            Ren::SmallVector<Ren::VtxAttribDesc, 4> attribs;
            if (js_pipeline.Has("vertex_input")) {
                const Sys::JsArrayP &js_vertex_input = js_pipeline.at("vertex_input").as_arr();
                for (const Sys::JsElementP &el : js_vertex_input.elements) {
                    const Sys::JsObjectP &js_attrib = el.as_obj();

                    Ren::VtxAttribDesc &desc = attribs.emplace_back();
                    if (int(js_attrib.at("buf").as_num().val) == 0) {
                        desc.buf = scene_manager_->scene_data().persistent_data.vertex_buf1;
                    } else if (int(js_attrib.at("buf").as_num().val) == 1) {
                        desc.buf = scene_manager_->scene_data().persistent_data.vertex_buf2;
                    }
                    desc.loc = uint8_t(js_attrib.at("loc").as_num().val);
                    desc.size = uint8_t(js_attrib.at("size").as_num().val);
                    desc.type = Ren::Type(js_attrib.at("type").as_str().val);
                    desc.stride = uint8_t(js_attrib.at("stride").as_num().val);
                    desc.offset = uint32_t(js_attrib.at("offset").as_num().val);
                }
            }

            Ren::VertexInputRef vtx_input =
                sh_loader_->LoadVertexInput(attribs, scene_manager_->scene_data().persistent_data.indices_buf);

            Ren::RenderTargetInfo depth_rt;
            Ren::SmallVector<Ren::RenderTargetInfo, 4> color_rts;
            if (js_pipeline.Has("render_pass")) {
                const Sys::JsObjectP &js_render_pass = js_pipeline.at("render_pass").as_obj();
                if (js_render_pass.Has("depth")) {
                    const Sys::JsObjectP &js_rt_info = js_render_pass.at("depth").as_obj();
                    depth_rt = Eng::ParseRTInfo(js_rt_info);
                    depth_rt.layout = Ren::eImageLayout::DepthStencilAttachmentOptimal;
                }
                if (js_render_pass.Has("color")) {
                    const Sys::JsArrayP &js_color_rts = js_render_pass.at("color").as_arr();
                    for (const Sys::JsElementP &js_rt : js_color_rts.elements) {
                        const Sys::JsObjectP &js_rt_info = js_rt.as_obj();
                        color_rts.push_back(Eng::ParseRTInfo(js_rt_info));
                        color_rts.back().layout = Ren::eImageLayout::ColorAttachmentOptimal;
                    }
                }
            }

            Ren::RenderPassRef render_pass = sh_loader_->LoadRenderPass(depth_rt, color_rts);

#if defined(REN_VK_BACKEND)
            futures_.push_back(threads_->Enqueue([rast_state, prog, vtx_input, render_pass, this]() {
                sh_loader_->LoadPipeline(rast_state, prog, vtx_input, render_pass, 0);
            }));
#else
            sh_loader_->LoadPipeline(rast_state, prog, vtx_input, render_pass, 0);
#endif
        }
    }
}
