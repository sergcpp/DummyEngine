#include "GSVideoTest.h"

#include <fstream>
#include <memory>

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

#include <Eng/Log.h>
#include <Eng/gui/Renderer.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/utils/Cmdline.h>
#include <Eng/utils/ShaderLoader.h>
#include <Ren/CPUFeatures.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/ThreadPool.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>
#undef GetObject

#include "../Viewer.h"

namespace GSVideoTestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "living_room_gumroad_vid.json";

__itt_string_handle *itt_copy_pbo_str = __itt_string_handle_create("Copy to PBO");
__itt_string_handle *itt_decode_str = __itt_string_handle_create("Decode Frame");
__itt_string_handle *itt_vid_tex_str = __itt_string_handle_create("Video Textures");
__itt_string_handle *itt_update_tex_str = __itt_string_handle_create("Update Textures");

extern const bool VerboseLogging;
} // namespace GSVideoTestInternal

#define UPDATE_PBO_FROM_SEPARATE_THREAD
#define UPDATE_TEX_FROM_SEPARATE_CONTEXT
// #define FORCE_WAIT_FOR_DECODER
#define FORCE_UPLOAD_EVERY_FRAME

#if defined(UPDATE_TEX_FROM_SEPARATE_CONTEXT) && !defined(UPDATE_PBO_FROM_SEPARATE_THREAD)
#error "!!!!"
#endif

GSVideoTest::GSVideoTest(Viewer *viewer) : GSBaseState(viewer) {
    // aux_gfx_thread_ = game_->GetComponent<Sys::ThreadWorker>(AUX_GFX_THREAD);
    // decoder_threads_ = std::make_shared<Sys::QThreadPool>(4 /* threads */, 8 /* queues */, "decoder_thread");
}

void GSVideoTest::Enter() {
    using namespace GSVideoTestInternal;

    GSBaseState::Enter();

    log_->Info("GSVideoTest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    OpenVideoFiles();
    InitVideoTextures();

    const Eng::SceneData &scene = scene_manager_->scene_data();

    for (int i = 0; i < 5; i++) {
        if (wall_picture_indices_[i] == 0xffffffff || !vp_[i].initialized()) {
            continue;
        }

        Eng::SceneObject *wall_pic = scene_manager_->GetObject(wall_picture_indices_[i]);

        const uint32_t mask = Eng::CompDrawableBit;
        if ((wall_pic->comp_mask & mask) == mask) {
            auto *dr =
                (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(wall_pic->components[Eng::CompDrawable]);

            Ren::Mesh *mesh = dr->mesh.get();

            for (auto &grp : mesh->groups()) {
                // hold reference to original material here
                Ren::MaterialRef mat = grp.front_mat;
                if (mat->name() == "wall_picture_yuv.mat") {
                    // replace material
                    grp.front_mat = vid_mat_[i];
                }
            }
        }
    }
}

bool GSVideoTest::OpenVideoFiles() {
#if defined(__ANDROID__)
#define TEXTURE_PATH "assets/textures/"
#else
#define TEXTURE_PATH "assets_pc/textures/"
#endif

    const char *file_names[] = {TEXTURE_PATH "video_test/turtle.ivf", TEXTURE_PATH "/video_test/train.ivf",
                                TEXTURE_PATH "/video_test/winter.ivf", TEXTURE_PATH "/video_test/bird.ivf",
                                TEXTURE_PATH "/video_test/wood.ivf"};

#undef TEXTURE_PATH

    for (int i = 0; i < 5; i++) {
        if (!vp_[i].OpenAndPreload(file_names[i], log_)) {
            log_->Error("GSVideoTest: Failed to load video file %s!", file_names[i]);
            return false;
        }
    }
    return true;
}

void GSVideoTest::OnPostloadScene(JsObjectP &js_scene) {
    GSBaseState::OnPostloadScene(js_scene);

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            initial_view_pos_[0] = float(js_orig.at(0).as_num().val);
            initial_view_pos_[1] = float(js_orig.at(1).as_num().val);
            initial_view_pos_[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            initial_view_dir_[0] = float(js_dir.at(0).as_num().val);
            initial_view_dir_[1] = float(js_dir.at(1).as_num().val);
            initial_view_dir_[2] = float(js_dir.at(2).as_num().val);
        }

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            max_fwd_speed_ = float(js_fwd_speed.val);
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov_ = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure_ = float(js_max_exposure.val);
        }
    }

    view_origin_ = initial_view_pos_;
    view_dir_ = initial_view_dir_;

    Eng::SceneData &scene = scene_manager_->scene_data();

    wall_picture_indices_[0] = scene_manager_->FindObject("wall_picture_0");
    wall_picture_indices_[1] = scene_manager_->FindObject("wall_picture_1");
    wall_picture_indices_[2] = scene_manager_->FindObject("wall_picture_2");
    wall_picture_indices_[3] = scene_manager_->FindObject("wall_picture_3");
    wall_picture_indices_[4] = scene_manager_->FindObject("wall_picture_4");
}

void GSVideoTest::Exit() {
    for (int i = 0; i < 5; i++) {
        if (vid_update_done_[i].valid()) {
            vid_update_done_[i].wait();
#ifdef UPDATE_TEX_FROM_SEPARATE_CONTEXT
            assert(tex_update_done_[i].valid());
            tex_update_done_[i].wait();
#endif
        }
        y_sbuf_[i].Unmap();
        uv_sbuf_[i].Unmap();
    }

    DestroyVideoTextures();

    GSBaseState::Exit();
}

void GSVideoTest::Draw() {
    using namespace GSVideoTestInternal;

    const uint64_t t1 = Sys::GetTimeUs();

    UpdateVideoTextures();

    const uint64_t t2 = Sys::GetTimeUs();

    static double time_acc = 0.0;

    const double alpha = 1.0; // 0.002;
    time_acc = (1.0 - alpha) * time_acc + alpha * double(t2 - t1);

    log_->Info("Main thread overhead is %fms", 0.001 * time_acc);

    //
    // Draw scene
    //
    GSBaseState::Draw();
}

void GSVideoTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) { GSBaseState::DrawUI(r, root); }

void GSVideoTest::UpdateFixed(const uint64_t dt_us) {
    using namespace GSVideoTestInternal;

    const Ren::Vec3f up = Ren::Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir_, up));

    const float fwd_speed = std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
                side_speed = std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

    view_origin_ += view_dir_ * fwd_speed;
    view_origin_ += side * side_speed;

    if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
        invalidate_view_ = true;
    }
}

bool GSVideoTest::HandleInput(const Eng::InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSVideoTestInternal;

    // pt switch for touch controls
    if (evt.type == Eng::RawInputEv::P1Down || evt.type == Eng::RawInputEv::P2Down) {
        if (evt.point.x > float(ren_ctx_->w()) * 0.9f && evt.point.y < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    // scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }
                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    bool input_processed = true;

    switch (evt.type) {
    case Eng::RawInputEv::P1Down:
        if (evt.point.x < (float(ren_ctx_->w()) / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case Eng::RawInputEv::P2Down:
        if (evt.point.x < (float(ren_ctx_->w()) / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case Eng::RawInputEv::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case Eng::RawInputEv::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case Eng::RawInputEv::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move.dx, up);
            rot = Rotate(rot, -0.005f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::RawInputEv::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move.dx, up);
            rot = Rotate(rot, 0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::RawInputEv::KeyDown: {
        if (evt.key_code == Eng::KeyUp || (evt.key_code == Eng::KeyW && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyDown ||
                   (evt.key_code == Eng::KeyS && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyLeft ||
                   (evt.key_code == Eng::KeyA && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyRight ||
                   (evt.key_code == Eng::KeyD && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::KeySpace) {
            enable_video_update_ = !enable_video_update_;
        } else if (evt.key_code == Eng::KeyLeftShift || evt.key_code == Eng::KeyRightShift) {
            shift_down_ = true;
        } else {
            input_processed = false;
        }
    } break;
    case Eng::RawInputEv::KeyUp: {
        if (!cmdline_enabled_ || view_pointer_) {
            if (evt.key_code == Eng::KeyUp || evt.key_code == Eng::KeyW || evt.key_code == Eng::KeyDown ||
                evt.key_code == Eng::KeyS) {
                fwd_press_speed_ = 0;
            } else if (evt.key_code == Eng::KeyLeft || evt.key_code == Eng::KeyA || evt.key_code == Eng::KeyRight ||
                       evt.key_code == Eng::KeyD) {
                side_press_speed_ = 0;
            } else {
                input_processed = false;
            }
        } else {
            input_processed = false;
        }
    } break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}

void GSVideoTest::UpdateAnim(const uint64_t dt_us) {
    const float delta_time_s = dt_us * 0.000001f;

    // Update camera
    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov_, 1.0f,
                              min_exposure_, max_exposure_);

    if (enable_video_update_) {
        video_time_us_ += fr_info_.delta_time_us;
    }

    // log_->Info("%f %f %f | %f %f %f", view_origin_[0], view_origin_[1],
    // view_origin_[2],
    //           view_dir_[0], view_dir_[1], view_dir_[2]);
}

void GSVideoTest::SaveScene(JsObjectP &js_scene) {
    GSBaseState::SaveScene(js_scene);

    const auto &alloc = js_scene.elements.get_allocator();

    { // write camera
        JsObjectP js_camera(alloc);

        { // write view origin
            JsArrayP js_view_origin(alloc);

            js_view_origin.Push(JsNumber{double(initial_view_pos_[0])});
            js_view_origin.Push(JsNumber{double(initial_view_pos_[1])});
            js_view_origin.Push(JsNumber{double(initial_view_pos_[2])});

            js_camera.Insert("view_origin", std::move(js_view_origin));
        }

        { // write view direction
            JsArrayP js_view_dir(alloc);

            js_view_dir.Push(JsNumber{double(initial_view_dir_[0])});
            js_view_dir.Push(JsNumber{double(initial_view_dir_[1])});
            js_view_dir.Push(JsNumber{double(initial_view_dir_[2])});

            js_camera.Insert("view_dir", std::move(js_view_dir));
        }

        { // write forward speed
            js_camera.Insert("fwd_speed", JsNumber{double(max_fwd_speed_)});
        }

        { // write fov
            js_camera.Insert("fov", JsNumber{double(view_fov_)});
        }

        { // write max exposure
            js_camera.Insert("max_exposure", JsNumber{double(max_exposure_)});
        }

        js_scene.Insert("camera", std::move(js_camera));
    }
}

void GSVideoTest::UpdateVideoTextures() {
    using namespace GSVideoTestInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_vid_tex_str);

    for (int tx = 0; tx < 5; tx++) {
        if (!vp_[tx].initialized()) {
            continue;
        }

#ifdef UPDATE_PBO_FROM_SEPARATE_THREAD
        if (vid_update_done_[tx].valid()) {
#ifdef FORCE_WAIT_FOR_DECODER
            vid_update_done_[tx].wait();
#else
            if (vid_update_done_[tx].wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                // Video update iteration did not finish yet
                continue;
            }
#endif
#ifdef UPDATE_TEX_FROM_SEPARATE_CONTEXT
            if (tex_update_done_[tx].valid()) {
                tex_update_done_[tx].wait();
            }
#endif
        }
#endif

        const auto decode_result = decode_result_[tx];

        // Request new frame
        /*const int q_index = tx % decoder_threads_->queue_count();
        decoder_threads_->Enqueue(q_index, [this, tx]() {
            using namespace GSVideoTestInternal;

            __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_decode_str);

            decode_result_[tx] = vp_[tx].UpdateFrame(video_time_us_ / 1000);

            __itt_task_end(__g_itt_domain);
        });*/

#ifndef FORCE_UPLOAD_EVERY_FRAME
        if (decode_result == VideoPlayer::eFrUpdateResult::Updated)
#endif
        {
            cur_frame_index_[tx] = (cur_frame_index_[tx] + 1) % TextureSyncWindow;
        }

        const int tex_to_render = cur_frame_index_[tx];
        const int tex_to_update = (cur_frame_index_[tx] + 1) % TextureSyncWindow;

#if !defined(UPDATE_PBO_FROM_SEPARATE_THREAD) || !defined(UPDATE_TEX_FROM_SEPARATE_CONTEXT)

#ifndef FORCE_UPLOAD_EVERY_FRAME
        if (decode_result == VideoPlayer::eFrUpdateResult::Updated)
#endif
        {
            UpdateVideoTextureData(tx, tex_to_render);
            SetVideoTextureFence(tx, tex_to_render);
        }
#endif

        // Switch material textures
        vid_mat_[tx]->textures[0] = y_tex_[tx][tex_to_render];
        vid_mat_[tx]->textures[1] = uv_tex_[tx][tex_to_render];

        vid_mat_[tx]->textures[0]->first_user = vid_mat_[tx].index();
        vid_mat_[tx]->textures[1]->first_user = vid_mat_[tx].index();

        vid_mat_[tx]->next_texture_user.resize(2);
        vid_mat_[tx]->next_texture_user[0] = 0xffffffff;
        vid_mat_[tx]->next_texture_user[1] = 0xffffffff;

        scene_manager_->InvalidateTexture(vid_mat_[tx]->textures[0]);
        scene_manager_->InvalidateTexture(vid_mat_[tx]->textures[1]);

        // Make sure PBO -> texture transfer finished
        WaitVideoTextureUpdated(tx, tex_to_update);

        // Now we are sure that PBO is no longer in use
#ifdef UPDATE_PBO_FROM_SEPARATE_THREAD
        if (ren_ctx_->capabilities.persistent_buf_mapping) {
#else
        if (false) {
#endif
            // Can update PBO from decoder thread
            /*vid_update_done_[tx] = decoder_threads_->Enqueue(q_index, [this, tx, tex_to_update]() {
#ifndef FORCE_UPLOAD_EVERY_FRAME
                if (decode_result_[tx] != VideoPlayer::eFrUpdateResult::Updated) {
                    return;
                }
#endif
                UpdateStageBufWithDecodedFrame_Persistent(tx, tex_to_update);

#ifdef UPDATE_TEX_FROM_SEPARATE_CONTEXT
                tex_update_done_[tx] = aux_gfx_thread_->AddTask([this, tx, tex_to_update]() {
                    UpdateVideoTextureData(tx, tex_to_update);
                    SetVideoTextureFence(tx, tex_to_update);
                    // We have to flush command buffer here to make sure
                    // fences will be visible in main gfx thread
                    FlushGPUCommands();
                });
#endif
            });*/
        } else {
            // Have to update PBO from main thread
            UpdateStageBufWithDecodedFrame(tx, tex_to_update);
            // No reason to wait, update texture here too
            UpdateVideoTextureData(tx, tex_to_update);
        }
    }

    __itt_task_end(__g_itt_domain);
}

void GSVideoTest::UpdateVideoTextureData(const int tex_index, const int frame_index) {
    using namespace GSVideoTestInternal;

    assert(vp_[tex_index].initialized());

    if (GSVideoTestInternal::VerboseLogging) { // NOLINT
        log_->Info("Updating texture %tx", frame_index);
    }

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_update_tex_str);

    const int tex_w = vp_[tex_index].w();
    const int tex_h = vp_[tex_index].h();

    const uint32_t y_buf_chunk_size = tex_w * tex_h, uv_buf_chunk_size = 2 * (tex_w / 2) * (tex_h / 2);

    // y_sbuf_[tex_index].FlushMappedRange(frame_index * y_buf_chunk_size, y_buf_chunk_size);
    y_tex_[tex_index][frame_index]->SetSubImage(0, 0 /* offsetx */, 0 /* offsety */, tex_w, tex_h,
                                                Ren::eTexFormat::RawR8, y_sbuf_[tex_index], ren_ctx_->current_cmd_buf(),
                                                frame_index * y_buf_chunk_size, y_buf_chunk_size);

    // uv_sbuf_[tex_index].FlushMappedRange(frame_index * uv_buf_chunk_size, uv_buf_chunk_size);
    uv_tex_[tex_index][frame_index]->SetSubImage(
        0, 0 /* offsetx */, 0 /* offsety */, tex_w / 2, tex_h / 2, Ren::eTexFormat::RawRG88, uv_sbuf_[tex_index],
        ren_ctx_->current_cmd_buf(), frame_index * uv_buf_chunk_size, uv_buf_chunk_size);

    __itt_task_end(__g_itt_domain);
}

void GSVideoTest::UpdateStageBufWithDecodedFrame(const int tex_index, const int frame_index) {
    assert(vp_[tex_index].initialized());

    const int tex_w = vp_[tex_index].w();
    const int tex_h = vp_[tex_index].h();

    { // copy Y plane
        int w, h, stride;
        const uint8_t *y_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::Y, w, h, stride);
        if (y_img && w == tex_w && h == tex_h) {
            if (y_sbuf_[tex_index].mapped_ptr()) { // persistent mapping case
                const int range_offset = frame_index * w * h;
                for (int y = 0; y < h; y++) {
                    memcpy(y_sbuf_[tex_index].mapped_ptr() + (range_offset + y * w), &y_img[y * stride], w);
                }
            } else { // non-persistent mapping case
                uint8_t *pinned_mem = y_sbuf_[tex_index].MapRange(frame_index * w * h, w * h);
                if (pinned_mem) {
                    for (int y = 0; y < h; y++) {
                        memcpy(&pinned_mem[y * w], &y_img[y * stride], w);
                    }
                    y_sbuf_[tex_index].Unmap();
                }
            }
        }
    }

    { // copy UV planes
        int u_w, u_h, u_stride;
        const uint8_t *u_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::U, u_w, u_h, u_stride);
        int v_w, v_h, v_stride;
        const uint8_t *v_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::V, v_w, v_h, v_stride);
        if (u_img && u_w == (tex_w / 2) && u_h == (tex_h / 2) && v_img && v_w == (tex_w / 2) && v_h == (tex_h / 2)) {
            const int range_offset = 2 * frame_index * u_w * u_h;

            if (uv_sbuf_[tex_index].mapped_ptr()) { // persistent mapping case
                uint8_t *uv_dst = uv_sbuf_[tex_index].mapped_ptr() + range_offset;

                Ren::InterleaveUVChannels_16px(u_img, v_img, u_stride, v_stride, u_w, u_h, uv_dst);
            } else { // non-persistent mapping case
                uint8_t *pinned_mem = uv_sbuf_[tex_index].MapRange(range_offset, 2 * u_w * u_h);
                if (pinned_mem) {
                    Ren::InterleaveUVChannels_16px(u_img, v_img, u_stride, v_stride, u_w, u_h, pinned_mem);

                    uv_sbuf_[tex_index].Unmap();
                }
            }
        }
    }
}

void GSVideoTest::UpdateStageBufWithDecodedFrame_Persistent(const int tex_index, const int frame_index) {
    using namespace GSVideoTestInternal;

    assert(y_sbuf_[tex_index].mapped_ptr() && uv_sbuf_[tex_index].mapped_ptr() &&
           "Persistent mapping must be available!");

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_copy_pbo_str);

    const int tex_w = vp_[tex_index].w();
    const int tex_h = vp_[tex_index].h();

    { // copy Y plane
        int w, h, stride;
        const uint8_t *y_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::Y, w, h, stride);
        if (y_img && w == tex_w && h == tex_h) {
            const int range_offset = frame_index * w * h;
            uint8_t *y_out = y_sbuf_[tex_index].mapped_ptr() + range_offset;

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
            if (w % 32 == 0 && Ren::g_CpuFeatures.avx_supported) {
                Ren::CopyYChannel_32px(y_img, stride, w, h, y_out);
            } else if (w % 16 == 0 && Ren::g_CpuFeatures.sse2_supported) {
                Ren::CopyYChannel_16px(y_img, stride, w, h, y_out);
            } else
#endif
            {
                for (int y = 0; y < h; y++) {
                    memcpy(&y_out[y * w], &y_img[y * stride], w);
                }
            }
        }
    }

    { // copy UV planes
        int u_w, u_h, u_stride;
        const uint8_t *u_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::U, u_w, u_h, u_stride);
        int v_w, v_h, v_stride;
        const uint8_t *v_img = vp_[tex_index].GetImagePtr(Eng::eYUVComp::V, v_w, v_h, v_stride);
        if (u_img && u_w == (tex_w / 2) && u_h == (tex_h / 2) && v_img && v_w == (tex_w / 2) && v_h == (tex_h / 2)) {
            const int range_offset = 2 * frame_index * u_w * u_h;
            uint8_t *uv_out = uv_sbuf_[tex_index].mapped_ptr() + range_offset;

            if (u_w % 16 == 0 && Ren::g_CpuFeatures.sse2_supported) {
                Ren::InterleaveUVChannels_16px(u_img, v_img, u_stride, v_stride, u_w, u_h, uv_out);
            } else {
                for (int y = 0; y < u_h; ++y) {
                    for (int x = 0; x < u_w; ++x) {
                        uv_out[2 * (y * u_w + x) + 0] = u_img[y * u_stride + x];
                        uv_out[2 * (y * u_w + x) + 1] = v_img[y * v_stride + x];
                    }
                }
            }
        }
    }

    __itt_task_end(__g_itt_domain);
}
