#include "GSUITest3.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Eng/Gui/Image.h>
#include <Eng/Gui/Image9Patch.h>
#include <Eng/Gui/Renderer.h>
#include <Eng/Gui/Utils.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Gui/PagedReader.h"
#include "../Viewer.h"

namespace GSUITest3Internal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "book.json";

const Ren::Vec2f page_corners_uvs[] = {Ren::Vec2f{0.093528f, 0.507586f}, Ren::Vec2f{0.3968f, 0.97749f},
                                       Ren::Vec2f{0.611766f, 0.507587f}, Ren::Vec2f{0.915037f, 0.977491f},
                                       Ren::Vec2f{0.093529f, 0.022512f}, Ren::Vec2f{0.3968f, 0.492416f},
                                       Ren::Vec2f{0.611766f, 0.022511f}, Ren::Vec2f{0.915037f, 0.492415f}};

const Ren::Vec3f page_corners_pos[] = {
    Ren::Vec3f{0.378295f, 0.065051f, 0.525073f},
    Ren::Vec3f{-0.369601f, 0.059627f, 0.04203f},
};

const int page_order_indices[][4] = {{}, {0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, -2, -1}};
} // namespace GSUITest3Internal

GSUITest3::GSUITest3(Eng::GameBase *game) : GSBaseState(game) {
    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    book_main_font_ = fonts->FindFont("book_main_font");
    book_emph_font_ = fonts->FindFont("book_emph_font");
    book_caption_font_ = fonts->FindFont("book_caption_font");
    book_caption_font_->set_scale(1.25f);
}

GSUITest3::~GSUITest3() = default;

void GSUITest3::Enter() {
    using namespace GSUITest3Internal;

    GSBaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    /*test_image_.reset(new Gui::Image{
        *ctx_, "assets_pc/textures/test_image.uncompressed.png", Ren::Vec2f{ -0.5f, -0.5f
    }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_frame_.reset(new Gui::Image9Patch{
        *ctx_, "assets_pc/textures/ui/frame_01.uncompressed.png", Ren::Vec2f{ 2.0f, 2.0f
    }, 1.0f, Ren::Vec2f{ 0.0f, 0.1f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });*/

#if defined(__ANDROID__)
    const char *book_name = "assets/scenes/test/test_book/test_book.json";
#else
    const char *book_name = "assets_pc/scenes/test/test_book/test_book.json";
#endif

    {
        JsObject js_book;

        { // Load book data from file
            Sys::AssetFile in_book(book_name);
            if (!in_book) {
                log_->Error("Can not open book file %s", book_name);
            } else {
                const size_t scene_size = in_book.size();

                std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
                in_book.Read((char *)&scene_data[0], scene_size);

                Sys::MemBuf mem(&scene_data[0], scene_size);
                std::istream in_stream(&mem);

                if (!js_book.Read(in_stream)) {
                    throw std::runtime_error("Cannot load dialog!");
                }
            }
        }

        {
            const auto page_root = Gui::RootElement{Ren::Vec2i{page_buf_.w, page_buf_.h}};
            paged_reader_.reset(new PagedReader{*ren_ctx_, Ren::Vec2f{-0.995f, -0.995f}, Ren::Vec2f{2.0f, 2.0f},
                                                &page_root, book_main_font_, book_emph_font_, book_caption_font_});

            paged_reader_->LoadBook(js_book, "en", "de");
        }
    }

    page_renderer_.reset(new Gui::Renderer{*ren_ctx_});

    { // init page framebuffer
        FrameBuf::ColorAttachmentDesc attachment;
        attachment.format = Ren::eTexFormat::RawRGB888;
        attachment.filter = Ren::eTexFilter::BilinearNoMipmap;
        attachment.wrap = Ren::eTexWrap::ClampToEdge;

        page_buf_ = FrameBuf{"Page buf", *ren_ctx_, 3072, 3072, &attachment, 1, {}, 1, log_.get()};
    }

    InitBookMaterials();

    const Eng::SceneData &scene = scene_manager_->scene_data();

    /*if (book_index_ != 0xffffffff) {
        SceneObject *book = scene_manager_->GetObject(book_index_);

        const uint32_t mask = CompDrawableBit | CompAnimStateBit;
        if ((book->comp_mask & mask) == mask) {
            auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(
                book->components[CompDrawable]);

            Ren::Mesh *mesh = dr->mesh.get();

            for (const auto &grp : mesh->groups()) {
                // hold reference to original material here
                Ren::MaterialRef mat = grp.mat;
                if (mat->name() == "book/book_page0.txt") {
                    // replace material
                    const_cast<Ren::TriGroup &>(grp).mat = page_mat_;
                }
            }
        }
    }*/

    // disable bloom and fxaa, they make fonts look bad
    render_flags_ &= ~Eng::EnableBloom;
    render_flags_ &= ~Eng::EnableFxaa;
}

void GSUITest3::OnPostloadScene(JsObjectP &js_scene) {
    using namespace GSUITest3Internal;

    GSBaseState::OnPostloadScene(js_scene);

    view_dir_ = Ren::Vec3f{0.0f, 0.0f, 1.0f};
    view_fov_ = 45.0f;

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            view_origin_[0] = float(js_orig.at(0).as_num().val);
            view_origin_[1] = float(js_orig.at(1).as_num().val);
            view_origin_[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            view_dir_[0] = float(js_dir.at(0).as_num().val);
            view_dir_[1] = float(js_dir.at(1).as_num().val);
            view_dir_[2] = float(js_dir.at(2).as_num().val);
        }

        /*if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = float(js_fwd_speed.val);
        }*/

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov_ = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure_ = float(js_max_exposure.val);
        }
    }

    book_index_ = scene_manager_->FindObject("book");
}

void GSUITest3::UpdateAnim(const uint64_t dt_us) {
    using namespace GSUITest3Internal;

    GSBaseState::UpdateAnim(dt_us);

    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;

    const Eng::SceneData &scene = scene_manager_->scene_data();

    if (book_index_ != 0xffffffff) {
        Eng::SceneObject *book = scene_manager_->GetObject(book_index_);

        uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((book->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(book->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(book->components[Eng::CompAnimState]);

            const int cur_page = paged_reader_->cur_page(), page_count = paged_reader_->page_count();

            if (book_state_ == eBookState::BkClosed) {
                view_offset_ = 0.5f;
            } else if (book_state_ == eBookState::BkOpening) {
                if (as->anim_time_s > 1.925f) {
                    view_offset_ = 0.0f;
                    book_state_ = eBookState::BkOpened;
                } else {
                    view_offset_ = 0.5f - 0.25f * as->anim_time_s;
                }
            } else if (book_state_ == eBookState::BkTurningFwd) {
                if (as->anim_time_s > 0.925f) {
                    if (cur_page < page_count - 2) {
                        paged_reader_->set_cur_page(cur_page + 2);
                    }
                    book_state_ = eBookState::BkOpened;
                }
            } else if (book_state_ == eBookState::BkTurningBck) {
                if (as->anim_time_s > 0.925f) {
                    if (cur_page >= 2) {
                        paged_reader_->set_cur_page(cur_page - 2);
                    }
                    book_state_ = eBookState::BkOpened;
                }
            }

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int book_anim_index = int(book_state_);

            skel->UpdateAnim(book_anim_index, as->anim_time_s);
            skel->ApplyAnim(book_anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }
}

void GSUITest3::Exit() { GSBaseState::Exit(); }

void GSUITest3::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITest3Internal;

    if (hit_point_screen_.initialized()) {
        paged_reader_->DrawHint(r, hit_point_screen_.GetValue() + Ren::Vec2f{0.0f, 0.05f}, root);
    }

    GSBaseState::DrawUI(r, root);
}

void GSUITest3::Draw() {
    using namespace GSUITest3Internal;

    if (book_state_ != eBookState::BkClosed) {
        if (book_state_ == eBookState::BkOpened) {
            if (hit_point_ndc_.initialized()) {
                auto page_root = Gui::RootElement{Ren::Vec2i{page_buf_.w, page_buf_.h}};

                const int page_base = paged_reader_->cur_page();
                for (int i = 0; i < 2; i++) {
                    paged_reader_->set_cur_page(page_base + page_order_indices[size_t(book_state_)][i]);

                    paged_reader_->Resize(2.0f * page_corners_uvs[i * 2] - Ren::Vec2f{1.0f},
                                          2.0f * (page_corners_uvs[i * 2 + 1] - page_corners_uvs[i * 2]), &page_root);
                    paged_reader_->Press(hit_point_ndc_.GetValue(), true);
                    if (paged_reader_->selected_sentence() != -1) {
                        break;
                    }
                }
                paged_reader_->set_cur_page(page_base + page_order_indices[size_t(book_state_)][0]);

                hit_point_ndc_.destroy();
            }
        }
        RedrawPages(page_renderer_.get());
    }

    auto up_vector = Ren::Vec3f{0.0f, 1.0f, 0.0f};
    if (Length2(Cross(view_dir_, up_vector)) < 0.001f) {
        up_vector = Ren::Vec3f{-1.0f, 0.0f, 0.0f};
    }

    const Ren::Vec3f view_origin = view_origin_ + Ren::Vec3f{0.0f, view_offset_, 0.0f};

    scene_manager_->SetupView(view_origin, (view_origin + view_dir_), up_vector, view_fov_, true, max_exposure_);

    GSBaseState::Draw();
}

bool GSUITest3::HandleInput(const Eng::InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSUITest3Internal;

    // pt switch for touch controls
    if (evt.type == Eng::RawInputEv::P1Down || evt.type == Eng::RawInputEv::P2Down) {
        if (evt.point.x > float(ren_ctx_->w()) * 0.9f && evt.point.y < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
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
    case Eng::RawInputEv::P1Down: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (book_state_ == eBookState::BkOpened) {
        }
    } break;
    case Eng::RawInputEv::P2Down: {

    } break;
    case Eng::RawInputEv::P1Up: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});

        bool blocked = false;

        if (book_state_ == eBookState::BkOpened) {
            const int cur_page = paged_reader_->cur_page(), page_count = paged_reader_->page_count();

            if (p[0] < 0.0f) {
                if (cur_page - 2 >= 0) {
                    // paged_reader_->set_cur_page(cur_page - 2);
                } else {
                    blocked = true;
                }
            } else {
                if (cur_page + 2 < page_count) {
                    // paged_reader_->set_cur_page(cur_page + 2);
                } else {
                    blocked = true;
                }
            }

            hit_point_ndc_ = MapPointToPageFramebuf(p);
            if (hit_point_ndc_.initialized()) {
                hit_point_screen_ = p;
                blocked = true;
            }
        }

        if (!blocked) {
            bool reset_anim_time = false;
            if (book_state_ == eBookState::BkClosed) {
                book_state_ = eBookState::BkOpening;
                reset_anim_time = true;
            } else if (book_state_ == eBookState::BkOpened) {
                if (p[0] >= 0.0f) {
                    book_state_ = eBookState::BkTurningFwd;
                } else {
                    book_state_ = eBookState::BkTurningBck;
                }
                reset_anim_time = true;
            }

            if (reset_anim_time) {
                const Eng::SceneData &scene = scene_manager_->scene_data();
                Eng::SceneObject *book = scene_manager_->GetObject(book_index_);

                const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
                if ((book->comp_mask & mask) == mask) {
                    auto *as = (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(
                        book->components[Eng::CompAnimState]);
                    as->anim_time_s = 0.0f;
                }
            }
        }
    } break;
    case Eng::RawInputEv::P2Up: {
    } break;
    case Eng::RawInputEv::P1Move: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        paged_reader_->Hover(p);

        hit_point_screen_.destroy();
    } break;
    case Eng::RawInputEv::P2Move: {

    } break;
    case Eng::RawInputEv::KeyDown: {
        input_processed = false;
    } break;
    case Eng::RawInputEv::KeyUp: {
        if (evt.key_code == Eng::KeyUp || (evt.key_code == Eng::KeyW && !cmdline_enabled_)) {
            // text_printer_->restart();
            paged_reader_->set_cur_page(0);
        } else {
            input_processed = false;
        }
    } break;
    case Eng::RawInputEv::Resize:
        // paged_reader_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}

Sys::Optional<Ren::Vec2f> GSUITest3::MapPointToPageFramebuf(const Ren::Vec2f &p) {
    using namespace GSUITest3Internal;
    using namespace Ren;

    const Camera &cam = scene_manager_->main_cam();

    const Mat4f &clip_from_view = cam.proj_matrix(), &view_from_world = cam.view_matrix();

    const Mat4f clip_from_world = clip_from_view * view_from_world, world_from_clip = Inverse(clip_from_world);

    auto ray_beg_cs = Vec4f{p[0], p[1], -1.0f, 1.0f}, ray_end_cs = Vec4f{p[0], p[1], 1.0f, 1.0f};

    Vec4f ray_beg_ws = world_from_clip * ray_beg_cs, ray_end_ws = world_from_clip * ray_end_cs;
    ray_beg_ws /= ray_beg_ws[3];
    ray_end_ws /= ray_end_ws[3];

    const auto ray_origin_ws = Vec3f{ray_beg_ws}, ray_dir_ws = Normalize(Vec3f{ray_end_ws - ray_beg_ws});

    auto page_plane = Vec4f{0.0f, 1.0f, 0.0f, -page_corners_pos[0][1]};

    const float t = -(page_plane[3] + ray_origin_ws[1]) / ray_dir_ws[1];
    const Vec3f inter_point = ray_origin_ws + t * ray_dir_ws;

    Sys::Optional<Ren::Vec2f> ret;

    if (inter_point[0] < page_corners_pos[0][0] && inter_point[0] > page_corners_pos[1][0] &&
        inter_point[2] < page_corners_pos[0][2] && inter_point[2] > page_corners_pos[1][2]) {

        Vec2f inter_point_norm =
            Vec2f{inter_point[0] - page_corners_pos[1][0], inter_point[2] - page_corners_pos[1][2]};
        inter_point_norm /=
            Vec2f{page_corners_pos[0][0] - page_corners_pos[1][0], page_corners_pos[0][2] - page_corners_pos[1][2]};

        inter_point_norm = Vec2f{1.0f} - inter_point_norm;
        std::swap(inter_point_norm[0], inter_point_norm[1]);

        Vec2f inter_point_ndc = page_corners_uvs[0] + inter_point_norm * (page_corners_uvs[1] - page_corners_uvs[0]);
        inter_point_ndc = inter_point_ndc * 2.0f - Vec2f{1.0f};

        // inter_point_ndc = -inter_point_ndc;
        // std::swap(inter_point_ndc[0], inter_point_ndc[1]);

        log_->Info("Hit point NDC: %f %f", inter_point_ndc[0], inter_point_ndc[1]);
        ret = inter_point_ndc;
    } else {
        log_->Info("No hit!");
    }

    return ret;
}
