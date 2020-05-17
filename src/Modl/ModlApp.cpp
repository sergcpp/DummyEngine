#include "ModlApp.h"

#include <cmath>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

#include <SDL2/SDL.h>

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)
#include <Ren/SW/SW.h>
#endif
#include <Ren/Mesh.h>
#include <Ren/RenderThread.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/AssetFileIO.h>
#include <Sys/DynLib.h>
#include <Sys/Log.h>

#include "../Eng/Utils/BVHSplit.cpp"

#pragma warning(disable : 4996)

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

namespace {
// slow, but ok for this task
int Tokenize(const std::string &str, const char *delims, std::string out_toks[32]) {
    const char *p = str.c_str();
    const char *q = strpbrk(p + 1, delims);

    int tok_count = 0;

    while (strchr(delims, (int)*p)) {
        p++;
    }

    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }

        out_toks[tok_count++].assign(p, q);

        if (!q)
            break;
        p = q + 1;
    }

    if (p[0]) {
        out_toks[tok_count++].assign(p);
    }
    return tok_count;
}

float RadicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

Ren::Vec2f Hammersley2D(int i, int N) {
    return Ren::Vec2f{float(i) / float(N), RadicalInverse_VdC((uint32_t)i)};
}

const auto center = Ren::Vec3f{-2.0f, 2.0f, 4.0f};
const auto target = Ren::Vec3f{0.0f, 0.0f, 0.0f};
const auto up = Ren::Vec3f{0.0f, 1.0f, 0.0f};
} // namespace

ModlApp::ModlApp() : quit_(false), cam_(center, target, up) {}

int ModlApp::Run(const std::vector<std::string> &args) {
    using namespace std;
    using namespace std::placeholders;

    enum class eInputFileType { None, Mesh, Anim } in_file_type = eInputFileType::None;
    string in_file_name, out_file_name, view_file_name, anim_file_name;

    if (args.size() < 2) {
        PrintUsage();
        return 0;
    }

    // enable vertex cache optimization (reordering of triangles)
    bool optimize_mesh = true;
    // enable generating of self-occlusion information
    bool generate_occlusion = false;

    for (unsigned i = 1; i < args.size(); i++) {
        if (args[i] == "-i") {
            in_file_name = args[++i];
        } else if (args[i] == "-o") {
            out_file_name = args[++i];
        } else if (args[i] == "-v") {
            view_file_name = args[++i];
        } else if (args[i] == "-a") {
            anim_file_name = args[++i];
        } else if (args[i] == "-noopt") {
            optimize_mesh = false;
        } else if (args[i] == "-genocc") {
            generate_occlusion = true;
        }
    }

    const int w = 1024, h = 576;

    if (Init(w, h) < 0) {
        return -1;
    }

    // Setup camera
    cam_.Perspective(45.0f, float(w) / h, 0.05f, 10000.0f);

    // Compile model or anim and/or load from file

    if (!in_file_name.empty()) {
        ifstream in_file(in_file_name, ios::binary);
        string line;
        getline(in_file, line);
        if (line == "ANIM_SEQUENCE") {
            in_file_type = eInputFileType::Anim;
        } else {
            in_file_type = eInputFileType::Mesh;
        }
    }

    if (out_file_name == "$") {
        size_t p = in_file_name.find_last_of('.');
        if (p != string::npos) {
            out_file_name = in_file_name.substr(0, p) +
                            ((in_file_type == eInputFileType::Mesh) ? ".mesh" : ".anim");
        }
    }

    if (view_file_name == "$") {
        view_file_name = out_file_name;
    }

#ifdef WIN32
    int res = system("DummyApp.exe --prepare_assets pc --norun");
#else
    int res = system("./DummyApp --prepare_assets pc --norun");
#endif
    if (res == -1) {
        std::cerr << "Failed to update assets" << std::endl;
    }

    if (in_file_type == eInputFileType::Anim) {
        const eCompileResult comp_res = CompileAnim(in_file_name, out_file_name);
        if (comp_res != eCompileResult::RES_SUCCESS) {
            return -1;
        }
    } else if (in_file_type == eInputFileType::Mesh) {
        const eCompileResult comp_res =
            CompileModel(in_file_name, out_file_name, optimize_mesh, generate_occlusion);
        if (comp_res != eCompileResult::RES_SUCCESS) {
            return -1;
        }
    }

    if (!view_file_name.empty()) { // load mesh file
        SDL_ShowWindow(window_);

        ifstream mesh_file(view_file_name, ios::binary);
        if (mesh_file) {
            Ren::eMeshLoadStatus load_status;
            view_mesh_ = ctx_.LoadMesh(out_file_name.c_str(), &mesh_file,
                                       std::bind(&ModlApp::OnMaterialNeeded, this, _1),
                                       &load_status);
            assert(load_status == Ren::MeshCreatedFromData);

            Ren::Vec3f bbox_min = view_mesh_->bbox_min(),
                       bbox_max = view_mesh_->bbox_max();
            Ren::Vec3f dims = bbox_max - bbox_min;
            float max_dim = std::max(dims[0], std::max(dims[1], dims[2]));
            view_dist_ = 2.0f * max_dim;

            if (!anim_file_name.empty()) {
                ifstream anim_file(anim_file_name, ios::binary);
                if (anim_file) {
                    Ren::AnimSeqRef anim_ref =
                        ctx_.LoadAnimSequence(anim_file_name.c_str(), anim_file);

                    Ren::Mesh *m = view_mesh_.get();
                    m->skel()->AddAnimSequence(anim_ref);
                }
            }
        }
    } else {
        return 0;
    }

    while (!terminated()) {
        PollEvents();
        Frame();

#if defined(USE_GL_RENDER)
        SDL_GL_SwapWindow(window_);
#elif defined(USE_SW_RENDER)
        const void *pixels = swGetPixelDataRef(swGetCurFramebuffer());

        SDL_UpdateTexture(texture_, NULL, pixels, w * sizeof(Uint32));
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, NULL, NULL);
        SDL_RenderPresent(renderer_);
#endif
    }

    Destroy();

    return 0;
}

int ModlApp::Init(int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL" << std::endl;
        return -1;
    }

#if defined(USE_GL_RENDER)
    // This needs to be done before window creation
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

    window_ = SDL_CreateWindow("View", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               w, h, SDL_WINDOW_OPENGL);

#if defined(USE_GL_RENDER)
    gl_ctx_ = SDL_GL_CreateContext(window_);
    SDL_GL_SetSwapInterval(1);
#endif

    ctx_.Init(w, h, &log_);
    InitInternal();

    Sys::InitWorker();

#if defined(USE_GL_RENDER)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
#elif defined(USE_SW_RENDER)
    swEnable(SW_DEPTH_TEST);
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
#endif

    { // create checker texture
        const int checker_res = 512;
        std::vector<uint8_t> checker_data(512 * 512 * 3);

        for (int y = 0; y < checker_res; y++) {
            for (int x = 0; x < checker_res; x++) {
                if ((x + y) % 2 == 1) {
                    checker_data[3 * (y * checker_res + x) + 0] = 255;
                    checker_data[3 * (y * checker_res + x) + 1] = 255;
                    checker_data[3 * (y * checker_res + x) + 2] = 255;
                } else {
                    checker_data[3 * (y * checker_res + x) + 0] = 0;
                    checker_data[3 * (y * checker_res + x) + 1] = 0;
                    checker_data[3 * (y * checker_res + x) + 2] = 0;
                }
            }
        }

        Ren::Texture2DParams p;
        p.w = p.h = checker_res;
        p.format = Ren::eTexFormat::RawRGB888;
        p.filter = Ren::eTexFilter::NoFilter;
        p.repeat = Ren::eTexRepeat::Repeat;

        checker_tex_ = ctx_.LoadTexture2D("__diag_checker", &checker_data[0],
                                          (int)checker_data.size(), p, nullptr);
    }

    return 0;
}

void ModlApp::Frame() {
    static unsigned t1 = SDL_GetTicks();
    unsigned t2 = SDL_GetTicks();
    unsigned dt_ms = t2 - t1;
    t1 = t2;

    ClearColorAndDepth(0.1f, 0.75f, 0.75f, 1);

    { // Update camera position
        Ren::Vec3f center = 0.5f * (view_mesh_->bbox_min() + view_mesh_->bbox_max());
        cam_.SetupView(center - Ren::Vec3f{0.0f, 0.0f, 1.0f} * view_dist_, center, up);
    }

    if (view_mesh_->type() == Ren::MeshSimple) {
        DrawMeshSimple(view_mesh_);
    } else if (view_mesh_->type() == Ren::MeshColored) {
        DrawMeshColored(view_mesh_);
    } else if (view_mesh_->type() == Ren::MeshSkeletal) {
        float dt_s = 0.001f * dt_ms;
        DrawMeshSkeletal(view_mesh_, dt_s);
    }

    ctx_.ProcessTasks();
}

void ModlApp::PollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_KEYDOWN: {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                quit_ = true;
                return;
            } else if (e.key.keysym.sym >= SDLK_0 && e.key.keysym.sym <= SDLK_9) {
                view_mode_ = eViewMode(e.key.keysym.sym - SDLK_0);
            } else if (e.key.keysym.sym == SDLK_r) {
                angle_x_ = 0.0f;
                angle_y_ = 0.0f;
            }
        } break;
        case SDL_MOUSEBUTTONDOWN: {
            mouse_grabbed_ = true;
        } break;
        case SDL_MOUSEBUTTONUP:
            mouse_grabbed_ = false;
            break;
        case SDL_MOUSEMOTION:
            if (mouse_grabbed_) {
                angle_y_ += 0.01f * e.motion.xrel;
                angle_x_ += -0.01f * e.motion.yrel;
            }
            break;
        case SDL_MOUSEWHEEL: {
            if (e.wheel.y > 0) {
                view_dist_ -= 0.05f * view_dist_;
            } else if (e.wheel.y < 0) {
                view_dist_ += 0.05f * view_dist_;
            }
        } break;
        case SDL_QUIT: {
            quit_ = true;
            return;
        }
        default:
            return;
        }
    }
}

void ModlApp::PrintUsage() {
    using namespace std;

    cout << "Usage:" << endl;
    cout << "\tmodl -i <input_file> [-o <output_file>]    : Compile model/anim" << endl;
    cout << "\tmodl -v <input_file> [-a <anim_file>]      : View model" << endl;
}

void ModlApp::Destroy() {
    DestroyInternal();

    view_mesh_.Release();
    checker_tex_.Release();
    ctx_.ReleaseAll();

    Sys::StopWorker();

#if defined(USE_GL_RENDER)
    SDL_GL_DeleteContext(gl_ctx_);
#endif
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

ModlApp::eCompileResult ModlApp::CompileModel(const std::string &in_file_name,
                                              const std::string &out_file_name,
                                              bool optimize, bool generate_occlusion) {
    using namespace std;
    using namespace std::placeholders;

    enum class eModelType {
        M_UNKNOWN,
        M_STATIC,
        M_COLORED,
        M_SKEL
    } mesh_type = eModelType::M_UNKNOWN;
    struct MeshInfo {
        char name[32];
        float bbox_min[3], bbox_max[3];

        MeshInfo() {
            strcpy(name, "ModelName");
            fill_n(bbox_min, 3, numeric_limits<float>::max());
            fill_n(bbox_max, 3, -numeric_limits<float>::max());
        }
    } mesh_info;
    static_assert(sizeof(MeshInfo) == 56, "fix struct packing!");
    static_assert(offsetof(MeshInfo, bbox_min) == 32, "!");
    static_assert(offsetof(MeshInfo, bbox_max) == 44, "!");

    struct OutBone {
        char name[64];
        int32_t id, parent_id;
        float bind_pos[3], bind_rot[4];
    };
    vector<OutBone> out_bones;
    static_assert(sizeof(OutBone) == 100, "fix struct packing!");
    static_assert(offsetof(OutBone, id) == 64, "!");
    static_assert(offsetof(OutBone, parent_id) == 68, "!");
    static_assert(offsetof(OutBone, bind_pos) == 72, "!");
    static_assert(offsetof(OutBone, bind_rot) == 84, "!");

    int num_vertices, num_indices;
    vector<float> positions, normals, tangents, uvs, uvs2, weights;
    vector<uint8_t> vtx_colors;
    vector<string> materials;
    vector<vector<uint32_t>> indices;

    vector<vector<uint32_t>> reordered_indices;

    ios::sync_with_stdio(false);
    ifstream in_file(in_file_name);
    if (!in_file) {
        cerr << "File " << in_file_name << " not found!" << endl;
        return eCompileResult::RES_FILE_NOT_FOUND;
    }

    string toks[64];
    for (string &s : toks) {
        s.reserve(64);
    }

    { // get mesh type
        string str;
        getline(in_file, str);

        {
            const int toks_count = Tokenize(str, " ", toks);
            if (!toks_count)
                return eCompileResult::RES_PARSE_ERROR;
            str = toks[0];
        }

        if (str == "STATIC_MESH") {
            mesh_type = eModelType::M_STATIC;
        } else if (str == "COLORED_MESH") {
            mesh_type = eModelType::M_COLORED;
        } else if (str == "SKELETAL_MESH") {
            mesh_type = eModelType::M_SKEL;
        } else {
            cerr << "Unknown mesh type" << endl;
            return eCompileResult::RES_PARSE_ERROR;
        }
    }

    { // prepare containers
        string str;

        getline(in_file, str);
        num_vertices = stoi(str);
        positions.reserve((size_t)num_vertices * 3);
        normals.reserve((size_t)num_vertices * 3);
        uvs.reserve((size_t)num_vertices * 2);
        uvs2.reserve((size_t)num_vertices * 2);
        vtx_colors.reserve((size_t)num_vertices * 4);
        weights.reserve((size_t)num_vertices * 4 * 2);

        getline(in_file, str);
        num_indices = stoi(str);
    }

    std::cout << "Reading vertex data... ";
    std::cout.flush();

    { // parse vertex information
        string str;
        str.reserve(512);

        for (int i = 0; i < num_vertices; i++) {
            getline(in_file, str);
            const int toks_count = Tokenize(str, " ", toks);
            if ((mesh_type == eModelType::M_STATIC && toks_count != 10) ||
                (mesh_type == eModelType::M_COLORED && toks_count != 12) ||
                (mesh_type == eModelType::M_SKEL && toks_count < 10)) {
                cerr << "Wrong number of tokens!" << endl;
                return eCompileResult::RES_PARSE_ERROR;
            }

            // parse vertex positions
            for (int j : {0, 1, 2}) {
                const float v = stof(toks[j]);
                positions.push_back(v);
                mesh_info.bbox_min[j] = min(mesh_info.bbox_min[j], v);
                mesh_info.bbox_max[j] = max(mesh_info.bbox_max[j], v);
            }

            // parse vertex normals
            for (int j : {3, 4, 5}) {
                normals.push_back(stof(toks[j]));
            }

            // parse vertex uvs
            for (int j : {6, 7}) {
                uvs.push_back(stof(toks[j]));
            }

            if (mesh_type == eModelType::M_STATIC || mesh_type == eModelType::M_SKEL) {
                // parse additional uvs
                for (int j : {8, 9}) {
                    uvs2.push_back(stof(toks[j]));
                }
            }

            if (mesh_type == eModelType::M_COLORED) {
                // parse per vertex color
                for (int j : {8, 9, 10, 11}) {
                    vtx_colors.push_back((uint8_t)(stof(toks[j]) * 255.0f));
                }
            } else if (mesh_type == eModelType::M_SKEL) {
                // parse joint indices and weights (limited to four bones)
                int bones_count = (toks_count - 10) / 2;
                int start_index = (int)weights.size();

                std::pair<int32_t, float> parsed_bones[16];
                int parsed_bones_count = 0;
                for (int j = 0; j < bones_count; j++) {
                    parsed_bones[parsed_bones_count].first = stoi(toks[10 + j * 2 + 0]);
                    parsed_bones[parsed_bones_count++].second =
                        stof(toks[10 + j * 2 + 1]);
                }

                sort(
                    begin(parsed_bones), begin(parsed_bones) + parsed_bones_count,
                    [](const std::pair<int, float> &b1, const std::pair<int, float> &b2) {
                        return b1.second > b2.second;
                    });

                float sum = 0.0f;
                for (int j = 0; j < 4; j++) {
                    if (j < parsed_bones_count) {
                        weights.push_back(
                            reinterpret_cast<const float &>(parsed_bones[j].first));
                        sum += parsed_bones[j].second;
                    } else {
                        const int32_t zero = 0;
                        weights.push_back(reinterpret_cast<const float &>(zero));
                    }
                }

                for (int j = 0; j < 4; j++) {
                    if (j < parsed_bones_count) {
                        weights.push_back(parsed_bones[j].second / sum);
                    } else {
                        weights.push_back(0.0f);
                    }
                }
            }
        }

        // fix for flat objects
        for (int j : {0, 1, 2}) {
            if (std::abs(mesh_info.bbox_min[j] - mesh_info.bbox_max[j]) < 0.001f) {
                mesh_info.bbox_max[j] += 0.001f;
            }
        }
    }

    std::cout << "Done" << std::endl;
    std::cout << "Reading triangle data... ";
    std::cout.flush();

    { // parse triangle information
        string str;
        str.reserve(512);

        for (int i = 0; i < num_indices / 3;) {
            getline(in_file, str);

            if (str[0] > '9' || str[0] < '0') {
                Tokenize(str, " ", toks);
                materials.push_back(toks[0]);
                indices.emplace_back();
            } else {
                const int toks_count = Tokenize(str, " \t", toks);
                if (toks_count != 3)
                    return eCompileResult::RES_PARSE_ERROR;
                for (int j : {0, 1, 2}) {
                    indices.back().push_back((uint32_t)stoi(toks[j]));
                }
                i++;
            }
        }
    }

    std::cout << "Done" << std::endl;

    if (mesh_type == eModelType::M_SKEL) { // parse skeletal information
        string str;
        while (getline(in_file, str)) {
            if (str.find("skeleton") != string::npos) {
                while (str.find('{') == string::npos)
                    getline(in_file, str);
                getline(in_file, str);
                while (str.find('}') == string::npos) {
                    const int toks_count = Tokenize(str, " \t\"", toks);
                    if (toks_count != 3)
                        return eCompileResult::RES_PARSE_ERROR;
                    out_bones.emplace_back();
                    out_bones.back().id = stoi(toks[0]);
                    if (toks[1].length() >= sizeof(OutBone().name)) {
                        cerr << "Bone name is too long" << endl;
                        return eCompileResult::RES_PARSE_ERROR;
                    }
                    strcpy(out_bones.back().name, toks[1].c_str());
                    out_bones.back().parent_id = stoi(toks[2]);
                    getline(in_file, str);
                }
            } else if (str.find("bind_pose") != string::npos) {
                while (str.find('{') == string::npos)
                    getline(in_file, str);
                getline(in_file, str);
                while (str.find('}') == string::npos) {
                    const int toks_count = Tokenize(str, " \t()", toks);
                    if (toks_count != 8)
                        return eCompileResult::RES_PARSE_ERROR;
                    int bone_index = stoi(toks[0]);
                    for (int j : {0, 1, 2}) {
                        out_bones[bone_index].bind_pos[j] = stof(toks[1 + j]);
                    }
                    for (int j : {0, 1, 2, 3}) {
                        out_bones[bone_index].bind_rot[j] = stof(toks[4 + j]);
                    }
                    getline(in_file, str);
                }
            }
        }
    }

    std::cout << "Generating tangents... ";
    std::cout.flush();

    { // generate tangents
        std::vector<Ren::vertex_t> vertices(num_vertices);

        for (int i = 0; i < num_vertices; i++) {
            memcpy(&vertices[i].p[0], &positions[i * 3], sizeof(float) * 3);
            memcpy(&vertices[i].n[0], &normals[i * 3], sizeof(float) * 3);
            memset(&vertices[i].b[0], 0, sizeof(float) * 3);
            memcpy(&vertices[i].t[0][0], &uvs[i * 2], sizeof(float) * 2);
            if (mesh_type == eModelType::M_STATIC || mesh_type == eModelType::M_SKEL) {
                memcpy(&vertices[i].t[1][0], &uvs2[i * 2], sizeof(float) * 2);
            } else if (mesh_type == eModelType::M_COLORED) {
                memcpy(&vertices[i].t[1][0], &vtx_colors[i * 4], sizeof(uint8_t) * 4);
            }
            vertices[i].index = i;
        }

        for (std::vector<uint32_t> &index_group : indices) {
            Ren::ComputeTextureBasis(vertices, index_group, &index_group[0],
                                     index_group.size());
        }

        tangents.resize(vertices.size() * 3);

        for (size_t i = 0; i < vertices.size(); i++) {
            memcpy(&tangents[i * 3], &vertices[i].b[0], sizeof(float) * 3);
        }

        for (int i = num_vertices; i < (int)vertices.size(); i++) {
            positions.push_back(vertices[i].p[0]);
            positions.push_back(vertices[i].p[1]);
            positions.push_back(vertices[i].p[2]);
            normals.push_back(vertices[i].n[0]);
            normals.push_back(vertices[i].n[1]);
            normals.push_back(vertices[i].n[2]);
            uvs.push_back(vertices[i].t[0][0]);
            uvs.push_back(vertices[i].t[0][1]);

            if (mesh_type == eModelType::M_COLORED) {
                const size_t colors_start = vtx_colors.size();
                vtx_colors.resize(vtx_colors.size() + 4);
                memcpy(&vtx_colors[colors_start], &vertices[i].t[1][0],
                       sizeof(uint8_t) * 4);
            } else {
                uvs2.push_back(vertices[i].t[1][0]);
                uvs2.push_back(vertices[i].t[1][1]);
            }

            if (mesh_type == eModelType::M_SKEL) {
                for (int j = 0; j < 8; j++) {
                    weights.push_back(weights[vertices[i].index * 8 + j]);
                }
            }
        }

        num_vertices = (int)vertices.size();
    }

    std::cout << "Done" << std::endl;

    if (optimize) {
        std::cout << "Optimizing mesh... ";
        std::cout.flush();

        for (std::vector<uint32_t> &index_group : indices) {
            reordered_indices.emplace_back();
            std::vector<uint32_t> &cur_strip = reordered_indices.back();

            cur_strip.resize(index_group.size());
            Ren::ReorderTriangleIndices(&index_group[0], (uint32_t)index_group.size(),
                                        (uint32_t)num_vertices, &cur_strip[0]);
        }

        std::cout << "Done" << std::endl;
    } else {
        reordered_indices = indices;
    }

    if (generate_occlusion) {
        std::cout << "Generating occlusion... ";

        vector<Ren::Vec4f> occlusion =
            GenerateOcclusion(positions, normals, tangents, reordered_indices);
        assert(occlusion.size() == positions.size() / 3);

        vtx_colors.resize(4 * occlusion.size());

        for (size_t i = 0; i < occlusion.size(); i++) {
            vtx_colors[i * 4 + 0] = uint8_t((0.5f * occlusion[i][0] + 0.5f) * 255.0f);
            vtx_colors[i * 4 + 1] = uint8_t((0.5f * occlusion[i][1] + 0.5f) * 255.0f);
            vtx_colors[i * 4 + 2] = uint8_t((0.5f * occlusion[i][2] + 0.5f) * 255.0f);
            vtx_colors[i * 4 + 3] = uint8_t(occlusion[i][3] * 255.0f);
        }

        mesh_type = eModelType::M_COLORED;

        std::cout << "Done" << std::endl;
    }

    struct MeshChunk {
        uint32_t index, num_indices;
        uint32_t alpha;

        MeshChunk(uint32_t ndx, uint32_t num, uint32_t has_alpha)
            : index(ndx), num_indices(num), alpha(has_alpha) {}
    };
    vector<uint32_t> total_indices;
    vector<MeshChunk> total_chunks, alpha_chunks;
    vector<int> alpha_mats;

    for (int i = 0; i < (int)reordered_indices.size(); i++) {
        bool alpha_test = false;
        { // check if material has transparency
            ifstream mat_file("assets_pc/materials/" + materials[i] + ".txt");
            if (mat_file) {
                streampos file_size = mat_file.tellg();
                mat_file.seekg(0, ios::end);
                file_size = mat_file.tellg() - file_size;
                mat_file.seekg(0, ios::beg);

                unique_ptr<char[]> mat_data(new char[(size_t)file_size]);
                mat_file.read(mat_data.get(), file_size);

                Ren::MaterialRef mat_ref = ctx_.LoadMaterial(
                    materials[i].c_str(), mat_data.get(), nullptr,
                    std::bind(&ModlApp::OnProgramNeeded, this, _1, _2, _3),
                    std::bind(&ModlApp::OnTextureNeeded, this, _1));
                Ren::Material *mat = mat_ref.get();
                alpha_test = (bool)(mat->flags() & Ren::AlphaTest);
            } else {
                cerr << "material " << materials[i] << " missing!" << endl;
            }
        }

        if (alpha_test) {
            alpha_chunks.emplace_back((uint32_t)total_indices.size(),
                                      (uint32_t)reordered_indices[i].size(), 1);
            alpha_mats.push_back(i);
        } else {
            total_chunks.emplace_back((uint32_t)total_indices.size(),
                                      (uint32_t)reordered_indices[i].size(), 0);
        }

        total_indices.insert(total_indices.end(), reordered_indices[i].begin(),
                             reordered_indices[i].end());
    }

    total_chunks.insert(std::end(total_chunks), std::begin(alpha_chunks),
                        std::end(alpha_chunks));

    for (int mat_ndx : alpha_mats) {
        materials.push_back(materials[mat_ndx]);
    }
    sort(alpha_mats.begin(), alpha_mats.end());
    for (int i = (int)alpha_mats.size() - 1; i >= 0; i--) {
        materials.erase(materials.begin() + alpha_mats[i]);
    }

    // Write output file
    ofstream out_file(out_file_name, ios::binary);

    if (mesh_type == eModelType::M_STATIC) {
        out_file.write("STATIC_MESH\0", 12);
    } else if (mesh_type == eModelType::M_COLORED) {
        out_file.write("COLORE_MESH\0", 12);
    } else if (mesh_type == eModelType::M_SKEL) {
        out_file.write("SKELET_MESH\0", 12);
    }

    enum eFileChunk {
        CH_MESH_INFO = 0,
        CH_VTX_ATTR,
        CH_VTX_NDX,
        CH_MATERIALS,
        CH_STRIPS,
        CH_BONES
    };

    struct ChunkPos {
        int32_t offset, length;
    };

    struct Header {
        int32_t num_chunks;
        ChunkPos p[6];
    } file_header;

    size_t header_size = sizeof(Header);
    if (mesh_type == eModelType::M_STATIC || mesh_type == eModelType::M_COLORED) {
        header_size -= sizeof(ChunkPos);
        file_header.num_chunks = 5;
    } else {
        file_header.num_chunks = 6;
    }

    size_t file_offset = 12 + header_size;

    file_header.p[CH_MESH_INFO].offset = (int32_t)file_offset;
    file_header.p[CH_MESH_INFO].length = sizeof(mesh_info);

    file_offset += file_header.p[CH_MESH_INFO].length;
    file_header.p[CH_VTX_ATTR].offset = (int32_t)file_offset;
    if (mesh_type == eModelType::M_COLORED) {
        file_header.p[CH_VTX_ATTR].length =
            (int32_t)(sizeof(float) * (positions.size() / 3) * 11 +
                      sizeof(uint8_t) * vtx_colors.size());
    } else {
        file_header.p[CH_VTX_ATTR].length = (int32_t)(
            sizeof(float) * (positions.size() / 3) * 13 + sizeof(float) * weights.size());
    }

    file_offset += file_header.p[CH_VTX_ATTR].length;
    file_header.p[CH_VTX_NDX].offset = (int32_t)file_offset;
    file_header.p[CH_VTX_NDX].length = (int32_t)(sizeof(uint32_t) * total_indices.size());

    file_offset += file_header.p[CH_VTX_NDX].length;
    file_header.p[CH_MATERIALS].offset = (int32_t)file_offset;
    file_header.p[CH_MATERIALS].length = (int32_t)(64 * materials.size());

    file_offset += file_header.p[CH_MATERIALS].length;
    file_header.p[CH_STRIPS].offset = (int32_t)file_offset;
    file_header.p[CH_STRIPS].length = (int32_t)(sizeof(MeshChunk) * total_chunks.size());

    if (mesh_type == eModelType::M_SKEL) {
        //file_header.num_chunks++;
        file_offset += file_header.p[CH_STRIPS].length;
        file_header.p[CH_BONES].offset = (int32_t)file_offset;
        file_header.p[CH_BONES].length = (int32_t)(100 * out_bones.size());
    }

    out_file.write((char *)&file_header, header_size);
    out_file.write((char *)&mesh_info, sizeof(MeshInfo));

    for (unsigned i = 0; i < positions.size() / 3; i++) {
        out_file.write((char *)&positions[i * 3], sizeof(float) * 3);
        out_file.write((char *)&normals[i * 3], sizeof(float) * 3);
        out_file.write((char *)&tangents[i * 3], sizeof(float) * 3);
        out_file.write((char *)&uvs[i * 2], sizeof(float) * 2);
        if (mesh_type == eModelType::M_STATIC || mesh_type == eModelType::M_SKEL) {
            out_file.write((char *)&uvs2[i * 2], sizeof(float) * 2);
        } else if (mesh_type == eModelType::M_COLORED) {
            out_file.write((char *)&vtx_colors[i * 4], sizeof(uint8_t) * 4);
        }
        if (mesh_type == eModelType::M_SKEL) {
            out_file.write((char *)&weights[i * 8], sizeof(float) * 8);
        }
    }

    out_file.write((char *)&total_indices[0], sizeof(uint32_t) * total_indices.size());

    for (std::string &str : materials) {
        char name[64]{};
        strcpy(name, str.c_str());
        strcat(name, ".txt");
        out_file.write(name, sizeof(name));
    }

    out_file.write((char *)&total_chunks[0], sizeof(MeshChunk) * total_chunks.size());

    if (mesh_type == eModelType::M_SKEL) {
        for (OutBone &bone : out_bones) {
            out_file.write((char *)&bone.name, 64);
            out_file.write((char *)&bone.id, sizeof(int32_t));
            out_file.write((char *)&bone.parent_id, sizeof(int32_t));
            out_file.write((char *)&bone.bind_pos, sizeof(float) * 3);
            out_file.write((char *)&bone.bind_rot, sizeof(float) * 4);
        }
    }

    return eCompileResult::RES_SUCCESS;
}

ModlApp::eCompileResult ModlApp::CompileAnim(const std::string &in_file_name,
                                             const std::string &out_file_name) {
    using namespace std;

    ifstream in_file(in_file_name);
    if (!in_file) {
        cerr << "File " << in_file_name << " not exists" << endl;
        return eCompileResult::RES_FILE_NOT_FOUND;
    }

    streampos file_size = in_file.tellg();
    in_file.seekg(0, ios::end);
    file_size = in_file.tellg() - file_size;
    in_file.seekg(0, ios::beg);

    { // check file type
        string str;
        getline(in_file, str);
        if (str != "ANIM_SEQUENCE") {
            cerr << "Wrong file type" << endl;
            return eCompileResult::RES_PARSE_ERROR;
        }
        in_file.ignore(file_size, '\t');
    }

    enum class eAnimType { Rotation, RotationTranslation };

    struct OutAnimBone {
        char name[64];
        char parent_name[64];
        int32_t anim_type;

        OutAnimBone() : name{}, parent_name{}, anim_type(0) {}
    };

    struct OutAnimInfo {
        char name[64];
        int32_t fps, len;

        OutAnimInfo() : name{}, fps(0), len(0) {}
    } anim_info;

    vector<OutAnimBone> out_bones;
    vector<float> frames;
    int frame_size = 0;

    string toks[64];
    for (string &s : toks) {
        s.reserve(64);
    }

    { // parse bones info
        string str;
        getline(in_file, str);
        while (str.find('}') == string::npos) {
            const int toks_count = Tokenize(str, " \"", toks);
            if (toks_count < 3)
                return eCompileResult::RES_PARSE_ERROR;
            OutAnimBone b;
            if (toks[1] == "RT") {
                b.anim_type = int32_t(eAnimType::RotationTranslation);
                frame_size += 7;
            } else if (toks[1] == "R") {
                b.anim_type = int32_t(eAnimType::Rotation);
                frame_size += 3;
            }
            strcpy(b.name, toks[2].c_str());
            if (toks_count > 3) {
                strcpy(b.parent_name, toks[3].c_str());
            } else {
                strcpy(b.parent_name, "None");
            }
            out_bones.push_back(b);
            getline(in_file, str);
        }
    }

    { // prepare containers
        string str;
        getline(in_file, str);
        const int toks_count = Tokenize(str, " []/", toks);
        if (toks_count != 3)
            return eCompileResult::RES_PARSE_ERROR;
        strcpy(anim_info.name, toks[0].c_str());
        anim_info.len = stoi(toks[1]);
        anim_info.fps = stoi(toks[2]);
        frames.reserve((size_t)frame_size * anim_info.len);
        getline(in_file, str);
    }

    { // parse frame animation
        string str;
        for (int i = 0; i < anim_info.len; i++) {
            getline(in_file, str);
            for (int j = 0; j < (int)out_bones.size(); j++) {
                getline(in_file, str);
                Tokenize(str, " ", toks);
                for (int k = 1; k < ((out_bones[j].anim_type ==
                                      int32_t(eAnimType::RotationTranslation))
                                         ? 8
                                         : 5);
                     k++) {
                    frames.push_back(stof(toks[k]));
                }
            }
        }
    }

    // Write output file

    enum eFileChunk { CH_SKELETON, CH_ANIM_INFO, CH_FRAMES };

    struct ChunkPos {
        int32_t offset, length;
    };

    static_assert(sizeof(ChunkPos) == 8, "fix struct packing!");
    static_assert(offsetof(ChunkPos, length) == 4, "!");

    struct Header {
        int32_t num_chunks;
        ChunkPos p[3];
    } file_header;

    static_assert(sizeof(Header) == 28, "fix struct packing!");
    static_assert(offsetof(Header, p) == 4, "!");

    size_t file_offset = 12 + sizeof(Header);
    file_header.num_chunks = 3;
    file_header.p[CH_SKELETON].offset = (int32_t)file_offset;
    file_header.p[CH_SKELETON].length = (int32_t)(sizeof(OutAnimBone) * out_bones.size());

    file_offset += file_header.p[CH_SKELETON].length;
    file_header.p[CH_ANIM_INFO].offset = (int32_t)file_offset;
    file_header.p[CH_ANIM_INFO].length = (int32_t)(sizeof(OutAnimInfo));

    file_offset += file_header.p[CH_ANIM_INFO].length;
    file_header.p[CH_FRAMES].offset = (int32_t)file_offset;
    file_header.p[CH_FRAMES].length = (int32_t)(sizeof(float) * frames.size());

    ofstream out_file(out_file_name, ios::binary);
    out_file.write("ANIM_SEQUEN\0", 12);

    out_file.write((char *)&file_header, sizeof(Header));

    out_file.write((char *)&out_bones[0], file_header.p[CH_SKELETON].length);
    out_file.write((char *)&anim_info, file_header.p[CH_ANIM_INFO].length);
    out_file.write((char *)&frames[0], file_header.p[CH_FRAMES].length);

    cout << "*** Anim info ***" << endl;
    cout << "Name:\t" << anim_info.name << endl;
    cout << "Bones:\t" << out_bones.size() << endl;

    return eCompileResult::RES_SUCCESS;
}

std::vector<Ren::Vec4f>
ModlApp::GenerateOcclusion(const std::vector<float> &positions,
                           const std::vector<float> &normals,
                           const std::vector<float> &tangents,
                           const std::vector<std::vector<uint32_t>> &indices) const {
    using Ren::Vec2f;
    using Ren::Vec3f;

    size_t primitives_count = 0;
    for (const std::vector<uint32_t> &index_grp : indices) {
        primitives_count += index_grp.size();
    }

    std::vector<prim_t> primitives;
    std::vector<uint32_t> tri_indices, prim_indices;
    primitives.reserve(primitives_count);
    tri_indices.reserve(primitives_count);
    prim_indices.reserve(primitives_count);

    for (const std::vector<uint32_t> &index_grp : indices) {
        for (size_t i = 0; i < index_grp.size(); i += 3) {
            const uint32_t i0 = index_grp[i + 0];
            const uint32_t i1 = index_grp[i + 1];
            const uint32_t i2 = index_grp[i + 2];

            tri_indices.push_back(i0);
            tri_indices.push_back(i1);
            tri_indices.push_back(i2);

            const Ren::Vec3f v0 = Ren::MakeVec3(&positions[3 * i0]);
            const Ren::Vec3f v1 = Ren::MakeVec3(&positions[3 * i1]);
            const Ren::Vec3f v2 = Ren::MakeVec3(&positions[3 * i2]);

            primitives.emplace_back();
            prim_t &new_prim = primitives.back();

            new_prim.bbox_min = Ren::Min(v0, Ren::Min(v1, v2));
            new_prim.bbox_max = Ren::Max(v0, Ren::Max(v1, v2));
        }
    }

    struct bvh_node_t { // NOLINT
        uint32_t prim_index, prim_count, left_child, right_child;
        Ren::Vec3f bbox_min;
        uint32_t parent;
        Ren::Vec3f bbox_max;
        uint32_t space_axis; // axis with maximal child's centroids distance
    };

    std::vector<bvh_node_t> nodes;
    size_t root_node = 0;
    size_t nodes_count = 0;

    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Ren::Vec3f min = Ren::Vec3f{std::numeric_limits<float>::max()};
        Ren::Vec3f max = Ren::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() = default;
        prims_coll_t(std::vector<uint32_t> &&_indices, const Ren::Vec3f &_min,
                     const Ren::Vec3f &_max)
            : indices(std::move(_indices)), min(_min), max(_max) {}
    };

    std::deque<prims_coll_t> prim_lists;
    prim_lists.emplace_back();

    for (size_t i = 0; i < primitives.size(); i++) {
        prim_lists.back().indices.push_back((uint32_t)i);
        prim_lists.back().min = Min(prim_lists.back().min, primitives[i].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, primitives[i].bbox_max);
    }

    const Ren::Vec3f root_min = prim_lists.back().min;
    const Ren::Vec3f root_max = prim_lists.back().max;

    split_settings_t s;
    s.oversplit_threshold = 0.95f;
    s.node_traversal_cost = 0.025f;

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(
            &primitives[0], prim_lists.back().indices.data(),
            (uint32_t)prim_lists.back().indices.size(), prim_lists.back().min,
            prim_lists.back().max, root_min, root_max, s);
        prim_lists.pop_back();

        const auto leaf_index = (uint32_t)nodes.size();
        uint32_t parent_index = 0xffffffff;

        if (leaf_index) {
            // skip bound checks in debug mode
            const bvh_node_t *_out_nodes = &nodes[0];
            for (uint32_t i = leaf_index - 1; i >= root_node; i--) {
                if (_out_nodes[i].left_child == leaf_index ||
                    _out_nodes[i].right_child == leaf_index) {
                    parent_index = (uint32_t)i;
                    break;
                }
            }
        }

        if (split_data.right_indices.empty()) {
            nodes.push_back({(uint32_t)prim_indices.size(),
                             (uint32_t)split_data.left_indices.size(), 0, 0,
                             split_data.left_bounds[0], parent_index,
                             split_data.left_bounds[1], 0});
            prim_indices.insert(prim_indices.end(), split_data.left_indices.begin(),
                                split_data.left_indices.end());
        } else {
            auto index = (uint32_t)nodes_count;

            const Ren::Vec3f c_left =
                (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f;
            const Ren::Vec3f c_right =
                (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Ren::Vec3f dist = Abs(c_left - c_right);

            const uint32_t space_axis =
                (dist[0] > dist[1] && dist[0] > dist[2])
                    ? 0
                    : ((dist[1] > dist[0] && dist[1] > dist[2]) ? 1 : 2);

            const Ren::Vec3f bbox_min = Min(split_data.left_bounds[0],
                                            split_data.right_bounds[0]),
                             bbox_max = Max(split_data.left_bounds[1],
                                            split_data.right_bounds[1]);

            nodes.push_back({
                0,
                0,
                index + 1,
                index + 2,
                Ren::Vec3f{bbox_min[0], bbox_min[1], bbox_min[2]},
                parent_index,
                Ren::Vec3f{bbox_max[0], bbox_max[1], bbox_max[2]},
                space_axis,
            });

            prim_lists.emplace_front(std::move(split_data.left_indices),
                                     split_data.left_bounds[0],
                                     split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices),
                                     split_data.right_bounds[0],
                                     split_data.right_bounds[1]);

            nodes_count += 2;
        }
    }

    auto intersect_triangle = [](const Vec3f &orig, const Vec3f &dir, const Vec3f &v0,
                                 const Vec3f &v1, const Vec3f &v2, Vec3f &inter) {
        const float eps = 0.000001f;

        const Vec3f edge1 = v1 - v0;
        const Vec3f edge2 = v2 - v0;

        const Vec3f pvec = Cross(dir, edge2);
        const float det = Dot(edge1, pvec);

        if (det > -eps && det < eps) {
            return false;
        }
        const float inv_det = 1.0f / det;

        const Vec3f tvec = orig - v0;
        const float u = Dot(tvec, pvec) * inv_det;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }

        const Vec3f qvec = Cross(tvec, edge1);
        const float v = Dot(dir, qvec) * inv_det;
        if (v < 0.0f || (u + v) > 1.0f) {
            return false;
        }

        const float t = Dot(edge2, qvec) * inv_det;

        inter[0] = t;
        inter[1] = u;
        inter[2] = v;

        return true;
    };

    auto bbox_test = [](const Vec3f &o, const Vec3f &inv_d, const float t,
                        const Vec3f &bbox_min, const Vec3f bbox_max) {
        float lo_x = inv_d[0] * (bbox_min[0] - o[0]);
        float hi_x = inv_d[0] * (bbox_max[0] - o[0]);
        if (lo_x > hi_x) {
            float tmp = lo_x;
            lo_x = hi_x;
            hi_x = tmp;
        }

        float lo_y = inv_d[1] * (bbox_min[1] - o[1]);
        float hi_y = inv_d[1] * (bbox_max[1] - o[1]);
        if (lo_y > hi_y) {
            float tmp = lo_y;
            lo_y = hi_y;
            hi_y = tmp;
        }

        float lo_z = inv_d[2] * (bbox_min[2] - o[2]);
        float hi_z = inv_d[2] * (bbox_max[2] - o[2]);
        if (lo_z > hi_z) {
            float tmp = lo_z;
            lo_z = hi_z;
            hi_z = tmp;
        }

        float tmin = lo_x > lo_y ? lo_x : lo_y;
        if (lo_z > tmin) {
            tmin = lo_z;
        }
        float tmax = hi_x < hi_y ? hi_x : hi_y;
        if (hi_z < tmax) {
            tmax = hi_z;
        }
        tmax *= 1.00000024f;

        return tmin <= tmax && tmin <= t && tmax > 0;
    };

    auto trace_occ_ray = [&](const Vec3f &orig, const Vec3f &dir, float max_dist) {
        const Vec3f inv_dir = 1.0f / dir;

        uint32_t stack[128];
        uint32_t stack_size = 0;

        stack[stack_size++] = (uint32_t)root_node;

        while (stack_size) {
            uint32_t cur = stack[--stack_size];
            const bvh_node_t &n = nodes[cur];

            if (!bbox_test(orig, inv_dir, max_dist, n.bbox_min, n.bbox_max)) {
                continue;
            }

            if (!n.prim_count) {
                stack[stack_size++] = n.left_child;
                stack[stack_size++] = n.right_child;
            } else {
                for (uint32_t i = n.prim_index; i < n.prim_index + n.prim_count; ++i) {
                    const int pi = prim_indices[i];

                    const Vec3f v0 =
                        Ren::MakeVec3(&positions[3 * tri_indices[3 * pi + 0]]);
                    const Vec3f v1 =
                        Ren::MakeVec3(&positions[3 * tri_indices[3 * pi + 1]]);
                    const Vec3f v2 =
                        Ren::MakeVec3(&positions[3 * tri_indices[3 * pi + 2]]);

                    Vec3f inter;
                    if (intersect_triangle(orig, dir, v0, v1, v2, inter) &&
                        inter[0] > 0.0f && inter[0] < max_dist) {
                        return true;
                    }
                }
            }
        }

        return false;
    };

    std::vector<Ren::Vec4f> occlusion;
    occlusion.reserve(positions.size() / 3);

    for (size_t i = 0; i < positions.size(); i += 3) {
        const int SampleCount = 128;
        const float bias = 0.001f;

        const Vec3f N = Ren::MakeVec3(&normals[i]);
        const Vec3f T = Ren::MakeVec3(&tangents[i]);
        const Vec3f B = Cross(N, T);

        const Vec3f orig = Ren::MakeVec3(&positions[i]);

        int occ_count = 0;
        Vec3f unoccluded_dir;
        for (int i = 0; i < SampleCount; i++) {
            const Vec2f uv = Hammersley2D(i, SampleCount);

            const float phi = 2 * Ren::Pi<float>() * uv[1];
            const float cos_phi = std::cos(phi);
            const float sin_phi = std::sin(phi);

            const float r = std::sqrt(1.0f - uv[0] * uv[0]);
            const Vec3f dir = r * sin_phi * B + uv[0] * N + r * cos_phi * T;

            if (trace_occ_ray(orig + bias * dir, dir, 0.25f)) {
                ++occ_count;
            } else {
                unoccluded_dir += dir;
            }
        }

        unoccluded_dir = Normalize(unoccluded_dir);

        const float x = Dot(unoccluded_dir, T);
        const float y = Dot(unoccluded_dir, B);
        const float z = Dot(unoccluded_dir, N);

        occlusion.emplace_back(x, y, z,
                               float(SampleCount - occ_count) / float(SampleCount));
    }

    return occlusion;
}

Ren::Texture2DRef ModlApp::OnTextureNeeded(const char *name) {
    Ren::eTexLoadStatus status;
    Ren::Texture2DRef ret = ctx_.LoadTexture2D(name, nullptr, 0, {}, &status);
    if (!ret->ready()) {
        std::string tex_name = name;
        Sys::LoadAssetComplete(
            (std::string("assets_pc/textures/") + tex_name).c_str(),
            [this, tex_name](void *data, int size) {
                ctx_.ProcessSingleTask([this, tex_name, data, size]() {
                    Ren::Texture2DParams p;
                    p.filter = Ren::eTexFilter::Trilinear;
                    p.repeat = Ren::eTexRepeat::Repeat;
                    ctx_.LoadTexture2D(tex_name.c_str(), data, size, p, nullptr);
                    LOGI("Texture %s loaded", tex_name.c_str());
                });
            },
            [tex_name]() { LOGE("Error loading %s", tex_name.c_str()); });
    }

    return ret;
}

Ren::ProgramRef ModlApp::OnProgramNeeded(const char *name, const char *vs_shader,
                                         const char *fs_shader) {
#if defined(USE_GL_RENDER)
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx_.LoadProgramGLSL(name, nullptr, nullptr, &status);
    if (!ret->ready()) {
        using namespace std;

        Sys::AssetFile vs_file(string("assets_pc/shaders/") + vs_shader),
            fs_file(string("assets_pc/shaders/") + fs_shader);
        if (!vs_file || !fs_file) {
            LOGE("Error loading program %s", name);
            return ret;
        }

        size_t vs_size = vs_file.size(), fs_size = fs_file.size();

        string vs_src, fs_src;
        vs_src.resize(vs_size);
        fs_src.resize(fs_size);
        vs_file.Read((char *)vs_src.data(), vs_size);
        fs_file.Read((char *)fs_src.data(), fs_size);

        ret = ctx_.LoadProgramGLSL(name, vs_src.c_str(), fs_src.c_str(), &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
    }
    return ret;
#elif defined(USE_SW_RENDER)
    Ren::ProgramRef LoadSWProgram(Ren::Context &, const char *);
    return LoadSWProgram(ctx_, name);
#endif
}

Ren::MaterialRef ModlApp::OnMaterialNeeded(const char *name) {
    using namespace std;

    Ren::eMatLoadStatus status;
    Ren::MaterialRef ret = ctx_.LoadMaterial(name, nullptr, &status, nullptr, nullptr);
    if (!ret->ready()) {
        Sys::AssetFile in_file(string("assets_pc/materials/") + name);
        if (!in_file) {
            LOGE("Error loading material %s", name);
            return ret;
        }

        size_t file_size = in_file.size();

        string mat_src;
        mat_src.resize(file_size);
        in_file.Read((char *)mat_src.data(), file_size);

        using namespace std::placeholders;

        ret = ctx_.LoadMaterial(name, mat_src.data(), &status,
                                std::bind(&ModlApp::OnProgramNeeded, this, _1, _2, _3),
                                std::bind(&ModlApp::OnTextureNeeded, this, _1));
        assert(status == Ren::MatCreatedFromData);
    }
    return ret;
}
