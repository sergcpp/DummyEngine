#ifdef _MSC_VER
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif
#include "Viewer.h"

#include <fstream>
#include <sstream>

#include <Eng/ViewerStateManager.h>
#include <Eng/renderer/Renderer.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/utils/Random.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Eng/widgets/DebugFrameUI.h>
#include <Ray/Ray.h>
#include <Ren/Context.h>
#include <Ren/MVec.h>
#include <Sys/ThreadPool.h>

#if defined(REN_VK_BACKEND)
#include <Ren/VKCtx.h>

#include <optick/optick.h>

namespace Ren {
extern bool ignore_optick_errors;
}
#elif defined(REN_GL_BACKEND)
#include <Ren/GLCtx.h>
#endif

#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Time_.h>

#include "states/LoadingState.h"
#include "utils/Dictionary.h"
#include "utils/Log.h"
#include "widgets/FontStorage.h"

Viewer::Viewer(const int w, const int h, const AppParams &_app_params, ILog *log)
    : ViewerBase(w, h, _app_params.validation_level, _app_params.nohwrt, _app_params.nosubgroup, log,
                 _app_params.device_name),
      log_(log), app_params(_app_params) {
    Sys::JsObject main_config;

    {
        // load config
#if defined(__ANDROID__)
        Sys::AssetFile config_file("assets/config.json", Sys::eOpenMode::In);
#else
        Sys::AssetFile config_file("assets_pc/config.json", Sys::eOpenMode::In);
#endif
        size_t config_file_size = config_file.size();

        std::unique_ptr<char[]> buf(new char[config_file_size]);
        config_file.Read(buf.get(), config_file_size);

        std::stringstream ss;
        ss.write(buf.get(), config_file_size);

        if (!main_config.Read(ss)) {
            throw std::runtime_error("Unable to load main config!");
        }
    }

    const Sys::JsObject &ui_settings = main_config.at("ui_settings").as_obj();

    { // load fonts
        font_storage_ = std::make_unique<FontStorage>();

        const Sys::JsObject &fonts = ui_settings.at("fonts").as_obj();
        for (const auto &el : fonts.elements) {
            const std::string &name = el.first;

#if defined(__ANDROID__)
            std::string file_name = std::string("assets/");
#else
            std::string file_name = std::string("assets_pc/");
#endif
            file_name += el.second.as_str().val;

            [[maybe_unused]] const Gui::BitmapFont *loaded_font = font_storage_->LoadFont(name, file_name, *ren_ctx_);
        }
    }

    // create UI for commandline
    cmdline_ui_ = std::make_unique<Eng::CmdlineUI>(*ren_ctx_, *font_storage_->FindFont("main_font"),
                                                   Gui::Vec2f{-1.0f, 0.5f}, Gui::Vec2f{2.0f, 0.5f}, ui_root_.get());

    // create UI for performance debugging
    debug_ui_ = std::make_unique<Eng::DebugFrameUI>(*ren_ctx_, Gui::Vec2f{-1.0f}, Gui::Vec2f{2.0f, 1.75f},
                                                    ui_root_.get(), font_storage_->FindFont("main_font"),
                                                    font_storage_->FindFont("large_font"));

#if defined(__ANDROID__)
    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);
    const Ren::Context *p_ctx = ren_ctx.get();
    input_manager->SetConverter(EvP1Move, [p_ctx](InputManager::Event &evt) {
        evt.move.dx *= 300.0f / p_ctx->w();
        evt.move.dy *= 300.0f / p_ctx->w();
    });
    input_manager->SetConverter(EvP2Move, [p_ctx](InputManager::Event &evt) {
        evt.move.dx *= 300.0f / p_ctx->w();
        evt.move.dy *= 300.0f / p_ctx->w();
    });
#endif

    dictionary_ = std::make_unique<Dictionary>();

    state_manager_->Push(std::make_unique<LoadingState>(this));
}

Viewer::~Viewer() { state_manager_ = {}; }

Ray::ILog *Viewer::ray_log() { return log_; }

void Viewer::Frame() {
    Ren::ApiContext *api_ctx = ren_ctx()->api_ctx();

#if defined(REN_VK_BACKEND)
    api_ctx->vkWaitForFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame], VK_TRUE,
                             UINT64_MAX);
    api_ctx->vkResetFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame]);

    ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
    DestroyDeferredResources(api_ctx, api_ctx->backend_frame);

    uint32_t next_image_index = 0;
    VkResult res = api_ctx->vkAcquireNextImageKHR(api_ctx->device, api_ctx->swapchain, UINT64_MAX,
                                                  api_ctx->image_avail_semaphores[api_ctx->backend_frame],
                                                  VK_NULL_HANDLE, &next_image_index);
    if (res != VK_SUCCESS) {
        ren_ctx()->log()->Error("Failed to acquire next image!");
    }

    api_ctx->active_present_image = next_image_index;

    const bool reset_result = ren_ctx()->default_descr_alloc()->Reset();
    assert(reset_result);

    ///////////////////////////////////////////////////////////////

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    api_ctx->vkBeginCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame], &begin_info);
    api_ctx->curr_cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    { // command buffer scope
        OPTICK_GPU_CONTEXT(api_ctx->draw_cmd_buf[api_ctx->backend_frame]);
        OPTICK_GPU_EVENT("Frame");

        api_ctx->vkCmdResetQueryPool(api_ctx->draw_cmd_buf[api_ctx->backend_frame],
                                     api_ctx->query_pools[api_ctx->backend_frame], 0, Ren::MaxTimestampQueries);

        { // change layout from present_src to attachment_optimal
            VkImageMemoryBarrier layout_transition_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            layout_transition_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            layout_transition_barrier.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            layout_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            layout_transition_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            layout_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            layout_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            layout_transition_barrier.image = api_ctx->present_images[next_image_index];
            VkImageSubresourceRange resource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            layout_transition_barrier.subresourceRange = resource_range;

            api_ctx->vkCmdPipelineBarrier(api_ctx->draw_cmd_buf[api_ctx->backend_frame],
                                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                          &layout_transition_barrier);
        }
#elif defined(REN_GL_BACKEND)
    // Make sure all operations have finished
    api_ctx->in_flight_fences[api_ctx->backend_frame].ClientWaitSync();
    api_ctx->in_flight_fences[api_ctx->backend_frame] = {};

    ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
#endif

        state_manager_->Draw();

#if defined(REN_VK_BACKEND)
        { // change layout from  attachment_optimal to present_src
            VkImageMemoryBarrier layout_transition_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            layout_transition_barrier.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            layout_transition_barrier.dstAccessMask = 0;
            layout_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            layout_transition_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            layout_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            layout_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            layout_transition_barrier.image = api_ctx->present_images[next_image_index];
            VkImageSubresourceRange resource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            layout_transition_barrier.subresourceRange = resource_range;

            api_ctx->vkCmdPipelineBarrier(
                api_ctx->draw_cmd_buf[api_ctx->backend_frame], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &layout_transition_barrier);
        }
    }

    api_ctx->vkEndCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame]);
    api_ctx->curr_cmd_buf = {};

    //////////////////////////////////////////////////////////////////////////////////////////////////

    Ren::ignore_optick_errors = true;

    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

    const VkSemaphore wait_semaphores[] = {api_ctx->image_avail_semaphores[api_ctx->backend_frame]};
    const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &api_ctx->render_finished_semaphores[api_ctx->backend_frame];

    res = api_ctx->vkQueueSubmit(api_ctx->graphics_queue, 1, &submit_info,
                                 api_ctx->in_flight_fences[api_ctx->backend_frame]);
    if (res != VK_SUCCESS) {
        ren_ctx()->log()->Error("Failed to submit into a queue!");
    }

    OPTICK_GPU_FLIP(&api_ctx->swapchain);
    Ren::ignore_optick_errors = false;

    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &api_ctx->render_finished_semaphores[api_ctx->backend_frame];

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &api_ctx->swapchain;

    present_info.pImageIndices = &next_image_index;

    res = api_ctx->vkQueuePresentKHR(api_ctx->present_queue, &present_info);
    if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR) {
        ren_ctx()->log()->Error("Failed to present queue!");
    }
#elif defined(REN_GL_BACKEND)
    api_ctx->in_flight_fences[api_ctx->backend_frame] = Ren::MakeFence();
#endif
    api_ctx->backend_frame = (api_ctx->backend_frame + 1) % Ren::MaxFramesInFlight;
}

void Viewer::PrepareAssets(std::string_view platform) {
    Eng::LogStdout log;

    const double t1 = Sys::GetTimeS();
    Eng::SceneManager::RegisterAsset("tei.json", "dict", HConvTEIToDict);

    Sys::ThreadPool temp_threads(std::thread::hardware_concurrency(), Sys::eThreadPriority::Low,
                                 "prepare_assets_thread");

#if !defined(__ANDROID__)
    if (platform == "all") {
        Eng::SceneManager::PrepareAssets("assets", "assets_pc", "pc", &temp_threads, &log);
        Eng::SceneManager::PrepareAssets("assets", "assets_android", "android", &temp_threads, &log);
    } else {
        std::string out_folder = "assets_";
        out_folder += platform;
        Eng::SceneManager::PrepareAssets("assets", out_folder.c_str(), platform, &temp_threads, &log);
    }
#endif
    const double t2 = Sys::GetTimeS();
    log.Info("Assets processed in %fs", t2 - t1);
}

bool Viewer::HConvTEIToDict(Eng::assets_context_t &ctx, const char *in_file, const char *out_file,
                            Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<Eng::asset_output_t> &) {
    ctx.log->Info("Prep %s", out_file);

    struct dict_link_t {
        uint32_t entries[16];
        uint32_t entries_count = 0;
    };

    struct dict_entry_t { // NOLINT
        eGramGrpPos pos;
        eGramGrpNum num;
        eGramGrpGen gen;
        Ren::String orth, pron;
        uint32_t trans_index, trans_count;
    };

    Ren::HashMap32<Ren::String, dict_link_t> dict_hashmap;
    std::vector<dict_entry_t> dict_entries;
    std::vector<std::string> translations;

    { // parse file and fill dictionary data structures
        Sys::JsObject js_root;

        { // read json file
            std::ifstream src_stream(in_file, std::ios::binary);
            if (!js_root.Read(src_stream)) {
                ctx.log->Error("Error parsing %s!", in_file);
                return false;
            }
        }

        Sys::JsObject &js_tei = js_root.at("TEI").as_obj();

        Sys::JsObject &js_text = js_tei.at("text").as_obj();
        Sys::JsObject &js_body = js_text.at("body").as_obj();
        Sys::JsArray &js_entries = js_body.at("entry").as_arr();

        int index = 0;
        for (Sys::JsElement &js_entry_el : js_entries.elements) {
            Sys::JsObject &js_entry = js_entry_el.as_obj();
            if (!js_entry.Has("sense")) {
                continue;
            }

            Sys::JsObject &js_form = js_entry.at("form").as_obj();
            Sys::JsString &js_orth = js_form.at("orth").as_str();

            dict_link_t &link = dict_hashmap[Ren::String{js_orth.val}];
            link.entries[link.entries_count++] = uint32_t(dict_entries.size());

            dict_entry_t &entry = dict_entries.emplace_back();

            entry.orth = Ren::String{js_orth.val};

            if (js_form.Has("pron")) {
                const Sys::JsString &js_pron = js_form.at("pron").as_str();
                entry.pron = Ren::String{js_pron.val};
            }

            // init defaults
            entry.pos = eGramGrpPos::Adjective;
            entry.num = eGramGrpNum::Singular;
            entry.gen = eGramGrpGen::Masculine;

            if (js_entry.Has("gramGrp")) {
                const Sys::JsObject &js_gram_grp = js_entry.at("gramGrp").as_obj();
                if (js_gram_grp.Has("pos")) {
                    const Sys::JsString &js_gram_grp_pos = js_gram_grp.at("pos").as_str();
                    if (js_gram_grp_pos.val == "n") {
                        entry.pos = eGramGrpPos::Noun;
                    } else if (js_gram_grp_pos.val == "v") {
                        entry.pos = eGramGrpPos::Verb;
                    } else if (js_gram_grp_pos.val == "a") {
                        entry.pos = eGramGrpPos::Adjective;
                    }
                }
                if (js_gram_grp.Has("num")) {
                    if (js_gram_grp.at("num").type() == Sys::JsType::String) {
                        const Sys::JsString &js_gram_grp_num = js_gram_grp.at("num").as_str();
                        if (js_gram_grp_num.val == "p") {
                            entry.num = eGramGrpNum::Plural;
                        }
                    }
                }
                if (js_gram_grp.Has("gen")) {
                    if (js_gram_grp.at("gen").type() == Sys::JsType::String) {
                        const Sys::JsString &js_gram_grp_gen = js_gram_grp.at("gen").as_str();
                        if (js_gram_grp_gen.val == "f") {
                            entry.gen = eGramGrpGen::Feminine;
                        } else if (js_gram_grp_gen.val == "n") {
                            entry.gen = eGramGrpGen::Neutral;
                        }
                    }
                }
            }

            entry.trans_index = uint32_t(translations.size());
            entry.trans_count = 0;

            if (js_entry.at("sense").type() == Sys::JsType::Object) {
                Sys::JsObject &js_sense = js_entry.at("sense").as_obj();
                Sys::JsElement &js_cit_els = js_sense.at("cit");
                if (js_cit_els.type() == Sys::JsType::Array) {
                    Sys::JsArray &js_cits = js_cit_els.as_arr();
                    for (Sys::JsElement &js_cit_el : js_cits.elements) {
                        Sys::JsObject &js_cit = js_cit_el.as_obj();
                        const Sys::JsString &js_cit_type = js_cit.at("-type").as_str();
                        if (js_cit_type.val == "trans") {
                            Sys::JsString &js_quote = js_cit.at("quote").as_str();

                            translations.emplace_back(std::move(js_quote.val));
                            entry.trans_count++;
                        }
                    }
                } else {
                    assert(js_cit_els.type() == Sys::JsType::Object);
                    Sys::JsObject &js_cit = js_cit_els.as_obj();
                    const Sys::JsString &js_cit_type = js_cit.at("-type").as_str();
                    if (js_cit_type.val == "trans") {
                        Sys::JsString &js_quote = js_cit.at("quote").as_str();

                        translations.emplace_back(std::move(js_quote.val));
                        entry.trans_count++;
                    }
                }
            } else {
                Sys::JsArray &js_senses = js_entry.at("sense").as_arr();
                for (Sys::JsElement &js_sense_el : js_senses.elements) {
                    Sys::JsObject &js_sense = js_sense_el.as_obj();
                    Sys::JsElement &js_cit_els = js_sense.at("cit");
                    if (js_cit_els.type() == Sys::JsType::Array) {
                        Sys::JsArray &js_cits = js_cit_els.as_arr();
                        for (Sys::JsElement &js_cit_el : js_cits.elements) {
                            Sys::JsObject &js_cit = js_cit_el.as_obj();
                            const Sys::JsString &js_cit_type = js_cit.at("-type").as_str();
                            if (js_cit_type.val == "trans") {
                                Sys::JsString &js_quote = js_cit.at("quote").as_str();

                                translations.emplace_back(std::move(js_quote.val));
                                entry.trans_count++;
                            }
                        }
                    } else {
                        assert(js_cit_els.type() == Sys::JsType::Object);
                        Sys::JsObject &js_cit = js_cit_els.as_obj();
                        const Sys::JsString &js_cit_type = js_cit.at("-type").as_str();
                        if (js_cit_type.val == "trans") {
                            Sys::JsString &js_quote = js_cit.at("quote").as_str();

                            translations.emplace_back(std::move(js_quote.val));
                            entry.trans_count++;
                        }
                    }
                }
            }

            index++;
        }
    }

    { // compact data structures and write output file
        size_t str_mem_req = 0;
        for (auto it = dict_hashmap.cbegin(); it < dict_hashmap.cend(); ++it) {
            str_mem_req += it->key.length() + 1;

            const dict_link_t &src_link = it->val;
            for (uint32_t i = 0; i < src_link.entries_count; i++) {
                const dict_entry_t &src_entry = dict_entries[src_link.entries[i]];

                str_mem_req += src_entry.orth.length() + 1;
                if (!src_entry.pron.empty()) {
                    str_mem_req += src_entry.pron.length() + 1;
                }
            }
        }

        // offset in buffer at which translation begins
        const size_t str_mem_trans_off = str_mem_req;

        for (const std::string &tr : translations) {
            str_mem_req += tr.length() + 1;
        }

        std::unique_ptr<char[]> comb_str_buf(new char[str_mem_req]);
        size_t comb_str_buf_ndx1 = 0, comb_str_buf_ndx2 = str_mem_trans_off;

        // Ren::HashMap32<const char *, Dictionary::dict_link_compact_t>
        // dictionary_hashmap_compact;
        std::vector<Dictionary::dict_link_compact_t> links_compact;
        std::vector<Dictionary::dict_entry_compact_t> entries_compact;

        int translations_processed = 0;
        const int translations_count = int(translations.size());

        int links_count = 0;

        for (auto it = dict_hashmap.cbegin(); it < dict_hashmap.cend(); ++it) {
            const dict_link_t &src_link = it->val;

            const size_t key_len = it->key.length();
            memcpy(&comb_str_buf[comb_str_buf_ndx1], it->key.c_str(), key_len + 1);
            const auto key_str_offset = uint32_t(comb_str_buf_ndx1);
            comb_str_buf_ndx1 += key_len + 1;

            Dictionary::dict_link_compact_t &link = links_compact.emplace_back();
            link.key_str_off = key_str_offset;
            link.entry_index = uint32_t(entries_compact.size());
            link.entry_count = src_link.entries_count;

            links_count += link.entry_count;

            for (uint32_t i = 0; i < src_link.entries_count; i++) {
                const dict_entry_t &src_entry = dict_entries[src_link.entries[i]];

                Dictionary::dict_entry_compact_t &dst_entry = entries_compact.emplace_back();

                dst_entry.pos = (uint8_t)src_entry.pos;
                dst_entry.num = (uint8_t)src_entry.num;
                dst_entry.gen = (uint8_t)src_entry.gen;

                { // correct word writing
                    const size_t len = src_entry.orth.length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx1], src_entry.orth.c_str(), len + 1);
                    dst_entry.orth_str_off = uint32_t(comb_str_buf_ndx1);
                    comb_str_buf_ndx1 += len + 1;
                }

                if (!src_entry.pron.empty()) { // word pronunciation
                    const size_t len = src_entry.pron.length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx1], src_entry.pron.c_str(), len + 1);
                    dst_entry.pron_str_off = uint32_t(comb_str_buf_ndx1);
                    comb_str_buf_ndx1 += len + 1;
                } else {
                    dst_entry.pron_str_off = 0xffffffff;
                }

                for (uint32_t j = src_entry.trans_index; j < src_entry.trans_index + src_entry.trans_count; j++) {
                    const size_t len = translations[j].length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx2], translations[j].c_str(), len + 1);

                    if (j == src_entry.trans_index) {
                        dst_entry.trans_str_off = uint32_t(comb_str_buf_ndx2);
                    }

                    comb_str_buf_ndx2 += len + 1;

                    translations_processed++;
                }
                dst_entry.trans_count = src_entry.trans_count;
            }
        }

        assert(comb_str_buf_ndx1 == str_mem_trans_off && "Translations start is not right!");
        assert(comb_str_buf_ndx2 == str_mem_req && "Buffer end is not right!");
        assert(translations_processed == translations_count && "Translations count does not match!");
        assert(links_count == dict_entries.size() && "Links count is wrong!");

        std::ofstream out_stream(out_file, std::ios::binary);
        const uint32_t header_size =
            4 + sizeof(uint32_t) + int(Dictionary::eDictChunks::DictChCount) * 3 * sizeof(uint32_t);
        uint32_t hdr_offset = 0, data_offset = header_size;

        { // File format string
            const char signature[] = {'D', 'I', 'C', 'T'};
            out_stream.write(signature, 4);
            hdr_offset += 4;
        }

        { // Header size
            out_stream.write((const char *)&header_size, sizeof(uint32_t));
            hdr_offset += sizeof(uint32_t);
        }

        { // Info data offsets
            const uint32_t info_data_chunk_id = uint32_t(Dictionary::eDictChunks::DictChInfo),
                           info_data_offset = data_offset, info_data_size = sizeof(Dictionary::dict_info_t);
            out_stream.write((const char *)&info_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&info_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&info_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += info_data_size;
        }

        { // Link data offsets
            const uint32_t link_data_chunk_id = uint32_t(Dictionary::eDictChunks::DictChLinks),
                           link_data_offset = data_offset,
                           link_data_size = uint32_t(sizeof(Dictionary::dict_link_compact_t) * links_compact.size());
            out_stream.write((const char *)&link_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&link_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&link_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += link_data_size;
        }

        { // Entry data offsets
            const uint32_t entry_data_chunk_id = uint32_t(Dictionary::eDictChunks::DictChEntries),
                           entry_data_offset = data_offset,
                           entry_data_size =
                               uint32_t(sizeof(Dictionary::dict_entry_compact_t) * entries_compact.size());
            out_stream.write((const char *)&entry_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&entry_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&entry_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += entry_data_size;
        }

        { // String data offsets
            const uint32_t string_data_chunk_id = uint32_t(Dictionary::eDictChunks::DictChStrings),
                           string_data_offset = data_offset, string_data_size = uint32_t(str_mem_req);
            out_stream.write((const char *)&string_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&string_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&string_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += string_data_size;
        }

        assert(hdr_offset == header_size);

        {                                 // Info data
            Dictionary::dict_info_t info; // NOLINT
            info.src_lang[0] = 'e';
            info.src_lang[1] = 'n';
            info.dst_lang[0] = 'd';
            info.dst_lang[1] = 'e';
            info.keys_count = uint32_t(links_compact.size());
            info.entries_count = uint32_t(entries_compact.size());

            out_stream.write((const char *)&info, sizeof(Dictionary::dict_info_t));
        }

        // Link data
        out_stream.write((const char *)links_compact.data(),
                         sizeof(Dictionary::dict_link_compact_t) * links_compact.size());

        // Entry data
        out_stream.write((const char *)entries_compact.data(),
                         sizeof(Dictionary::dict_entry_compact_t) * entries_compact.size());

        // String data
        out_stream.write(comb_str_buf.get(), str_mem_req);
    }

    return true;
}
