#include "Viewer.h"

#include <fstream>
#include <sstream>

#include <Eng/GameStateManager.h>
#include <Eng/Log.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Ray/RendererFactory.h>
#include <Ren/Context.h>
#include <Ren/MVec.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>

#include "Gui/DebugInfoUI.h"
#include "Gui/FontStorage.h"
#include "States/GSCreate.h"

Viewer::Viewer(int w, int h, const char *local_dir) : GameBase(w, h, local_dir) {
    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    JsObject main_config;

    {
        // load config
#if defined(__ANDROID__)
        Sys::AssetFile config_file("assets/config.json", Sys::AssetFile::FileIn);
#else
        Sys::AssetFile config_file("assets_pc/config.json", Sys::AssetFile::FileIn);
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

    const JsObject &ui_settings = main_config.at("ui_settings").as_obj();

    {   // load fonts
        auto font_storage = std::make_shared<FontStorage>();
        AddComponent(UI_FONTS_KEY, font_storage);

        const JsObject &fonts = ui_settings.at("fonts").as_obj();
        for (const auto &el : fonts.elements) {
            const std::string &name = el.first;
            

#if defined(__ANDROID__)
            std::string file_name = std::string("assets/");
#else
            std::string file_name = std::string("assets_pc/");
#endif
            file_name += el.second.as_str().val;

            std::shared_ptr<Gui::BitmapFont> loaded_font = font_storage->LoadFont(name, file_name, ctx.get());
            (void)loaded_font;
        }
    }

    {   // create commnadline
        auto cmdline = std::make_shared<Cmdline>();
        AddComponent(CMDLINE_KEY, cmdline);
    }

    {   // create UI for performance debugging
        auto font_storage = GetComponent<FontStorage>(UI_FONTS_KEY);
        auto ui_root = GetComponent<Gui::BaseElement>(UI_ROOT_KEY);
        auto debug_ui = std::make_shared<DebugInfoUI>(Ren::Vec2f{ -1.0f, -1.0f }, Ren::Vec2f{ 2.0f, 2.0f }, ui_root.get(), font_storage->FindFont("main_font"));
        AddComponent(UI_DEBUG_KEY, debug_ui);
    }

    {
        auto threads = GetComponent<Sys::ThreadPool>(THREAD_POOL_KEY);
        auto renderer = std::make_shared<Renderer>(*ctx, threads);
        AddComponent(RENDERER_KEY, renderer);

        Ray::settings_t s;
        s.w = w;
        s.h = h;

        auto ray_renderer = Ray::CreateRenderer(s, Ray::RendererSSE2 | Ray::RendererAVX | Ray::RendererAVX2 /*| Ray::RendererOCL*/);
        AddComponent(RAY_RENDERER_KEY, ray_renderer);

        auto scene_manager = std::make_shared<SceneManager>(*ctx, *ray_renderer, *threads);
        AddComponent(SCENE_MANAGER_KEY, scene_manager);
    }

#if defined(__ANDROID__)
    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);
    const Ren::Context *p_ctx = ctx.get();
    input_manager->SetConverter(EvP1Move, [p_ctx](InputManager::Event &evt) {
        evt.move.dx *= 300.0f / p_ctx->w();
        evt.move.dy *= 300.0f / p_ctx->w();
    });
    input_manager->SetConverter(EvP2Move, [p_ctx](InputManager::Event &evt) {
        evt.move.dx *= 300.0f / p_ctx->w();
        evt.move.dy *= 300.0f / p_ctx->w();
    });
#endif

    auto swap_interval = std::make_shared<TimeInterval>();
    AddComponent(SWAP_TIMER_KEY, swap_interval);

    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    state_manager->Push(GSCreate(GS_DRAW_TEST, this));
}

void Viewer::Resize(int w, int h) {
    GameBase::Resize(w, h);
}

void Viewer::Frame() {
    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    state_manager->Draw(0);
}

void Viewer::PrepareAssets(const char *platform) {
    LogStdout log;

    SceneManager::RegisterAsset("tei.json", "dict", HConvTEIToDict);

#if !defined(__ANDROID__)
    if (strcmp(platform, "all") == 0) {
        SceneManager::PrepareAssets("assets", "assets_pc", "pc", nullptr, &log);
        SceneManager::PrepareAssets("assets", "assets_android", "android", nullptr, &log);
    } else {
        std::string out_folder = "assets_";
        out_folder += platform;
        SceneManager::PrepareAssets("assets", out_folder.c_str(), platform, nullptr, &log);
    }
#endif
}

void Viewer::HConvTEIToDict(assets_context_t &ctx, const char *in_file, const char *out_file) {
    ctx.log->Info("[PrepareAssets] Prep %s", out_file);

    enum eGramGrpPos {
        Noun, Verb, Adjekt
    };

    enum eGramGrpNum {
        Singular, Plural
    };

    enum eGramGrpGen {
        Masculine, Feminine, Neutral
    };

    struct dict_link_t {
        uint32_t entries[16];
        uint32_t entries_count = 0;
    };

    struct dict_entry_t {
        eGramGrpPos pos;
        eGramGrpNum num;
        eGramGrpGen gen;
        Ren::String orth, pron;
        uint32_t trans_index, trans_count;
    };

    Ren::HashMap32<Ren::String, dict_link_t> dictionary_hashmap;
    std::vector<dict_entry_t> dict_entries;
    std::vector<std::string> translations;

    {   // parse file and fill dictionary data structures
        JsObject js_root;

        {   // read json file
            std::ifstream src_stream(in_file, std::ios::binary);

            if (!js_root.Read(src_stream)) {
                ctx.log->Error("Error parsing %s!", in_file);
                return;
            }
        }

        JsObject &js_tei = js_root.at("TEI").as_obj();

        JsObject &js_text = js_tei.at("text").as_obj();
        JsObject &js_body = js_text.at("body").as_obj();
        JsArray &js_entries = js_body.at("entry").as_arr();

        int index = 0;
        for (JsElement &js_entry_el : js_entries.elements) {
            JsObject &js_entry = js_entry_el.as_obj();
            if (!js_entry.Has("sense")) continue;

            JsObject &js_form = js_entry.at("form").as_obj();
            JsString &js_orth = js_form.at("orth").as_str();

            dict_link_t &link = dictionary_hashmap[Ren::String{js_orth.val.c_str()}];
            link.entries[link.entries_count++] = (uint32_t) dict_entries.size();

            dict_entries.emplace_back();
            dict_entry_t &entry = dict_entries.back();

            entry.orth = Ren::String{js_orth.val.c_str()};

            if (js_form.Has("pron")) {
                const JsString &js_pron = js_form.at("pron").as_str();
                entry.pron = Ren::String{js_pron.val.c_str()};
            }

            // init defaults
            entry.pos = Noun;
            entry.num = Singular;
            entry.gen = Feminine;

            if (js_form.Has("gramGrp")) {
                const JsObject &js_gram_grp = js_form.at("gramGrp").as_obj();
                if (js_gram_grp.Has("pos")) {
                    const JsString &js_gram_grp_pos = js_gram_grp.at("pos").as_str();
                    if (js_gram_grp_pos.val == "n") {
                        entry.pos = Noun;
                    } else if (js_gram_grp_pos.val == "a") {
                        entry.pos = Adjekt;
                    }
                }
                if (js_gram_grp.Has("num")) {
                    const JsString &js_gram_grp_num = js_form.at("num").as_str();
                    if (js_gram_grp_num.val == "p") {
                        entry.num = Plural;
                    }
                }
                if (js_gram_grp.Has("gen")) {
                    const JsString &js_gram_grp_gen = js_form.at("gen").as_str();
                    if (js_gram_grp_gen.val == "m") {
                        entry.gen = Masculine;
                    } else if (js_gram_grp_gen.val == "n") {
                        entry.gen = Neutral;
                    }
                }
            }

            entry.trans_index = (uint32_t) translations.size();
            entry.trans_count = 0;

            if (js_entry.at("sense").type() == JS_TYPE_OBJECT) {
                JsObject &js_sense = js_entry.at("sense").as_obj();
                JsElement &js_cit_els = js_sense.at("cit");
                if (js_cit_els.type() == JS_TYPE_ARRAY) {
                    JsArray &js_cits = js_cit_els.as_arr();
                    for (JsElement &js_cit_el : js_cits.elements) {
                        JsObject &js_cit = js_cit_el.as_obj();
                        const JsString &js_cit_type = js_cit.at("-type").as_str();
                        if (js_cit_type.val == "trans") {
                            JsString &js_quote = js_cit.at("quote").as_str();

                            translations.emplace_back(std::move(js_quote.val));
                            entry.trans_count++;
                        }
                    }
                } else {
                    assert(js_cit_els.type() == JS_TYPE_OBJECT);
                    JsObject &js_cit = js_cit_els.as_obj();
                    const JsString &js_cit_type = js_cit.at("-type").as_str();
                    if (js_cit_type.val == "trans") {
                        JsString &js_quote = js_cit.at("quote").as_str();

                        translations.emplace_back(std::move(js_quote.val));
                        entry.trans_count++;
                    }
                }
            } else {
                JsArray &js_senses = js_entry.at("sense").as_arr();
                for (JsElement &js_sense_el : js_senses.elements) {
                    JsObject &js_sense = js_sense_el.as_obj();
                    JsElement &js_cit_els = js_sense.at("cit");
                    if (js_cit_els.type() == JS_TYPE_ARRAY) {
                        JsArray &js_cits = js_cit_els.as_arr();
                        for (JsElement &js_cit_el : js_cits.elements) {
                            JsObject &js_cit = js_cit_el.as_obj();
                            const JsString &js_cit_type = js_cit.at("-type").as_str();
                            if (js_cit_type.val == "trans") {
                                JsString &js_quote = js_cit.at("quote").as_str();

                                translations.emplace_back(std::move(js_quote.val));
                                entry.trans_count++;
                            }
                        }
                    } else {
                        assert(js_cit_els.type() == JS_TYPE_OBJECT);
                        JsObject &js_cit = js_cit_els.as_obj();
                        const JsString &js_cit_type = js_cit.at("-type").as_str();
                        if (js_cit_type.val == "trans") {
                            JsString &js_quote = js_cit.at("quote").as_str();

                            translations.emplace_back(std::move(js_quote.val));
                            entry.trans_count++;
                        }
                    }
                }
            }

            index++;
        }
    }

    {   // compact data structures and write output file
        size_t str_mem_req = 0;
        for (auto it = dictionary_hashmap.cbegin(); it < dictionary_hashmap.cend(); ++it) {
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

        struct dict_link_compact_t {
            uint32_t key_str_off;
            uint32_t entry_index;
            uint32_t entry_count;
        };
        static_assert(sizeof(dict_link_compact_t) == 12, "!");

        struct dict_entry_compact_t {
            uint8_t pos;
            uint8_t num;
            uint8_t gen;
            uint8_t trans_count;
            uint32_t orth_str_off;
            uint32_t pron_str_off;
            uint32_t trans_str_off;
        };
        static_assert(sizeof(dict_entry_compact_t) == 16, "!");

        std::unique_ptr<char[]> comb_str_buf(new char[str_mem_req]);
        size_t comb_str_buf_ndx1 = 0, comb_str_buf_ndx2 = str_mem_trans_off;

        Ren::HashMap32<const char *, dict_link_compact_t> dictionary_hashmap_compact;
        std::vector<dict_link_compact_t> links_compact;
        std::vector<dict_entry_compact_t> entries_compact;

        int translations_processed = 0;
        int translations_count = translations.size();

        for (auto it = dictionary_hashmap.cbegin(); it < dictionary_hashmap.cend(); ++it) {
            const dict_link_t &src_link = it->val;

            const size_t key_len = it->key.length();
            memcpy(&comb_str_buf[comb_str_buf_ndx1], it->key.c_str(), key_len + 1);
            const char *key = &comb_str_buf[comb_str_buf_ndx1];
            uint32_t key_str_offset = comb_str_buf_ndx1;
            comb_str_buf_ndx1 += key_len + 1;

            dict_link_compact_t &link = dictionary_hashmap_compact[key];
            link.key_str_off = key_str_offset;
            link.entry_index = (uint32_t) entries_compact.size();
            link.entry_count = src_link.entries_count;

            for (uint32_t i = 0; i < src_link.entries_count; i++) {
                const dict_entry_t &src_entry = dict_entries[src_link.entries[i]];

                entries_compact.emplace_back();
                dict_entry_compact_t &dst_entry = entries_compact.back();

                dst_entry.pos = src_entry.pos;
                dst_entry.num = src_entry.num;
                dst_entry.gen = src_entry.gen;

                {   // correct word writing
                    const size_t len = src_entry.orth.length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx1], src_entry.orth.c_str(), len + 1);
                    dst_entry.orth_str_off = comb_str_buf_ndx1;
                    comb_str_buf_ndx1 += len + 1;
                }

                if (!src_entry.pron.empty()) { // word pronunciation
                    const size_t len = src_entry.pron.length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx1], src_entry.pron.c_str(), len + 1);
                    dst_entry.pron_str_off = comb_str_buf_ndx1;
                    comb_str_buf_ndx1 += len + 1;
                }

                for (int j = src_entry.trans_index; j < src_entry.trans_index + src_entry.trans_count; j++) {
                    const size_t len = translations[j].length();
                    memcpy(&comb_str_buf[comb_str_buf_ndx2], translations[j].c_str(), len + 1);

                    if (j == src_entry.trans_index) {
                        dst_entry.trans_str_off = comb_str_buf_ndx2;
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

        const dict_link_compact_t *test1 = dictionary_hashmap_compact.Find("Wahrscheinlichkeit");
        const dict_entry_compact_t &entry1 = entries_compact[test1->entry_index];
        const dict_link_compact_t *test2 = dictionary_hashmap_compact.Find("ficken");
        const dict_entry_compact_t &entry2 = entries_compact[test2->entry_index];

        enum eDictChunks {
            DictChInfo,
            DictChLinks,
            DictChEntries,
            DictChStrings,
            DictChCount
        };

        struct dict_info_t {
            char src_lang[2], dst_lang[2];
            uint32_t keys_count, entries_count;
        };
        static_assert(sizeof(dict_info_t) == 12, "!");

        std::ofstream out_stream(out_file, std::ios::binary);
        const uint32_t header_size = 4 + sizeof(uint32_t) + int(DictChCount) * 3 * sizeof(uint32_t);
        uint32_t hdr_offset = 0, data_offset = header_size;

        {   // File format string
            const char signature[] = { 'D', 'I', 'C', 'T' };
            out_stream.write(signature, 4);
            hdr_offset += 4;
        }

        {   // Header size
            out_stream.write((const char *)&header_size, sizeof(uint32_t));
            hdr_offset += sizeof(uint32_t);
        }

        {   // Info data offsets
            const uint32_t
                    info_data_chunk_id = (uint32_t)DictChInfo,
                    info_data_offset = data_offset,
                    info_data_size = sizeof(dict_info_t);
            out_stream.write((const char *)&info_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&info_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&info_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += info_data_size;
        }

        {   // Link data offsets
            const uint32_t
                    link_data_chunk_id = (uint32_t)DictChLinks,
                    link_data_offset = data_offset,
                    link_data_size = sizeof(dict_link_compact_t) * links_compact.size();
            out_stream.write((const char *)&link_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&link_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&link_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += link_data_size;
        }

        {   // Entry data offsets
            const uint32_t
                    entry_data_chunk_id = (uint32_t)DictChEntries,
                    entry_data_offset = data_offset,
                    entry_data_size = sizeof(dict_entry_compact_t) * entries_compact.size();
            out_stream.write((const char *)&entry_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&entry_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&entry_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += entry_data_size;
        }

        {   // String data offsets
            const uint32_t
                    string_data_chunk_id = (uint32_t)DictChStrings,
                    string_data_offset = data_offset,
                    string_data_size = sizeof(dict_entry_compact_t) * entries_compact.size();
            out_stream.write((const char *)&string_data_chunk_id, sizeof(uint32_t));
            out_stream.write((const char *)&string_data_offset, sizeof(uint32_t));
            out_stream.write((const char *)&string_data_size, sizeof(uint32_t));
            hdr_offset += 3 * sizeof(uint32_t);
            data_offset += string_data_size;
        }

        assert(hdr_offset == header_size);

        {   // Info data
            dict_info_t info;
            info.src_lang[0] = 'e';
            info.src_lang[1] = 'n';
            info.dst_lang[0] = 'd';
            info.dst_lang[1] = 'e';
            info.keys_count = dictionary_hashmap_compact.size();
            info.entries_count = (uint32_t)entries_compact.size();

            out_stream.write((const char *)&info, sizeof(dict_info_t));
        }

        // Link data
        out_stream.write((const char *)links_compact.data(), sizeof(dict_link_compact_t) * links_compact.size());

        // Entry data
        out_stream.write((const char *)entries_compact.data(), sizeof(dict_entry_compact_t) * entries_compact.size());

        // String data
        out_stream.write(comb_str_buf.get(), str_mem_req);
    }
}

