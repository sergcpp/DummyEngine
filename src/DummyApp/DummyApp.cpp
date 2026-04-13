#include "DummyApp.h"

#include <cstring>

#include <Eng/ViewerBase.h>
#include <Eng/input/InputManager.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

const char *DummyApp::Version() const { return "v0.2.1-unknown-commit"; }

void DummyApp::ParseArgs(int argc, char *argv[], int &w, int &h, AppParams &out_params) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--prepare-assets") == 0) {
            Viewer::PrepareAssets(argv[i + 1]);
            i++;
        } else if (strcmp(arg, "--no-run") == 0) {
            std::exit(0);
        } else if ((strcmp(arg, "--width") == 0 || strcmp(arg, "-w") == 0) && (i + 1 < argc)) {
            w = std::atoi(argv[++i]);
        } else if ((strcmp(arg, "--height") == 0 || strcmp(arg, "-h") == 0) && (i + 1 < argc)) {
            h = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--fullscreen") == 0 || strcmp(arg, "-fs") == 0) {
            fullscreen_ = true;
        } else if (strcmp(arg, "--device") == 0 || strcmp(arg, "-d") == 0) {
            out_params.device_name = argv[++i];
        } else if (strcmp(arg, "--validation-level") == 0 || strcmp(arg, "-vl") == 0) {
            out_params.validation_level = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--no-vsync") == 0) {
            out_params.novsync = true;
        } else if (strcmp(arg, "--no-hwrt") == 0) {
            out_params.nohwrt = true;
        } else if (strcmp(arg, "--no-subgroup") == 0) {
            out_params.nosubgroup = true;
        } else if (strcmp(argv[i], "--no-show") == 0) {
            out_params.noshow = true;
        } else if ((strcmp(argv[i], "--scene") == 0 || strcmp(argv[i], "-s") == 0) && (++i != argc)) {
            out_params.scene_name = argv[i];
        } else if ((strcmp(argv[i], "--reference") == 0 || strcmp(argv[i], "-ref") == 0) && (++i != argc)) {
            out_params.ref_name = argv[i];
        } else if (strcmp(argv[i], "--psnr") == 0 && (++i != argc)) {
            out_params.psnr[0] = strtod(argv[i], nullptr);
            while ((i + 1) != argc && argv[i + 1][0] != '-') {
                out_params.psnr.push_back(strtod(argv[++i], nullptr));
            }
        } else if (strcmp(argv[i], "--pt") == 0) {
            out_params.pt = true;
        } else if (strcmp(argv[i], "--pt-no-denoise") == 0) {
            out_params.pt_denoise = false;
        } else if (strcmp(arg, "--pt-max-samples") == 0 && (i + 1 < argc)) {
            out_params.pt_max_samples = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-max-diff-depth") == 0 && (i + 1 < argc)) {
            out_params.pt_max_diff_depth = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-max-spec-depth") == 0 && (i + 1 < argc)) {
            out_params.pt_max_spec_depth = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-max-refr-depth") == 0 && (i + 1 < argc)) {
            out_params.pt_max_refr_depth = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-max-transp-depth") == 0 && (i + 1 < argc)) {
            out_params.pt_max_transp_depth = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-max-total-depth") == 0 && (i + 1 < argc)) {
            out_params.pt_max_total_depth = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--pt-clamp-direct") == 0 && (i + 1 < argc)) {
            out_params.pt_clamp_direct = strtof(argv[++i], nullptr);
        } else if (strcmp(arg, "--pt-clamp-indirect") == 0 && (i + 1 < argc)) {
            out_params.pt_clamp_indirect = strtof(argv[++i], nullptr);
        } else if (strcmp(argv[i], "--exposure") == 0 && (++i != argc)) {
            out_params.exposure = strtof(argv[i], nullptr);
        } else if (strcmp(argv[i], "--cam-path") == 0 && (++i != argc)) {
            out_params.cam_path = std::stoi(argv[i]);
        } else if (strcmp(argv[i], "--preset") == 0 && (++i != argc)) {
            if (strcmp(argv[i], "medium") == 0) {
                out_params.gfx_preset = eGfxPreset::Medium;
            } else if (strcmp(argv[i], "high") == 0) {
                out_params.gfx_preset = eGfxPreset::High;
            } else if (strcmp(argv[i], "ultra") == 0) {
                out_params.gfx_preset = eGfxPreset::Ultra;
            }
        } else if (strcmp(argv[i], "--res-scale") == 0 && (++i != argc)) {
            out_params.res_scale = strtof(argv[i], nullptr);
        } else if (strcmp(argv[i], "--sun-dir") == 0 && (++i != argc)) {
            out_params.sun_dir[0] = strtof(argv[i++], nullptr);
            out_params.sun_dir[1] = strtof(argv[i++], nullptr);
            out_params.sun_dir[2] = strtof(argv[i], nullptr);
        } else if (strcmp(argv[i], "--no-postprocess") == 0) {
            out_params.postprocess = false;
        } else if (strcmp(argv[i], "--no-fog") == 0) {
            out_params.fog = false;
        } else if (strcmp(argv[i], "--freeze-sky") == 0) {
            out_params.freeze_sky = true;
        } else if (strcmp(argv[i], "--tex-budget") == 0 && (i + 1 != argc)) {
            out_params.tex_budget = std::atoi(argv[++i]);
        }
    }
}

void DummyApp::Frame() {
    if (!minimized_) {
        viewer_->Frame();
    }
}

void DummyApp::Resize(const int w, const int h, const bool novsync) {
    minimized_ = (w == 0 || h == 0);
    if (viewer_ && !minimized_) {
        viewer_->Resize(w, h, novsync);
    }
}

void DummyApp::AddEvent(const Eng::eInputEvent type, const uint32_t key_code, const float x, const float y,
                        const float dx, const float dy) {
    if (!input_manager_) {
        return;
    }

    Eng::input_event_t evt;
    evt.type = type;
    evt.key_code = key_code;
    evt.point[0] = x;
    evt.point[1] = viewer_->height - 1 - y;
    evt.move[0] = dx;
    evt.move[1] = -dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager_->AddRawInputEvent(evt);
}