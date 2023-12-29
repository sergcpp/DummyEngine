#include "SceneManager.h"

#include <fstream>
#include <functional>
#include <iterator>
#include <numeric>
#include <regex>

#include <dirent.h>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#undef max
#undef min

#include <Net/hash/Crc32.h>
#include <Net/hash/murmur.h>
//#include <Ray/internal/TextureSplitter.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../gui/Renderer.h"
#include "../gui/Utils.h"

#include <glslang/Include/glslang_c_interface.h>

// TODO: pass defines as a parameter
#include "../renderer/shaders/Renderer_GL_Defines.inl"

namespace SceneManagerInternal {
void LoadTGA(Sys::AssetFile &in_file, int w, int h, uint8_t *out_data) {
    auto in_file_size = (size_t)in_file.size();

    std::vector<char> in_file_data(in_file_size);
    in_file.Read(&in_file_data[0], in_file_size);

    Ren::eTexFormat format;
    int _w, _h;
    std::unique_ptr<uint8_t[]> pixels = Ren::ReadTGAFile(&in_file_data[0], _w, _h, format);

    if (_w != w || _h != h)
        return;

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

std::unique_ptr<uint8_t[]> GetTextureData(const Ren::Tex2DRef &tex_ref, const bool flip_y) {
    const Ren::Tex2DParams &params = tex_ref->params;

    std::unique_ptr<uint8_t[]> tex_data(new uint8_t[4 * params.w * params.h]);
#if defined(__ANDROID__)
    Sys::AssetFile in_file((std::string("assets/textures/") + tex_ref->name().c_str()).c_str());
    SceneManagerInternal::LoadTGA(in_file, params.w, params.h, &tex_data[0]);
#else
    tex_ref->DownloadTextureData(Ren::eTexFormat::RawRGBA8888, (void *)&tex_data[0]);
#endif

    for (int y = 0; y < params.h / 2 && flip_y; y++) {
        std::swap_ranges(&tex_data[4 * y * params.w], &tex_data[4 * (y + 1) * params.w],
                         &tex_data[4 * (params.h - y - 1) * params.w]);
    }

    return tex_data;
}

void ReadAllFiles_r(Eng::assets_context_t &ctx, const char *in_folder,
                    const std::function<void(Eng::assets_context_t &ctx, const char *)> &callback) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        ctx.log->Error("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while ((in_ent = readdir(in_dir))) {
        if (in_ent->d_type == DT_DIR || in_ent->d_type == DT_LNK) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_r(ctx, path.c_str(), callback);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            callback(ctx, path.c_str());
        }
    }

    closedir(in_dir);
}

void ReadAllFiles_MT_r(Eng::assets_context_t &ctx, const char *in_folder,
                       const std::function<void(Eng::assets_context_t &ctx, const char *)> &callback,
                       Sys::ThreadPool *threads, std::deque<std::future<void>> &events) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        ctx.log->Error("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while ((in_ent = readdir(in_dir))) {
        if (in_ent->d_type == DT_DIR || in_ent->d_type == DT_LNK) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_MT_r(ctx, path.c_str(), callback, threads, events);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            if (events.size() > 64) {
                events.front().wait();
                events.pop_front();
            }

            events.push_back(threads->Enqueue([path, &ctx, callback]() { callback(ctx, path.c_str()); }));
        }
    }

    closedir(in_dir);
}

uint32_t HashFile(const char *in_file, Ren::ILog *log) {
    std::ifstream in_file_stream(in_file, std::ios::binary | std::ios::ate);
    if (!in_file_stream) {
        return 0;
    }
    const size_t in_file_size = size_t(in_file_stream.tellg());
    in_file_stream.seekg(0, std::ios::beg);

    const size_t HashChunkSize = 8 * 1024;
    uint8_t in_file_buf[HashChunkSize];

    log->Info("[PrepareAssets] Hashing %s", in_file);

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

bool GetFileModifyTime(const char *in_file, char out_str[32], Eng::assets_context_t &ctx, bool report_error) {
#ifdef _WIN32
    auto filetime_to_uint64 = [](const FILETIME &ft) -> uint64_t {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        return ull.QuadPart;
    };

    HANDLE in_h = CreateFile(in_file, 0, 0, NULL, OPEN_EXISTING, NULL, NULL);
    if (in_h == INVALID_HANDLE_VALUE) {
        if (report_error) {
            ctx.log->Info("[PrepareAssets] Failed to open file %s", in_file);
        }
        CloseHandle(in_h);
        return false;
    }

    FILETIME in_t;
    GetFileTime(in_h, NULL, NULL, &in_t);
    CloseHandle(in_h);

    const uint64_t val = filetime_to_uint64(in_t);
#else
    struct stat st1 = {};
    const int res1 = stat(in_file, &st1);
    if (res1 == -1) {
        if (report_error) {
            ctx.log->Info("[PrepareAssets] Failed to open input file %s!", in_file);
        }
        return {};
    }

    const auto val = (unsigned long long)st1.st_ctime;
#endif

    sprintf(out_str, "%llu", val);

    return true;
}

bool CheckAssetChanged(const char *in_file, const char *out_file, Eng::assets_context_t &ctx) {
#if !defined(NDEBUG) && 0
    log->Info("Warning: glsl is forced to be not skipped!");
    if (strstr(in_file, ".glsl")) {
        return true;
    }
#endif

    if (strstr(in_file, "Antenna_Metal_diff")) {
        volatile int ii = 0;
    }

    char in_t[32] = "", out_t[32] = "";
    GetFileModifyTime(in_file, in_t, ctx, true /* report_error */);
    GetFileModifyTime(out_file, out_t, ctx, false /* report_error */);

    std::lock_guard<std::mutex> _(ctx.cache_mtx);
    JsObjectP &js_files = ctx.cache->js_db["files"].as_obj();

    const int *in_ndx = ctx.cache->db_map.Find(in_file);
    if (in_ndx) {
        bool file_has_changed = true;

        JsObjectP &js_in_file = js_files[*in_ndx].second.as_obj();
        if (js_in_file.Has("in_time") && js_in_file.Has("out_time")) {
            const JsStringP &js_in_file_time = js_in_file.at("in_time").as_str();
            const JsStringP &js_out_file_time = js_in_file.at("out_time").as_str();
            file_has_changed = (strncmp(js_in_file_time.val.c_str(), in_t, 32) != 0 ||
                                strncmp(js_out_file_time.val.c_str(), out_t, 32) != 0);
        }

        if (file_has_changed) {
            const uint32_t in_hash = HashFile(in_file, ctx.log);
            const std::string in_hash_str = std::to_string(in_hash);

            if (js_in_file.Has("in_hash") && js_in_file.Has("out_hash")) {
                const JsStringP &js_in_file_hash = js_in_file.at("in_hash").as_str();
                if (js_in_file_hash.val == in_hash_str) {
                    const uint32_t out_hash = HashFile(out_file, ctx.log);
                    const std::string out_hash_str = std::to_string(out_hash);

                    const JsStringP &js_out_file_hash = js_in_file.at("out_hash").as_str();
                    if (js_out_file_hash.val == out_hash_str) {
                        // write new time
                        if (!js_in_file.Has("in_time")) {
                            js_in_file.Push("in_time", JsStringP(in_t, *ctx.mp_alloc));
                        } else {
                            JsStringP &js_in_file_time = js_in_file["in_time"].as_str();
                            js_in_file_time.val = in_t;
                        }

                        if (!js_in_file.Has("out_time")) {
                            js_in_file.Push("out_time", JsStringP(out_t, *ctx.mp_alloc));
                        } else {
                            JsStringP &js_out_file_time = js_in_file["out_time"].as_str();
                            js_out_file_time.val = out_t;
                        }

                        // can skip
                        file_has_changed = false;
                    }
                }
            }

            if (file_has_changed) {
                // store new hash and time value
                if (!js_in_file.Has("in_hash")) {
                    js_in_file.Push("in_hash", JsStringP(in_hash_str.c_str(), *ctx.mp_alloc));
                } else {
                    JsStringP &js_in_file_hash = js_in_file["in_hash"].as_str();
                    js_in_file_hash.val = in_hash_str.c_str();
                }

                // write new time
                if (!js_in_file.Has("in_time")) {
                    js_in_file.Push("in_time", JsStringP(in_t, *ctx.mp_alloc));
                } else {
                    JsStringP &js_in_file_time = js_in_file["in_time"].as_str();
                    js_in_file_time.val = in_t;
                }
            }
        }

        bool dependencies_have_changed = false;

        if (js_in_file.Has("deps")) {
            const JsObjectP &js_deps = js_in_file.at("deps").as_obj();
            for (const auto &dep : js_deps.elements) {
                const JsObjectP &js_dep = dep.second.as_obj();
                if (!js_dep.Has("in_time") || !js_dep.Has("in_hash")) {
                    dependencies_have_changed = true;
                    break;
                }

                const JsStringP &js_dep_time = js_dep.at("in_time").as_str();

                char dep_t[32] = "";
                GetFileModifyTime(dep.first.c_str(), dep_t, ctx, true /* report_error */);

                if (strncmp(js_dep_time.val.c_str(), dep_t, 32) != 0) {
                    const uint32_t dep_hash = HashFile(dep.first.c_str(), ctx.log);
                    const std::string dep_hash_str = std::to_string(dep_hash);

                    const JsStringP &js_dep_hash = js_dep.at("in_hash").as_str();
                    if (js_dep_hash.val != dep_hash_str) {
                        dependencies_have_changed = true;
                        break;
                    }
                }
            }
        }

        if (!file_has_changed && !dependencies_have_changed) {
            // can skip
            return false;
        }

    } else {
        const uint32_t in_hash = HashFile(in_file, ctx.log);
        const std::string in_hash_str = std::to_string(in_hash);

        JsObjectP new_entry(*ctx.mp_alloc);
        new_entry.Push("in_time", JsStringP(in_t, *ctx.mp_alloc));
        new_entry.Push("in_hash", JsStringP(in_hash_str.c_str(), *ctx.mp_alloc));
        const size_t new_ndx = js_files.Push(in_file, std::move(new_entry));
        ctx.cache->db_map[in_file] = int(new_ndx);
    }

    return true;
}

bool CreateFolders(const char *out_file, Ren::ILog *log) {
    const char *end = strchr(out_file, '/');
    while (end) {
        char folder[256] = {};
        strncpy(folder, out_file, end - out_file + 1);
#ifdef _WIN32
        if (!CreateDirectory(folder, NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                log->Error("[PrepareAssets] Failed to create directory!");
                return false;
            }
        }
#else
        struct stat st = {};
        if (stat(folder, &st) == -1) {
            if (mkdir(folder, 0777) != 0) {
                log->Error("[PrepareAssets] Failed to create directory!");
                return false;
            }
        }
#endif
        end = strchr(end + 1, '/');
    }
    return true;
}

void ReplaceTextureExtension(const char *platform, std::string &tex) {
    size_t n;
    if ((n = tex.find(".uncompressed")) == std::string::npos) {
        if ((n = tex.find(".tga")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, 3, "ktx");
            }
        } else if ((n = tex.find(".png")) != std::string::npos || (n = tex.find(".img")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, 3, "ktx");
            }
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

bool GetTexturesAverageColor(const char *in_file, uint8_t out_color[4]);

bool g_astc_initialized = false;
} // namespace SceneManagerInternal

Ren::HashMap32<std::string, Eng::SceneManager::Handler> Eng::SceneManager::g_asset_handlers;

void Eng::SceneManager::RegisterAsset(const char *in_ext, const char *out_ext, const ConvertAssetFunc &convert_func) {
    g_asset_handlers[in_ext] = {out_ext, convert_func};
}

bool Eng::SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, const char *platform,
                                      Sys::ThreadPool *p_threads, Ren::ILog *log) {
    using namespace SceneManagerInternal;

    // for astc codec
    if (!g_astc_initialized) {
        InitASTCCodec();
        g_astc_initialized = true;
    }

    g_asset_handlers["bff"] = {"bff", HCopy};
    g_asset_handlers["mesh"] = {"mesh", HCopy};
    g_asset_handlers["anim"] = {"anim", HCopy};
    g_asset_handlers["wav"] = {"wav", HCopy};
    g_asset_handlers["ivf"] = {"ivf", HCopy};
    g_asset_handlers["glsl"] = {"glsl", HCopy};
    g_asset_handlers["vert.glsl"] = {"vert.glsl", HPreprocessShader};
    g_asset_handlers["frag.glsl"] = {"frag.glsl", HPreprocessShader};
    g_asset_handlers["tesc.glsl"] = {"tesc.glsl", HPreprocessShader};
    g_asset_handlers["tese.glsl"] = {"tese.glsl", HPreprocessShader};
    g_asset_handlers["comp.glsl"] = {"comp.glsl", HPreprocessShader};
    g_asset_handlers["ttf"] = {"font", HConvTTFToFont};
    g_asset_handlers["json"] = {"json", HPreprocessJson};

    if (strcmp(platform, "pc") == 0) {
        g_asset_handlers["tga"] = {"dds", HConvToDDS};
        g_asset_handlers["hdr"] = {"dds", HConvHDRToRGBM};
        g_asset_handlers["png"] = {"dds", HConvToDDS};
        g_asset_handlers["img"] = {"dds", HConvImgToDDS};
        g_asset_handlers["dds"] = {"dds", HCopy};
        g_asset_handlers["rgen.glsl"] = {"rgen.glsl", HPreprocessShader};
        g_asset_handlers["rint.glsl"] = {"rint.glsl", HPreprocessShader};
        g_asset_handlers["rchit.glsl"] = {"rchit.glsl", HPreprocessShader};
        g_asset_handlers["rahit.glsl"] = {"rahit.glsl", HPreprocessShader};
        g_asset_handlers["rmiss.glsl"] = {"rmiss.glsl", HPreprocessShader};
        g_asset_handlers["rcall.glsl"] = {"rcall.glsl", HPreprocessShader};
    } else if (strcmp(platform, "android") == 0) {
        g_asset_handlers["tga"] = {"ktx", HConvToASTC};
        g_asset_handlers["hdr"] = {"ktx", HConvHDRToRGBM};
        g_asset_handlers["png"] = {"ktx", HConvToASTC};
        g_asset_handlers["img"] = {"ktx", HConvImgToASTC};
        g_asset_handlers["ktx"] = {"ktx", HCopy};
    }
    g_asset_handlers["txt"] = {"txt", HPreprocessMaterial};

    // auto

    g_asset_handlers["uncompressed.tga"] = {"uncompressed.tga", HCopy};
    g_asset_handlers["uncompressed.png"] = {"uncompressed.png", HCopy};

    double last_db_write = Sys::GetTimeS();
    auto convert_file = [out_folder, &last_db_write](assets_context_t &ctx, const char *in_file) {
        const char *base_path = strchr(in_file, '/');
        if (!base_path) {
            return;
        }
        const char *ext = strchr(in_file, '.');
        if (!ext) {
            return;
        }

        ext++;

        Handler *handler = g_asset_handlers.Find(ext);
        if (!handler) {
            ctx.log->Info("[PrepareAssets] No handler found for %s", in_file);
            return;
        }

        const std::string out_file =
            out_folder + std::string(base_path, strlen(base_path) - strlen(ext)) + handler->ext;

        if (!CheckAssetChanged(in_file, out_file.c_str(), ctx)) {
            return;
        }

        if (!CreateFolders(out_file.c_str(), ctx.log)) {
            ctx.log->Info("[PrepareAssets] Failed to create directories for %s", out_file.c_str());
            return;
        }

        // std::lock_guard<std::mutex> _(ctx.cache_mtx);

        Ren::SmallVector<std::string, 32> dependencies;
        const bool res = handler->convert(ctx, in_file, out_file.c_str(), dependencies);
        if (res) {
            const uint32_t out_hash = HashFile(out_file.c_str(), ctx.log);
            const std::string out_hash_str = std::to_string(out_hash);

            std::lock_guard<std::mutex> _(ctx.cache_mtx);
            JsObjectP &js_files = ctx.cache->js_db["files"].as_obj();

            const int *in_ndx = ctx.cache->db_map.Find(in_file);
            if (in_ndx) {
                JsObjectP &js_in_file = js_files[*in_ndx].second.as_obj();
                // store new hash value
                if (!js_in_file.Has("out_hash")) {
                    js_in_file.Push("out_hash", JsStringP(out_hash_str.c_str(), *ctx.mp_alloc));
                } else {
                    JsStringP &js_out_file_hash = js_in_file["out_hash"].as_str();
                    js_out_file_hash.val = out_hash_str.c_str();
                }

                char out_t[32];
                GetFileModifyTime(out_file.c_str(), out_t, ctx, true /* report_error */);

                // store new time value
                if (!js_in_file.Has("out_time")) {
                    js_in_file.Push("out_time", JsStringP(out_t, *ctx.mp_alloc));
                } else {
                    JsStringP &js_out_file_time = js_in_file["out_time"].as_str();
                    js_out_file_time.val = out_t;
                }

                // store new dependencies list
                if (!dependencies.empty()) {
                    size_t ndx;
                    if ((ndx = js_in_file.IndexOf("deps")) == js_in_file.Size()) {
                        js_in_file.Push("deps", JsObjectP{*ctx.mp_alloc});
                    }

                    JsObjectP &js_deps = js_in_file.elements[ndx].second.as_obj();
                    for (size_t i = 0; i < dependencies.size(); ++i) {
                        char dep_t[32];
                        GetFileModifyTime(dependencies[i].c_str(), dep_t, ctx, true /* report_error */);

                        const uint32_t dep_hash = HashFile(dependencies[i].c_str(), ctx.log);
                        const std::string dep_hash_str = std::to_string(out_hash);

                        JsObjectP js_dep(*ctx.mp_alloc);
                        js_dep.Push("in_time", JsStringP(dep_t, *ctx.mp_alloc));
                        js_dep.Push("in_hash", JsStringP(dep_hash_str.c_str(), *ctx.mp_alloc));

                        if (i < js_deps.Size()) {
                            js_deps[i].first = dependencies[i].c_str();
                            js_deps[i].second = std::move(js_dep);
                        } else {
                            js_deps.Push(dependencies[i].c_str(), std::move(js_dep));
                        }
                    }
                    if (js_deps.elements.size() > dependencies.size()) {
                        js_deps.elements.erase(js_deps.elements.begin() + dependencies.size(),
                                               js_deps.elements.begin() + js_deps.elements.size());
                    }
                } else {
                    size_t ndx;
                    if ((ndx = js_in_file.IndexOf("deps")) < js_in_file.Size()) {
                        js_in_file.elements.erase(js_in_file.elements.begin() + ndx);
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
        for (int i = 0; i < int(js_files.elements.size()); i++) {
            const char *key = js_files.elements[i].first.c_str();
            ctx.cache->db_map[key] = i;

            if (js_files.elements[i].second.as_obj().Has("color")) {
                const JsNumber &js_color = js_files.elements[i].second.as_obj().at("color").as_num();
                ctx.cache->texture_averages[key] = uint32_t(js_color.val);
            }
        }
    } else {
        ctx.cache->js_db.Push("files", JsObjectP{mp_alloc});
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
                              Ren::SmallVectorImpl<std::string> &) {
    ctx.log->Info("[PrepareAssets] Skip %s", out_file);
    return true;
}

bool Eng::SceneManager::HCopy(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &) {
    ctx.log->Info("[PrepareAssets] Copy %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return false;
    }
    std::ofstream dst_stream(out_file, std::ios::binary);

    const int BufSize = 64 * 1024;
    char buf[BufSize];

    while (src_stream) {
        src_stream.read(buf, BufSize);
        dst_stream.write(buf, src_stream.gcount());
    }

    return dst_stream.good();
}

bool Eng::SceneManager::HPreprocessMaterial(assets_context_t &ctx, const char *in_file, const char *out_file,
                                            Ren::SmallVectorImpl<std::string> &out_dependencies) {
    ctx.log->Info("[PrepareAssets] Prep %s", out_file);

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

            if (line.rfind("texture:", 0) == 0) {
                const size_t n1 = line.find(' ');
                const size_t n2 = line.find(' ', n1 + 1);
                const std::string tex_name = "assets/textures/" + line.substr(n1 + 1, n2 - n1 - 1);

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
                        ctx.log->Error("[PrepareAssets] Failed to get average color of %s", tex_name.c_str());
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
            }
        }
        dst_stream << line << "\n";
    }

    return true;
}

bool Eng::SceneManager::HPreprocessJson(assets_context_t &ctx, const char *in_file, const char *out_file,
                                        Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return false;
    }
    std::ofstream dst_stream(out_file, std::ios::binary);

    JsObject js_root;
    if (!js_root.Read(src_stream)) {
        throw std::runtime_error("Cannot load scene!");
    }

    std::string base_path = in_file;
    { // extract base part of file path
        const size_t n = base_path.find_last_of('/');
        if (n != std::string::npos) {
            base_path = base_path.substr(0, n + 1);
        }
    }

    if (js_root.Has("objects")) {
        JsArray &js_objects = js_root.at("objects").as_arr();
        for (JsElement &js_obj_el : js_objects.elements) {
            JsObject &js_obj = js_obj_el.as_obj();

            if (js_obj.Has("decal")) {
                JsObject &js_decal = js_obj.at("decal").as_obj();
                if (js_decal.Has("diff")) {
                    JsString &js_diff_tex = js_decal.at("diff").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_diff_tex.val);
                }
                if (js_decal.Has("norm")) {
                    JsString &js_norm_tex = js_decal.at("norm").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_norm_tex.val);
                }
                if (js_decal.Has("spec")) {
                    JsString &js_spec_tex = js_decal.at("spec").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_spec_tex.val);
                }
                if (js_decal.Has("mask")) {
                    JsString &js_mask_tex = js_decal.at("mask").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_mask_tex.val);
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

                    const std::string html_file_path = base_path + js_file_path;

                    std::string caption;
                    std::string html_body = ExtractHTMLData(ctx, html_file_path.c_str(), caption);

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

    JsFlags flags;
    flags.use_spaces = 1;

    js_root.Write(dst_stream, flags);

    return true;
}
