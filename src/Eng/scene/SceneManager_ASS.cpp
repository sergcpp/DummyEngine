#include "SceneManager.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <numeric>
#include <regex>

#include <Net/hash/Crc32.h>
#include <Net/hash/murmur.h>
// #include <Ray/internal/TextureSplitter.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../gui/Renderer.h"
#include "../gui/Utils.h"

#include <glslang/Include/glslang_c_interface.h>

namespace SceneManagerInternal {
const uint32_t AssetsBuildVersion = 19;

void LoadTGA(Sys::AssetFile &in_file, int w, int h, uint8_t *out_data) {
    auto in_file_size = (size_t)in_file.size();

    std::vector<uint8_t> in_file_data(in_file_size);
    in_file.Read((char *)&in_file_data[0], in_file_size);

    Ren::eTexFormat format;
    int _w, _h;
    std::unique_ptr<uint8_t[]> pixels = Ren::ReadTGAFile(in_file_data, _w, _h, format);

    if (_w != w || _h != h) {
        return;
    }

    if (format == Ren::eTexFormat::RawRGB888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = pixels[3ull * (y * w + x)];
                out_data[i++] = pixels[3ull * (y * w + x) + 1];
                out_data[i++] = pixels[3ull * (y * w + x) + 2];
                out_data[i++] = 255;
            }
        }
    } else if (format == Ren::eTexFormat::RawRGBA8888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = pixels[4ull * (y * w + x)];
                out_data[i++] = pixels[4ull * (y * w + x) + 1];
                out_data[i++] = pixels[4ull * (y * w + x) + 2];
                out_data[i++] = pixels[4ull * (y * w + x) + 3];
            }
        }
    } else {
        assert(false);
    }
}

std::vector<float> FlushSeams(const float *pixels, int width, int height, float invalid_threshold, int filter_size) {
    std::vector<float> temp_pixels1(pixels, pixels + 4 * width * height), temp_pixels2(4 * width * height);

    // Avoid bound checks in debug
    float *_temp_pixels1 = temp_pixels1.data(), *_temp_pixels2 = temp_pixels2.data();

    // apply dilation filter
    for (int i = 0; i < filter_size; i++) {
        bool has_invalid = false;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const float *in_p = &_temp_pixels1[4 * (y * width + x)];
                float *out_p = &_temp_pixels2[4 * (y * width + x)];

                if (in_p[3] >= invalid_threshold) {
                    const float mul = 1.0f / in_p[3];

                    out_p[0] = in_p[0] * mul;
                    out_p[1] = in_p[1] * mul;
                    out_p[2] = in_p[2] * mul;
                    out_p[3] = in_p[3] * mul;
                } else {
                    has_invalid = true;

                    float new_p[4] = {};
                    int count = 0;

                    const int _ys[] = {y - 1, y, y + 1};
                    const int _xs[] = {x - 1, x, x + 1};
                    for (const int _y : _ys) {
                        if (_y < 0 || _y > height - 1) {
                            continue;
                        }

                        for (const int _x : _xs) {
                            if ((_x == x && _y == y) || _x < 0 || _x > width - 1) {
                                continue;
                            }

                            const float *p = &_temp_pixels1[4 * (_y * width + _x)];
                            if (p[3] >= invalid_threshold) {
                                new_p[0] += p[0];
                                new_p[1] += p[1];
                                new_p[2] += p[2];
                                new_p[3] += p[3];
                                ++count;
                            }
                        }
                    }

                    const float mul = count ? (1.0f / float(count)) : 1.0f;

                    out_p[0] = new_p[0] * mul;
                    out_p[1] = new_p[1] * mul;
                    out_p[2] = new_p[2] * mul;
                    out_p[3] = new_p[3] * mul;
                }
            }
        }

        std::swap(_temp_pixels1, _temp_pixels2);
        if (!has_invalid) {
            break;
        }
    }

    return temp_pixels1;
}

void ReadAllFiles_r(Eng::assets_context_t &ctx, const std::filesystem::path &in_folder,
                    const std::function<void(Eng::assets_context_t &ctx, const std::filesystem::path &)> &callback) {
    if (!std::filesystem::exists(in_folder)) {
        // ctx.log->Error("Cannot open folder %s", in_folder.generic_string().c_str());
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(in_folder)) {
        if (std::filesystem::is_directory(entry.path())) {
            ReadAllFiles_r(ctx, entry.path(), callback);
        } else {
            callback(ctx, entry.path());
        }
    }
}

void ReadAllFiles_MT_r(Eng::assets_context_t &ctx, const std::filesystem::path &in_folder,
                       const std::function<void(Eng::assets_context_t &ctx, const std::filesystem::path &)> &callback,
                       Sys::ThreadPool *threads, std::deque<std::future<void>> &events) {
    if (!std::filesystem::exists(in_folder)) {
        // ctx.log->Error("Cannot open folder %s", in_folder.generic_string().c_str());
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(in_folder)) {
        if (std::filesystem::is_directory(entry.path())) {
            ReadAllFiles_MT_r(ctx, entry.path(), callback, threads, events);
        } else {
            events.push_back(threads->Enqueue([entry, &ctx, callback]() { callback(ctx, entry.path()); }));
        }
    }
}

uint32_t HashFile(const std::filesystem::path &in_file, Ren::ILog *log) {
    std::ifstream in_file_stream(in_file, std::ios::binary | std::ios::ate);
    if (!in_file_stream) {
        return 0;
    }
    const size_t in_file_size = size_t(in_file_stream.tellg());
    in_file_stream.seekg(0, std::ios::beg);

    const size_t HashChunkSize = 8 * 1024;
    uint8_t in_file_buf[HashChunkSize];

    // log->Info("Hashing %s", in_file.generic_string().c_str());

    uint32_t hash = 0;

    for (size_t i = 0; i < in_file_size; i += HashChunkSize) {
        const size_t portion = std::min(HashChunkSize, in_file_size - i);
        in_file_stream.read((char *)&in_file_buf[0], portion);
#if 0
        hash = crc32_fast(&in_file_buf[0], portion, hash);
#else
        hash = murmur3_32(&in_file_buf[0], portion, hash);
#endif
    }

    return hash;
}

template <typename TP> std::time_t to_time_t(TP tp) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

bool SkipAssetForCurrentBuild(const Ren::Bitmask<Eng::eAssetFlags> flags) {
#if defined(NDEBUG)
    if (flags & Eng::eAssetFlags::DebugOnly) {
        return true;
    }
#else
    if (flags & Eng::eAssetFlags::ReleaseOnly) {
        return true;
    }
#endif
#if !defined(USE_GL_RENDER)
    if (flags & Eng::eAssetFlags::GLOnly) {
        return true;
    }
#endif
#if !defined(USE_VK_RENDER)
    if (flags & Eng::eAssetFlags::VKOnly) {
        return true;
    }
#endif
    return false;
}

bool CheckAssetChanged(const std::filesystem::path &in_file, const std::filesystem::path &out_file,
                       Eng::assets_context_t &ctx) {
#if !defined(NDEBUG) && 0
    ctx.log->Info("Warning: glsl is forced to be not skipped!");
    if (in_file.extension() == ".glsl") {
        return true;
    }
#endif
    namespace fs = std::filesystem;

    if (!fs::exists(in_file)) {
        ctx.log->Error("File does not exist: %s!", in_file.generic_string().c_str());
        return false;
    }
    const time_t in_t = to_time_t(fs::last_write_time(in_file));
    const std::string in_t_str = std::to_string(in_t);

    std::lock_guard<std::mutex> _(ctx.cache_mtx);
    JsObjectP &js_files = ctx.cache->js_db["files"].as_obj();

    // const int *in_ndx = ctx.cache->db_map.Find(in_file.generic_string().c_str());
    const size_t in_ndx = js_files.IndexOf(in_file.generic_string());
    if (in_ndx < js_files.Size()) {
        bool file_not_changed = true;

        JsObjectP &js_in_file = js_files[in_ndx].second.as_obj();
        if (js_in_file.Has("time") && js_in_file.Has("outputs")) {
            const JsStringP &js_in_file_time = js_in_file.at("time").as_str();
            file_not_changed &= (strncmp(js_in_file_time.val.c_str(), in_t_str.c_str(), 32) == 0);

            const JsObjectP &js_outputs = js_in_file["outputs"].as_obj();
            for (const auto &output : js_outputs.elements) {
                const Ren::Bitmask<Eng::eAssetFlags> flags = Ren::Bitmask<Eng::eAssetFlags>{
                    uint32_t(atoi(output.second.as_obj().at("flags").as_str().val.c_str()))};
                if (SkipAssetForCurrentBuild(flags)) {
                    continue;
                }

                time_t out_t = {};
                if (fs::exists(output.first)) {
                    out_t = to_time_t(fs::last_write_time(output.first));
                } else {
                    file_not_changed = false;
                }
                const std::string out_t_str = std::to_string(out_t);

                if (!output.second.as_obj().Has("in_time") || !output.second.as_obj().Has("out_time")) {
                    file_not_changed = false;
                    continue;
                }

                const JsStringP &js_out_in_file_time = output.second.as_obj().at("in_time").as_str();
                const JsStringP &js_out_file_time = output.second.as_obj().at("out_time").as_str();

                file_not_changed &= (js_out_in_file_time.val == js_in_file_time.val);
                file_not_changed &= (strncmp(js_out_file_time.val.c_str(), out_t_str.c_str(), 32) == 0);
            }
        } else {
            file_not_changed = false;
        }

        if (!file_not_changed) {
            const uint32_t in_hash = HashFile(in_file, ctx.log);
            const std::string in_hash_str = std::to_string(in_hash);

            if (js_in_file.Has("hash") && js_in_file.Has("outputs")) {
                const JsStringP &js_in_file_hash = js_in_file.at("hash").as_str();
                if (js_in_file_hash.val == in_hash_str) {
                    JsObjectP &js_outputs = js_in_file["outputs"].as_obj();
                    for (auto &output : js_outputs.elements) {
                        const Ren::Bitmask<Eng::eAssetFlags> flags = Ren::Bitmask<Eng::eAssetFlags>{
                            uint32_t(atoi(output.second.as_obj().at("flags").as_str().val.c_str()))};
                        if (SkipAssetForCurrentBuild(flags)) {
                            continue;
                        }

                        JsObjectP &js_output = output.second.as_obj();

                        if (!js_output.Has("hash")) {
                            file_not_changed = false;
                            continue;
                        }

                        const uint32_t out_hash = HashFile(output.first, ctx.log);
                        const std::string out_hash_str = std::to_string(out_hash);

                        const JsStringP &js_out_file_hash = js_output.at("hash").as_str();
                        if (js_out_file_hash.val == out_hash_str) {
                            // write new time
                            time_t out_t = {};
                            if (fs::exists(output.first)) {
                                out_t = to_time_t(fs::last_write_time(output.first));
                            } else {
                                file_not_changed = false;
                            }

                            const std::string out_t_str = std::to_string(out_t);

                            if (!js_output.Has("time")) {
                                js_output.Insert("time", JsStringP(out_t_str, *ctx.mp_alloc));
                            } else {
                                JsStringP &js_out_file_time = js_output.at("time").as_str();
                                js_out_file_time.val = out_t_str;
                            }
                        } else {
                            file_not_changed = false;
                        }
                    }
                } else {
                    file_not_changed = false;
                }
            } else {
                file_not_changed = false;
            }

            if (!file_not_changed) {
                // store new hash and time value
                if (!js_in_file.Has("hash")) {
                    js_in_file.Insert("hash", JsStringP(in_hash_str, *ctx.mp_alloc));
                } else {
                    JsStringP &js_in_file_hash = js_in_file["hash"].as_str();
                    js_in_file_hash.val = in_hash_str;
                }

                // write new time
                if (!js_in_file.Has("time")) {
                    js_in_file.Insert("time", JsStringP(in_t_str, *ctx.mp_alloc));
                } else {
                    JsStringP &js_in_file_time = js_in_file["time"].as_str();
                    js_in_file_time.val = in_t_str;
                }
            }
        }

        bool dependencies_have_changed = false;

        if (js_in_file.Has("deps")) {
            const JsObjectP &js_deps = js_in_file.at("deps").as_obj();
            for (const auto &dep : js_deps.elements) {
                const JsObjectP &js_dep = dep.second.as_obj();
                if (!js_dep.Has("time") || !js_dep.Has("hash")) {
                    dependencies_have_changed = true;
                    break;
                }

                const JsStringP &js_dep_time = js_dep.at("time").as_str();

                if (!fs::exists(dep.first)) {
                    ctx.log->Error("File does not exist: %s!", dep.first.c_str());
                } else {
                    const auto dep_t = to_time_t(fs::last_write_time(dep.first));
                    const std::string dep_t_str = std::to_string(dep_t);

                    if (strncmp(js_dep_time.val.c_str(), dep_t_str.c_str(), 32) != 0) {
                        const uint32_t dep_hash = HashFile(dep.first, ctx.log);
                        const std::string dep_hash_str = std::to_string(dep_hash);

                        const JsStringP &js_dep_hash = js_dep.at("hash").as_str();
                        if (js_dep_hash.val != dep_hash_str) {
                            dependencies_have_changed = true;
                            break;
                        }
                    }
                }
            }
        }

        if (file_not_changed && !dependencies_have_changed) {
            // can skip
            return false;
        }

    } else {
        const uint32_t in_hash = HashFile(in_file, ctx.log);
        const std::string in_hash_str = std::to_string(in_hash);

        JsObjectP new_entry(*ctx.mp_alloc);
        new_entry.Insert("time", JsStringP(in_t_str, *ctx.mp_alloc));
        new_entry.Insert("hash", JsStringP(in_hash_str, *ctx.mp_alloc));
        js_files.Insert(in_file.generic_string(), std::move(new_entry));
    }

    return true;
}

void ReplaceTextureExtension(std::string_view platform, std::string &tex) {
    size_t n;
    if ((n = tex.find(".uncompressed")) == std::string::npos) {
        if ((n = tex.find(".tga")) != std::string::npos) {
            if (platform == "pc") {
                tex.replace(n + 1, 3, "dds");
            } else if (platform == "android") {
                tex.replace(n + 1, 3, "ktx");
            }
        } else if ((n = tex.find(".png")) != std::string::npos || (n = tex.find(".jpg")) != std::string::npos ||
                   (n = tex.find(".img")) != std::string::npos) {
            if (platform == "pc") {
                tex.replace(n + 1, 3, "dds");
            } else if (platform == "android") {
                tex.replace(n + 1, 3, "ktx");
            }
        } else if ((n = tex.find(".hdr")) != std::string::npos) {
            tex.replace(n + 1, 3, "dds");
        } else if ((n = tex.find(".tex")) != std::string::npos) {
            tex.replace(n + 1, 3, "dds");
        }
    }
}

void LoadDB(const char *out_folder, JsObjectP &out_js_assets_db) {
    const std::string file_names[] = {std::string(out_folder) + "/assets_db.json",
                                      std::string(out_folder) + "/assets_db.json1",
                                      std::string(out_folder) + "/assets_db.json2"};

    int i = 0;
    for (; i < 3; i++) {
        std::ifstream in_file(file_names[i], std::ios::binary);
        if (in_file) {
            try {
                if (out_js_assets_db.Read(in_file)) {
                    if (!out_js_assets_db.Has("version")) {
                        out_js_assets_db.elements.clear();
                    } else {
                        const JsStringP &js_assets_version = out_js_assets_db.at("version").as_str();
                        if (js_assets_version.val != std::to_string(AssetsBuildVersion)) {
                            out_js_assets_db.elements.clear();
                        }
                    }
                    break;
                } else {
                    // unsuccessful read can leave junk
                    out_js_assets_db.elements.clear();
                }
            } catch (...) {
                // unsuccessful read can leave junk
                out_js_assets_db.elements.clear();
            }
        }
    }

    if (!out_js_assets_db.Has("version")) {
        out_js_assets_db.Insert("version",
                                JsStringP{std::to_string(AssetsBuildVersion), out_js_assets_db.get_allocator()});
    } else {
        JsStringP &js_assets_version = out_js_assets_db.at("version").as_str();
        js_assets_version.val = std::to_string(AssetsBuildVersion);
    }

    if (i != 0 && !out_js_assets_db.elements.empty()) {
        // write loaded db as the most recent one
        std::ofstream out_file(file_names[0], std::ios::binary);
        try {
            out_js_assets_db.Write(out_file);
        } catch (...) {
        }
    }
}

bool WriteDB(const JsObjectP &js_db, const char *out_folder) {
    const std::string name1 = std::string(out_folder) + "/assets_db.json";
    const std::string name2 = std::string(out_folder) + "/assets_db.json1";
    const std::string name3 = std::string(out_folder) + "/assets_db.json2";
    const std::string temp = std::string(out_folder) + "/assets_db.json_temp";

    bool write_successful = false;

    try {
        std::ofstream out_file(temp, std::ios::binary);
        out_file.precision(std::numeric_limits<double>::max_digits10);
        js_db.Write(out_file);
        write_successful = out_file.good();
    } catch (...) {
    }

    if (write_successful) {
        std::remove(name3.c_str());
        std::rename(name2.c_str(), name3.c_str());
        std::rename(name1.c_str(), name2.c_str());
        std::rename(temp.c_str(), name1.c_str());
    }

    return write_successful;
}

std::string ExtractHTMLData(Eng::assets_context_t &ctx, const char *in_file, std::string &out_caption) {
    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    const int file_size = int(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    // TODO: buffered read?
    std::unique_ptr<char[]> in_buf(new char[file_size]);
    src_stream.read(&in_buf[0], file_size);

    std::string out_str;
    out_str.reserve(file_size);

    bool body_active = false, header_active = false;
    bool p_active = false;

    int buf_pos = 0;
    while (buf_pos < file_size) {
        int start_pos = buf_pos;

        uint32_t unicode;
        buf_pos += Gui::ConvChar_UTF8_to_Unicode(&in_buf[buf_pos], unicode);

        if (unicode == Gui::g_unicode_less_than) {
            char tag_str[32];
            int tag_str_len = 0;

            while (unicode != Gui::g_unicode_greater_than) {
                buf_pos += Gui::ConvChar_UTF8_to_Unicode(&in_buf[buf_pos], unicode);
                tag_str[tag_str_len++] = char(unicode);
            }
            tag_str[tag_str_len - 1] = '\0';

            if (strcmp(tag_str, "body") == 0) {
                body_active = true;
                continue;
            } else if (strcmp(tag_str, "/body") == 0) {
                body_active = false;
                continue;
            } else if (strcmp(tag_str, "header") == 0) {
                header_active = true;
                continue;
            } else if (strcmp(tag_str, "p") == 0) {
                p_active = true;
            } else if (strcmp(tag_str, "/p") == 0) {
                out_str += "</p>";
                p_active = false;
                continue;
            } else if (strcmp(tag_str, "/header") == 0) {
                header_active = false;
                continue;
            }
        }

        if (body_active) {
            if (p_active) {
                out_str.append(&in_buf[start_pos], buf_pos - start_pos);
            } else if (header_active) {
                out_caption.append(&in_buf[start_pos], buf_pos - start_pos);
            }
        }
    }

    return out_str;
}

static const char g_base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz"
                                     "0123456789+/";

bool is_base64(const uint8_t c) { return (isalnum(c) || (c == '+') || (c == '/')); }

std::vector<uint8_t> base64_decode(const std::string_view encoded_string) {
    int in_len = int(encoded_string.size());
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] =
                    char(std::find(std::begin(g_base64_chars), std::end(g_base64_chars), char_array_4[i]) -
                         std::begin(g_base64_chars));
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }
        for (j = 0; j < 4; j++) {
            char_array_4[j] = char(std::find(std::begin(g_base64_chars), std::end(g_base64_chars), char_array_4[j]) -
                                   std::begin(g_base64_chars));
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret.push_back(char_array_3[j]);
        }
    }

    return ret;
}

enum class eGLTFComponentType { Byte = 5120, UByte = 5121, Short = 5122, UShort = 5123, UInt = 5125, Float = 5126 };

bool GetTexturesAverageColor(const char *in_file, uint8_t out_color[4]);

bool g_astc_initialized = false;
} // namespace SceneManagerInternal

Ren::HashMap32<std::string, Eng::SceneManager::Handler> Eng::SceneManager::g_asset_handlers;

void Eng::SceneManager::RegisterAsset(const char *in_ext, const char *out_ext, const ConvertAssetFunc &convert_func) {
    g_asset_handlers[in_ext] = {out_ext, convert_func};
}

bool Eng::SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, std::string_view platform,
                                      Sys::ThreadPool *p_threads, Ren::ILog *log) {
    using namespace SceneManagerInternal;

    // for astc codec
    if (!g_astc_initialized) {
        InitASTCCodec();
        g_astc_initialized = true;
    }

    g_asset_handlers["bff"] = {"bff", HCopy};
    g_asset_handlers["mesh"] = {"mesh", HCopy};
    g_asset_handlers["gltf"] = {"mesh", HConvGLTFToMesh};
    g_asset_handlers["anim"] = {"anim", HCopy};
    g_asset_handlers["wav"] = {"wav", HCopy};
    g_asset_handlers["ivf"] = {"ivf", HCopy};
    // g_asset_handlers["glsl"] = {"glsl", HCopy};
    g_asset_handlers["vert.glsl"] = {"vert.glsl", HCompileShader};
    g_asset_handlers["frag.glsl"] = {"frag.glsl", HCompileShader};
    g_asset_handlers["tesc.glsl"] = {"tesc.glsl", HCompileShader};
    g_asset_handlers["tese.glsl"] = {"tese.glsl", HCompileShader};
    g_asset_handlers["comp.glsl"] = {"comp.glsl", HCompileShader};
    g_asset_handlers["geom.glsl"] = {"geom.glsl", HCompileShader};
    g_asset_handlers["ttf"] = {"font", HConvTTFToFont};
    g_asset_handlers["json"] = {"json", HPreprocessJson};

    if (platform == "pc") {
        // g_asset_handlers["hdr"] = {"dds", HConvHDRToRGBM};
        g_asset_handlers["hdr"] = {"dds", HConvHDRToDDS};
        g_asset_handlers["tex"] = {"dds", HConvToDDS};
        g_asset_handlers["img"] = {"dds", HConvImgToDDS};
        // g_asset_handlers["dds"] = {"dds", HCopy};
        g_asset_handlers["rgen.glsl"] = {"rgen.glsl", HCompileShader};
        g_asset_handlers["rint.glsl"] = {"rint.glsl", HCompileShader};
        g_asset_handlers["rchit.glsl"] = {"rchit.glsl", HCompileShader};
        g_asset_handlers["rahit.glsl"] = {"rahit.glsl", HCompileShader};
        g_asset_handlers["rmiss.glsl"] = {"rmiss.glsl", HCompileShader};
        g_asset_handlers["rcall.glsl"] = {"rcall.glsl", HCompileShader};
    } else if (platform == "android") {
        g_asset_handlers["tga"] = {"ktx", HConvToASTC};
        // g_asset_handlers["hdr"] = {"ktx", HConvHDRToRGBM};
        // g_asset_handlers["png"] = {"ktx", HConvToASTC};
        // g_asset_handlers["jpg"] = {"ktx", HConvToASTC};
        g_asset_handlers["img"] = {"ktx", HConvImgToASTC};
        g_asset_handlers["ktx"] = {"ktx", HCopy};
    }
    g_asset_handlers["mat"] = {"mat", HPreprocessMaterial};

    // auto

    g_asset_handlers["uncompressed.tga"] = {"uncompressed.tga", HCopy};
    g_asset_handlers["uncompressed.png"] = {"uncompressed.png", HCopy};

    double last_db_write = Sys::GetTimeS();
    auto convert_file = [out_folder, &last_db_write](assets_context_t &ctx, const std::filesystem::path &in_file) {
        const std::filesystem::path parent_path = in_file.parent_path();

        auto it = parent_path.begin();
        ++it;

        std::filesystem::path base_path;
        while (it != parent_path.end()) {
            base_path /= *it++;
        }

        const std::filesystem::path _ext = in_file.extension();
        if (_ext.empty()) {
            return;
        }

        const std::string filename = in_file.filename().generic_string();
        const std::string ext_str(std::find(filename.begin(), filename.end(), '.'), filename.end());
        const char *ext = ext_str.c_str() + 1;

        Handler *handler = g_asset_handlers.Find(ext);
        if (!handler) {
            // ctx.log->Info("No handler found for %s", in_file.generic_string().c_str());
            return;
        }

        std::filesystem::path out_file = out_folder / base_path / in_file.stem().stem();
        out_file += ".";
        out_file += handler->ext;

        if (!CheckAssetChanged(in_file, out_file, ctx)) {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(out_file.parent_path());
        if (ec) {
            ctx.log->Info("Failed to create directories for %s", out_file.generic_string().c_str());
            return;
        }

        std::string in_t_str;
        if (std::filesystem::exists(in_file)) {
            const auto in_t = to_time_t(std::filesystem::last_write_time(in_file));
            in_t_str = std::to_string(in_t);
        }

        // std::lock_guard<std::mutex> _(ctx.cache_mtx);

        Ren::SmallVector<std::string, 32> dependencies;
        Ren::SmallVector<asset_output_t, 32> outputs;
        const bool res = handler->convert(ctx, in_file.generic_string().c_str(), out_file.generic_string().c_str(),
                                          dependencies, outputs);
        if (res) {
            if (outputs.empty()) {
                outputs.push_back({out_file.generic_string()});
            }

            std::lock_guard<std::mutex> _(ctx.cache_mtx);
            JsObjectP &js_files = ctx.cache->js_db["files"].as_obj();

            const size_t in_ndx = js_files.IndexOf(in_file.generic_string());
            if (in_ndx < js_files.Size()) {
                JsObjectP &js_in_file = js_files[in_ndx].second.as_obj();

                // store new outputs
                if (!js_in_file.Has("outputs")) {
                    js_in_file.Insert("outputs", JsObjectP{*ctx.mp_alloc});
                }

                JsObjectP &js_outputs = js_in_file["outputs"].as_obj();
                for (const asset_output_t &out_file : outputs) {
                    std::string out_t_str = "0", out_hash_str = "0";
                    if (std::filesystem::exists(out_file.name)) {
                        const uint32_t out_hash = HashFile(out_file.name, ctx.log);
                        out_hash_str = std::to_string(out_hash);

                        const auto out_t = to_time_t(std::filesystem::last_write_time(out_file.name));
                        out_t_str = std::to_string(out_t);
                    }

                    if (!js_outputs.Has(out_file.name)) {
                        js_outputs.Insert(out_file.name, JsObjectP{*ctx.mp_alloc});
                    }

                    JsObjectP &js_output = js_outputs[out_file.name].as_obj();

                    // store new flags
                    if (!js_output.Has("flags")) {
                        js_output.Insert("flags", JsStringP(std::to_string(uint32_t(out_file.flags)), *ctx.mp_alloc));
                    } else {
                        JsStringP &js_flags = js_output["flags"].as_str();
                        js_flags.val = std::to_string(uint32_t(out_file.flags)).c_str();
                    }

                    if (SkipAssetForCurrentBuild(out_file.flags)) {
                        continue;
                    }

                    // store new hash value
                    if (!js_output.Has("hash")) {
                        js_output.Insert("hash", JsStringP(out_hash_str, *ctx.mp_alloc));
                    } else {
                        JsStringP &js_hash = js_output["hash"].as_str();
                        js_hash.val = out_hash_str.c_str();
                    }

                    // store new time values
                    if (!js_output.Has("in_time")) {
                        js_output.Insert("in_time", JsStringP(in_t_str, *ctx.mp_alloc));
                    } else {
                        JsStringP &js_time = js_output["in_time"].as_str();
                        js_time.val = in_t_str;
                    }
                    if (!js_output.Has("out_time")) {
                        js_output.Insert("out_time", JsStringP(out_t_str, *ctx.mp_alloc));
                    } else {
                        JsStringP &js_time = js_output["out_time"].as_str();
                        js_time.val = out_t_str;
                    }
                }
                if (js_outputs.elements.size() > outputs.size()) {
                    for (auto it = begin(js_outputs.elements); it != end(js_outputs.elements);) {
                        auto it2 = std::find_if(outputs.begin(), outputs.end(), [it](const asset_output_t &el) {
                            return el.name == it->first.c_str();
                        });
                        if (it2 == outputs.end()) {
                            it = js_outputs.elements.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                // store new dependencies list
                if (!js_in_file.Has("deps")) {
                    js_in_file.Insert("deps", JsObjectP{*ctx.mp_alloc});
                }

                JsObjectP &js_deps = js_in_file["deps"].as_obj();
                for (size_t i = 0; i < dependencies.size(); ++i) {
                    std::string dep_t_str;
                    if (std::filesystem::exists(dependencies[i])) {
                        const auto dep_t = to_time_t(std::filesystem::last_write_time(dependencies[i]));
                        dep_t_str = std::to_string(dep_t);
                    }

                    const uint32_t dep_hash = HashFile(dependencies[i], ctx.log);
                    const std::string dep_hash_str = std::to_string(dep_hash);

                    JsObjectP js_dep(*ctx.mp_alloc);
                    js_dep.Insert("time", JsStringP(dep_t_str, *ctx.mp_alloc));
                    js_dep.Insert("hash", JsStringP(dep_hash_str, *ctx.mp_alloc));

                    js_deps[dependencies[i]] = std::move(js_dep);
                }
                if (js_deps.elements.size() > dependencies.size()) {
                    for (auto it = begin(js_deps.elements); it != end(js_deps.elements);) {
                        auto it2 = std::find(begin(dependencies), end(dependencies), it->first.c_str());
                        if (it2 == end(dependencies)) {
                            it = js_deps.elements.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }

            if (Sys::GetTimeS() - last_db_write > 5.0 && WriteDB(ctx.cache->js_db, out_folder)) {
                last_db_write = Sys::GetTimeS();
            }
        }
    };

    Sys::MultiPoolAllocator<char> mp_alloc(32, 512);
    assets_context_t ctx = {platform, log, {}, &mp_alloc};
    ctx.cache = std::make_unique<AssetCache>(*ctx.mp_alloc);

    LoadDB(out_folder, ctx.cache->js_db);

    if (ctx.cache->js_db.Has("files")) {
        const JsObjectP &js_files = ctx.cache->js_db.at("files").as_obj();
        for (auto it = begin(js_files.elements); it != end(js_files.elements); ++it) {
            const size_t ndx = it->second.as_obj().IndexOf("color");
            if (ndx < it->second.as_obj().Size()) {
                const JsNumber &js_color = it->second.as_obj()[ndx].second.as_num();
                ctx.cache->texture_averages[it->first.c_str()] = uint32_t(js_color.val);
            }
        }
    } else {
        ctx.cache->js_db.Insert("files", JsObjectP{mp_alloc});
    }

    glslang_initialize_process();

    if (p_threads) {
        std::deque<std::future<void>> events;
        ReadAllFiles_MT_r(ctx, in_folder, convert_file, p_threads, events);

        for (std::future<void> &e : events) {
            e.wait();
        }
    } else {
        ReadAllFiles_r(ctx, in_folder, convert_file);
    }

    WriteDB(ctx.cache->js_db, out_folder);

    glslang_finalize_process();

    return true;
}

bool Eng::SceneManager::HSkip(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &) {
    ctx.log->Info("Skip %s", out_file);
    return true;
}

bool Eng::SceneManager::HCopy(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &) {
    ctx.log->Info("Copy %s", out_file);

    return std::filesystem::copy_file(in_file, out_file, std::filesystem::copy_options::overwrite_existing);
}

bool Eng::SceneManager::HConvGLTFToMesh(assets_context_t &ctx, const char *in_file, const char *out_file,
                                        Ren::SmallVectorImpl<std::string> &out_dependencies,
                                        Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return false;
    }

    JsObject js_gltf;
    if (!js_gltf.Read(src_stream)) {
        ctx.log->Error("Failed to parse %s", in_file);
        return false;
    }

    try {
        std::vector<std::vector<uint8_t>> buffers;
        const JsArray &js_buffers = js_gltf.at("buffers").as_arr();
        for (const JsElement &js_buffer : js_buffers.elements) {
            const JsObject &js_buf = js_buffer.as_obj();

            const JsNumber &js_byte_length = js_buf.at("byteLength").as_num();
            const JsString &js_uri = js_buf.at("uri").as_str();

            if (std::string_view(js_uri.val.c_str(), 5) == "data:") {
                const size_t n = js_uri.val.find(',', 5);
                if (n != std::string::npos) {
                    std::vector<uint8_t> data = base64_decode(std::string_view(js_uri.val.c_str() + n + 1));
                    data.resize(size_t(js_byte_length.val));
                    buffers.emplace_back(std::move(data));
                } else {
                    ctx.log->Error("Unsupported URI");
                    return false;
                }
            } else {
                ctx.log->Error("Unsupported URI");
                return false;
            }
        }

        std::vector<float> positions, normals, tangents, uvs, uvs2, weights;
        std::vector<std::string> materials;
        std::vector<std::vector<uint32_t>> indices;

        int tangents_stride = 3;

        const JsArray &js_materials = js_gltf.at("materials").as_arr();
        for (const JsElement &js_material : js_materials.elements) {
            const JsObject &js_mat = js_material.as_obj();
            materials.push_back(js_mat.at("name").as_str().val);
        }

        const JsArray &js_accessors = js_gltf.at("accessors").as_arr();
        const JsArray &js_buffer_views = js_gltf.at("bufferViews").as_arr();

        const JsArray &js_meshes = js_gltf.at("meshes").as_arr();
        if (js_meshes.Size() == 1) {
            const JsObject &js_mesh = js_meshes.at(0).as_obj();

            const JsArray &js_primitives = js_mesh.at("primitives").as_arr();
            for (const JsElement &js_primitive : js_primitives.elements) {
                const JsObject &js_prim = js_primitive.as_obj();

                const JsObject &js_attributes = js_prim.at("attributes").as_obj();
                const uint32_t index_offset = uint32_t(positions.size()) / 3;

                { // parse positions
                    const int pos_ndx = int(js_attributes.at("POSITION").as_num().val);
                    const JsObject &js_pos_accessor = js_accessors.at(pos_ndx).as_obj();
                    const JsObject &js_pos_view =
                        js_buffer_views.at(int(js_pos_accessor.at("bufferView").as_num().val)).as_obj();
                    assert(int(js_pos_accessor.at("componentType").as_num().val) == int(eGLTFComponentType::Float));
                    assert(js_pos_accessor.at("type").as_str().val == "VEC3");

                    const auto &pos_buf = buffers[int(js_pos_view.at("buffer").as_num().val)];
                    const int pos_byte_len = int(js_pos_view.at("byteLength").as_num().val);
                    const int pos_byte_off = int(js_pos_view.at("byteOffset").as_num().val);
                    assert((pos_byte_len % sizeof(float)) == 0);
                    const size_t pos_off = positions.size();
                    positions.resize(positions.size() + pos_byte_len / sizeof(float));
                    memcpy(&positions[pos_off], &pos_buf[pos_byte_off], pos_byte_len);
                }

                { // parse normals
                    const int norm_ndx = int(js_attributes.at("NORMAL").as_num().val);
                    const JsObject &js_norm_accessor = js_accessors.at(norm_ndx).as_obj();
                    const JsObject &js_norm_view =
                        js_buffer_views.at(int(js_norm_accessor.at("bufferView").as_num().val)).as_obj();
                    assert(int(js_norm_accessor.at("componentType").as_num().val) == int(eGLTFComponentType::Float));
                    assert(js_norm_accessor.at("type").as_str().val == "VEC3");

                    const auto &norm_buf = buffers[int(js_norm_view.at("buffer").as_num().val)];
                    const int norm_byte_len = int(js_norm_view.at("byteLength").as_num().val);
                    const int norm_byte_off = int(js_norm_view.at("byteOffset").as_num().val);
                    assert((norm_byte_len % sizeof(float)) == 0);
                    const size_t norm_off = normals.size();
                    normals.resize(normals.size() + norm_byte_len / sizeof(float));
                    memcpy(&normals[norm_off], &norm_buf[norm_byte_off], norm_byte_len);
                }

                { // parse uvs
                    const int uvs_ndx = int(js_attributes.at("TEXCOORD_0").as_num().val);
                    const JsObject &js_uvs_accessor = js_accessors.at(uvs_ndx).as_obj();
                    const JsObject &js_uvs_view =
                        js_buffer_views.at(int(js_uvs_accessor.at("bufferView").as_num().val)).as_obj();
                    assert(int(js_uvs_accessor.at("componentType").as_num().val) == int(eGLTFComponentType::Float));
                    assert(js_uvs_accessor.at("type").as_str().val == "VEC2");

                    const auto &uvs_buf = buffers[int(js_uvs_view.at("buffer").as_num().val)];
                    const int uvs_byte_len = int(js_uvs_view.at("byteLength").as_num().val);
                    const int uvs_byte_off = int(js_uvs_view.at("byteOffset").as_num().val);
                    assert((uvs_byte_len % sizeof(float)) == 0);
                    const size_t uvs_off = uvs.size();
                    uvs.resize(uvs.size() + uvs_byte_len / sizeof(float));
                    memcpy(&uvs[uvs_off], &uvs_buf[uvs_byte_off], uvs_byte_len);
                }

                if (js_attributes.Has("TANGENT")) {
                    const int tan_ndx = int(js_attributes.at("TANGENT").as_num().val);
                    const JsObject &js_tan_accessor = js_accessors.at(tan_ndx).as_obj();
                    const JsObject &js_tan_view =
                        js_buffer_views.at(int(js_tan_accessor.at("bufferView").as_num().val)).as_obj();
                    assert(int(js_tan_accessor.at("componentType").as_num().val) == int(eGLTFComponentType::Float));
                    assert(js_tan_accessor.at("type").as_str().val == "VEC4");

                    const auto &tan_buf = buffers[int(js_tan_view.at("buffer").as_num().val)];
                    const int tan_byte_len = int(js_tan_view.at("byteLength").as_num().val);
                    const int tan_byte_off = int(js_tan_view.at("byteOffset").as_num().val);
                    assert((tan_byte_len % sizeof(float)) == 0);
                    const size_t tan_off = tangents.size();
                    tangents.resize(tangents.size() + tan_byte_len / sizeof(float));
                    memcpy(&tangents[tan_off], &tan_buf[tan_byte_off], tan_byte_len);

                    tangents_stride = 4;
                }

                { // parse indices
                    const int indices_ndx = int(js_prim.at("indices").as_num().val);
                    const JsObject &js_indices_accessor = js_accessors.at(indices_ndx).as_obj();
                    const JsObject &js_indices_view =
                        js_buffer_views.at(int(js_indices_accessor.at("bufferView").as_num().val)).as_obj();
                    const eGLTFComponentType comp_type =
                        eGLTFComponentType(js_indices_accessor.at("componentType").as_num().val);
                    assert(js_indices_accessor.at("type").as_str().val == "SCALAR");

                    const auto &indices_buf = buffers[int(js_indices_view.at("buffer").as_num().val)];
                    const int indices_byte_len = int(js_indices_view.at("byteLength").as_num().val);
                    const int indices_byte_off = int(js_indices_view.at("byteOffset").as_num().val);
                    auto &ndx = indices.emplace_back();
                    if (comp_type == eGLTFComponentType::UByte) {
                        ndx.resize(indices_byte_len);
                        const uint8_t *src = &indices_buf[indices_byte_off];
                        for (uint32_t &n : ndx) {
                            n = index_offset + uint32_t(*src++);
                        }
                    } else if (comp_type == eGLTFComponentType::UShort) {
                        ndx.resize(indices_byte_len / sizeof(uint16_t));
                        const auto *src = reinterpret_cast<const uint16_t *>(&indices_buf[indices_byte_off]);
                        for (uint32_t &n : ndx) {
                            n = index_offset + uint32_t(*src++);
                        }
                    } else if (comp_type == eGLTFComponentType::UInt) {
                        ndx.resize(indices_byte_len / sizeof(uint32_t));
                        const auto *src = reinterpret_cast<const uint32_t *>(&indices_buf[indices_byte_off]);
                        for (uint32_t &n : ndx) {
                            n = index_offset + uint32_t(*src++);
                        }
                    } else {
                        ctx.log->Error("Unsupported indices format");
                        return false;
                    }
                }
            }

            int vertex_count = int(positions.size() / 3);
            uvs2.resize(2 * vertex_count, 0.0f);

            if (tangents.empty()) {
                // generate tangents
                std::vector<Ren::vertex_t> vertices(vertex_count);

                for (int i = 0; i < vertex_count; ++i) {
                    memcpy(&vertices[i].p[0], &positions[i * 3ull], sizeof(float) * 3);
                    memcpy(&vertices[i].n[0], &normals[i * 3ull], sizeof(float) * 3);
                    memset(&vertices[i].b[0], 0, sizeof(float) * 3);
                    memcpy(&vertices[i].t[0][0], &uvs[i * 2ull], sizeof(float) * 2);
                    vertices[i].t[1][0] = vertices[i].t[1][1] = 0;
                    vertices[i].index = i;
                }

                Ren::ComputeTangentBasis(vertices, &indices[0], int(indices.size()));

                tangents.resize(vertices.size() * 3);

                for (size_t i = 0; i < vertices.size(); ++i) {
                    vertices[i].b[0] = -vertices[i].b[0];
                    vertices[i].b[1] = -vertices[i].b[1];
                    vertices[i].b[2] = -vertices[i].b[2];
                    memcpy(&tangents[i * 3], &vertices[i].b[0], sizeof(float) * 3);
                }

                for (int i = vertex_count; i < int(vertices.size()); ++i) {
                    positions.push_back(vertices[i].p[0]);
                    positions.push_back(vertices[i].p[1]);
                    positions.push_back(vertices[i].p[2]);
                    normals.push_back(vertices[i].n[0]);
                    normals.push_back(vertices[i].n[1]);
                    normals.push_back(vertices[i].n[2]);
                    uvs.push_back(vertices[i].t[0][0]);
                    uvs.push_back(vertices[i].t[0][1]);
                    uvs2.push_back(vertices[i].t[1][0]);
                    uvs2.push_back(vertices[i].t[1][1]);
                }

                vertex_count = int(vertices.size());
            }

#ifdef NDEBUG
            const bool OptimizeMesh = false;
#else
            const bool OptimizeMesh = false;
#endif

            std::vector<std::vector<uint32_t>> reordered_indices;
            if (OptimizeMesh) {
                for (std::vector<uint32_t> &index_group : indices) {
                    std::vector<uint32_t> &cur_strip = reordered_indices.emplace_back();
                    cur_strip.resize(index_group.size());
                    Ren::ReorderTriangleIndices(&index_group[0], uint32_t(index_group.size()), uint32_t(vertex_count),
                                                &cur_strip[0]);
                }
            } else {
                reordered_indices = indices;
            }

            struct MeshChunk {
                uint32_t index, num_indices;
                uint32_t alpha;

                MeshChunk(uint32_t ndx, uint32_t num, uint32_t has_alpha)
                    : index(ndx), num_indices(num), alpha(has_alpha) {}
            };
            std::vector<uint32_t> total_indices;
            std::vector<MeshChunk> total_chunks;

            for (int i = 0; i < int(reordered_indices.size()); ++i) {
                total_chunks.emplace_back(uint32_t(total_indices.size()), uint32_t(reordered_indices[i].size()), 0);
                total_indices.insert(end(total_indices), begin(reordered_indices[i]), end(reordered_indices[i]));
            }

            std::ofstream out_f(out_file, std::ios::binary);
            out_f.write("STATIC_MESH\0", 12);

            struct Header {
                int32_t num_chunks;
                Ren::MeshChunkPos p[7];
            } file_header = {};
            file_header.num_chunks = 5;

            Ren::MeshFileInfo mesh_info = {};
            for (int i = 0; i < vertex_count; ++i) {
                for (int j : {0, 1, 2}) {
                    mesh_info.bbox_min[j] = fminf(mesh_info.bbox_min[j], positions[i * 3 + j]);
                    mesh_info.bbox_max[j] = fmaxf(mesh_info.bbox_max[j], positions[i * 3 + j]);
                }
            }

            const int32_t header_size = sizeof(int32_t) + file_header.num_chunks * sizeof(Ren::MeshChunkPos);
            int32_t file_offset = 12 + header_size;

            file_header.p[int(Ren::eMeshFileChunk::Info)].offset = file_offset;
            file_header.p[int(Ren::eMeshFileChunk::Info)].length = sizeof(Ren::MeshFileInfo);
            file_offset += file_header.p[int(Ren::eMeshFileChunk::Info)].length;

            file_header.p[int(Ren::eMeshFileChunk::VtxAttributes)].offset = file_offset;
            file_header.p[int(Ren::eMeshFileChunk::VtxAttributes)].length =
                int32_t(sizeof(float) * (positions.size() / 3) * 13);
            file_offset += file_header.p[int(Ren::eMeshFileChunk::VtxAttributes)].length;

            file_header.p[int(Ren::eMeshFileChunk::TriIndices)].offset = file_offset;
            file_header.p[int(Ren::eMeshFileChunk::TriIndices)].length =
                int32_t(sizeof(uint32_t) * total_indices.size());
            file_offset += file_header.p[int(Ren::eMeshFileChunk::TriIndices)].length;

            file_header.p[int(Ren::eMeshFileChunk::Materials)].offset = file_offset;
            file_header.p[int(Ren::eMeshFileChunk::Materials)].length = int32_t(64 * materials.size());
            file_offset += file_header.p[int(Ren::eMeshFileChunk::Materials)].length;

            file_header.p[int(Ren::eMeshFileChunk::TriGroups)].offset = file_offset;
            file_header.p[int(Ren::eMeshFileChunk::TriGroups)].length =
                int32_t(sizeof(MeshChunk) * total_chunks.size());
            file_offset += file_header.p[int(Ren::eMeshFileChunk::TriGroups)].length;

            out_f.write((char *)&file_header, header_size);
            out_f.write((char *)&mesh_info, sizeof(Ren::MeshFileInfo));

            for (int i = 0; i < int(positions.size()) / 3; ++i) {
                out_f.write((char *)&positions[i * 3ull], sizeof(float) * 3);
                out_f.write((char *)&normals[i * 3ull], sizeof(float) * 3);
                out_f.write((char *)&tangents[i * tangents_stride], sizeof(float) * 3);
                out_f.write((char *)&uvs[i * 2ull], sizeof(float) * 2);
                out_f.write((char *)&uvs2[i * 2ull], sizeof(float) * 2);
            }

            out_f.write((char *)&total_indices[0], sizeof(uint32_t) * total_indices.size());

            for (const std::string &str : materials) {
                std::string name = str;
                name += ".mat";
                name.resize(64);
                out_f.write(name.c_str(), 64);
            }

            out_f.write((char *)&total_chunks[0], sizeof(MeshChunk) * total_chunks.size());
        } else {
            ctx.log->Error("Multiple meshes are not supported");
            return false;
        }
    } catch (...) {
        ctx.log->Error("Failed to parse %s", in_file);
        return false;
    }

    return true;
}

bool Eng::SceneManager::HPreprocessMaterial(assets_context_t &ctx, const char *in_file, const char *out_file,
                                            Ren::SmallVectorImpl<std::string> &out_dependencies,
                                            Ren::SmallVectorImpl<asset_output_t> &) {
    ctx.log->Info("Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return false;
    }
    std::ofstream dst_stream(out_file, std::ios::binary);

    std::string line;
    while (std::getline(src_stream, line)) {
        if (!line.empty()) {
            if (line.back() == '\n') {
                line.pop_back();
            }
            if (line.back() == '\r') {
                line.pop_back();
            }

            if (line.rfind("textures:", 0) == 0) {
                dst_stream << line << "\n";
                while (std::getline(src_stream, line)) {
                    if (line.rfind("    - ", 0) != 0) {
                        break;
                    }

                    if (line.back() == '\n') {
                        line.pop_back();
                    }
                    if (line.back() == '\r') {
                        line.pop_back();
                    }

                    const size_t n2 = line.find(' ', 6);
                    const std::string tex_name = "assets/textures/" + line.substr(6, n2 - 6);

                    auto it = std::find(std::begin(out_dependencies), std::end(out_dependencies), tex_name);
                    if (it == std::end(out_dependencies)) {
                        out_dependencies.emplace_back(tex_name);
                    }

                    uint8_t average_color[4] = {0, 255, 255, 255};

                    const uint32_t *cached_color = ctx.cache->texture_averages.Find(tex_name.c_str());
                    if (cached_color) {
                        memcpy(average_color, cached_color, 4);
                    } else {
                        if (!SceneManagerInternal::GetTexturesAverageColor(tex_name.c_str(), average_color)) {
                            ctx.log->Error("Failed to get average color of %s", tex_name.c_str());
                        } else {
                            std::lock_guard<std::mutex> _(ctx.cache_mtx);
                            ctx.cache->WriteTextureAverage(tex_name.c_str(), average_color);
                        }
                    }

                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, line);

                    static char const hex_chars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                       '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
                    line += " #";
                    for (int i = 0; i < 4; i++) {
                        line += hex_chars[average_color[i] / 16];
                        line += hex_chars[average_color[i] % 16];
                    }

                    dst_stream << line << "\n";
                }
            }
        }
        dst_stream << line << "\n";
    }

    return true;
}

bool Eng::SceneManager::HPreprocessJson(assets_context_t &ctx, const char *in_file, const char *out_file,
                                        Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return false;
    }
    std::ofstream dst_stream(out_file, std::ios::binary);

    JsObject js_root;
    if (!js_root.Read(src_stream)) {
        throw std::runtime_error("Cannot load scene!");
    }

    const std::filesystem::path base_path = std::filesystem::path(in_file).parent_path();

    if (js_root.Has("objects")) {
        JsArray &js_objects = js_root.at("objects").as_arr();
        for (JsElement &js_obj_el : js_objects.elements) {
            JsObject &js_obj = js_obj_el.as_obj();

            if (js_obj.Has("decal")) {
                JsObject &js_decal = js_obj.at("decal").as_obj();
                if (js_decal.Has("diff")) {
                    JsString &js_diff_tex = js_decal.at("diff").as_str();
                    ReplaceTextureExtension(ctx.platform, js_diff_tex.val);
                }
                if (js_decal.Has("norm")) {
                    JsString &js_norm_tex = js_decal.at("norm").as_str();
                    ReplaceTextureExtension(ctx.platform, js_norm_tex.val);
                }
                if (js_decal.Has("spec")) {
                    JsString &js_spec_tex = js_decal.at("spec").as_str();
                    ReplaceTextureExtension(ctx.platform, js_spec_tex.val);
                }
                if (js_decal.Has("mask")) {
                    JsString &js_mask_tex = js_decal.at("mask").as_str();
                    ReplaceTextureExtension(ctx.platform, js_mask_tex.val);
                }
            }
        }
    }

    if (js_root.Has("probes")) {
        JsArray &js_probes = js_root.at("probes").as_arr();
        for (JsElement &js_probe_el : js_probes.elements) {
            JsObject &js_probe = js_probe_el.as_obj();

            if (js_probe.Has("faces")) {
                JsArray &js_faces = js_probe.at("faces").as_arr();
                for (JsElement &js_face_el : js_faces.elements) {
                    JsString &js_face_str = js_face_el.as_str();
                    ReplaceTextureExtension(ctx.platform, js_face_str.val);
                }
            }
        }
    }

    if (js_root.Has("chapters")) {
        JsArray &js_chapters = js_root.at("chapters").as_arr();
        for (JsElement &js_chapter_el : js_chapters.elements) {
            JsObject &js_chapter = js_chapter_el.as_obj();

            JsObject js_caption, js_text_data;

            if (js_chapter.Has("html_src")) {
                JsObject &js_html_src = js_chapter.at("html_src").as_obj();
                for (auto &js_src_pair : js_html_src.elements) {
                    const std::string &js_lang = js_src_pair.first, &js_file_path = js_src_pair.second.as_str().val;

                    const std::filesystem::path html_file_path = base_path / std::filesystem::path(js_file_path);

                    std::string caption;
                    std::string html_body = ExtractHTMLData(ctx, html_file_path.u8string().c_str(), caption);

                    caption = std::regex_replace(caption, std::regex("\n"), "");
                    caption = std::regex_replace(caption, std::regex("\r"), "");
                    caption = std::regex_replace(caption, std::regex("\'"), "&apos;");
                    caption = std::regex_replace(caption, std::regex("\""), "&quot;");
                    caption = std::regex_replace(caption, std::regex("<h1>"), "");
                    caption = std::regex_replace(caption, std::regex("</h1>"), "");

                    html_body = std::regex_replace(html_body, std::regex("\n"), "");
                    html_body = std::regex_replace(html_body, std::regex("\'"), "&apos;");
                    html_body = std::regex_replace(html_body, std::regex("\""), "&quot;");

                    // remove spaces
                    if (!caption.empty()) {
                        int n = 0;
                        while (n < (int)caption.length() && caption[n] == ' ') {
                            n++;
                        }
                        caption.erase(0, n);
                        while (caption.back() == ' ') {
                            caption.pop_back();
                        }
                    }

                    if (!html_body.empty()) {
                        int n = 0;
                        while (n < (int)html_body.length() && html_body[n] == ' ') {
                            n++;
                        }
                        html_body.erase(0, n);
                        while (html_body.back() == ' ') {
                            html_body.pop_back();
                        }
                    }

                    js_caption[js_lang] = JsString{caption};
                    js_text_data[js_lang] = JsString{html_body};
                }
            }

            js_chapter["caption"] = std::move(js_caption);
            js_chapter["text_data"] = std::move(js_text_data);
        }
    }

    if (js_root.Has("environment")) {
        JsObject &js_environment = js_root.at("environment").as_obj();
        if (js_environment.Has("env_map")) {
            JsString &js_env_map = js_environment.at("env_map").as_str();
            ReplaceTextureExtension(ctx.platform, js_env_map.val);
        }
    }

    if (js_root.Has("objects")) {
        JsArray &js_objects = js_root.at("objects").as_arr();
        for (JsElement &js_obj_el : js_objects.elements) {
            JsObject &js_obj = js_obj_el.as_obj();
            if (js_obj.Has("drawable")) {
                JsObject &js_drawable = js_obj.at("drawable").as_obj();
                if (js_drawable.Has("mesh_file")) {
                    JsString &js_mesh_file = js_drawable.at("mesh_file").as_str();
                    size_t n;
                    if ((n = js_mesh_file.val.find(".gltf")) != std::string::npos) {
                        js_mesh_file.val.replace(n + 1, 4, "mesh");
                    }
                }
            }
            if (js_obj.Has("acc_structure")) {
                JsObject &js_acc_structure = js_obj.at("acc_structure").as_obj();
                if (js_acc_structure.Has("mesh_file")) {
                    JsString &js_mesh_file = js_acc_structure.at("mesh_file").as_str();
                    size_t n;
                    if ((n = js_mesh_file.val.find(".gltf")) != std::string::npos) {
                        js_mesh_file.val.replace(n + 1, 4, "mesh");
                    }
                }
            }
        }
    }

    JsFlags flags;
    flags.use_spaces = 1;

    js_root.Write(dst_stream, flags);

    return true;
}
