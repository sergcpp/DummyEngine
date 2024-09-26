#include "DebugFrameUI.h"

#include <bitset>

#include <Gui/BitmapFont.h>
#include <Ren/Context.h>
#include <Ren/HashMap32.h>
#include <Ren/ScopeExit.h>
#include <Sys/Time_.h>

#include "../input/InputManager.h"
#include "../renderer/Renderer_Structs.h"
#include "../scene/SceneData.h"

namespace Eng::DebugFrameUIInternal {
const Gui::Vec2f ElementSizePx = Gui::Vec2f{256, 384};
const Gui::Vec2f ElementSpacingPx = Gui::Vec2f{384, 200};
const float HeapScale = 1.0f / (32 * 1024.0f);

void insert_sorted(Ren::SmallVectorImpl<int16_t> &vec, const int16_t val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val);
    if (it == std::end(vec) || val < (*it)) {
        vec.insert(it, val);
    }
}

void insert_sorted_rev(Ren::SmallVectorImpl<int16_t> &vec, const int16_t val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val, std::greater<int16_t>());
    if (it == std::end(vec) || val > (*it)) {
        vec.insert(it, val);
    }
}

void remove_sorted_rev(Ren::SmallVectorImpl<int16_t> &vec, const int16_t val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val, std::greater<int16_t>());
    if (it != std::end(vec) && val == (*it)) {
        vec.erase(it);
    }
}
} // namespace Eng::DebugFrameUIInternal

Eng::DebugFrameUI::DebugFrameUI(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                                const BaseElement *parent, const Gui::BitmapFont *font_small,
                                const Gui::BitmapFont *font_large)
    : BaseElement(pos, size, parent), font_small_(font_small), font_large_(font_large),
      back_(ctx, "assets_pc/textures/internal/back.dds", Gui::Vec2f{1.5f}, 1.0f, Gui::Vec2f{-1}, Gui::Vec2f{2}, this),
      element_(ctx, "assets_pc/textures/internal/square.dds", Gui::Vec2f{1.5f}, 1.0f, Gui::Vec2f{-1}, Gui::Vec2f{2},
               nullptr),
      element_highlighted_(ctx, "assets_pc/textures/internal/square_highlighted.dds", Gui::Vec2f{1.5f}, 1.0f,
                           Gui::Vec2f{-1}, Gui::Vec2f{2}, nullptr),
      line_(ctx, "assets_pc/textures/internal/line.dds", Gui::Vec2f{}, Gui::Vec2f{}, nullptr) {}

void Eng::DebugFrameUI::UpdateInfo(const FrontendInfo &frontend_info, const BackendInfo &backend_info,
                                   const ItemsInfo &items_info, const bool debug_items) {
    const float alpha = 0.98f;
    const float k = (1.0f - alpha);

    auto us_to_ms = [](uint64_t v) -> float { return 0.001f * v; };

    debug_items_ = debug_items;

    front_info_smooth_.occluders_time_ms *= alpha;
    front_info_smooth_.occluders_time_ms += k * us_to_ms(frontend_info.occluders_time_us);
    front_info_smooth_.main_gather_time_ms *= alpha;
    front_info_smooth_.main_gather_time_ms += k * us_to_ms(frontend_info.main_gather_time_us);
    front_info_smooth_.shadow_gather_time_ms *= alpha;
    front_info_smooth_.shadow_gather_time_ms += k * us_to_ms(frontend_info.shadow_gather_time_us);
    front_info_smooth_.drawables_sort_time_ms *= alpha;
    front_info_smooth_.drawables_sort_time_ms += k * us_to_ms(frontend_info.drawables_sort_time_us);
    front_info_smooth_.items_assignment_time_ms *= alpha;
    front_info_smooth_.items_assignment_time_ms += k * us_to_ms(frontend_info.items_assignment_time_us);
    front_info_smooth_.total_time_ms *= alpha;
    front_info_smooth_.total_time_ms += k * us_to_ms(frontend_info.end_timepoint_us - frontend_info.start_timepoint_us);

    back_info_smooth_.passes_info.resize(backend_info.passes_info.size());
    for (int i = 0; i < int(backend_info.passes_info.size()); ++i) {
        if (back_info_smooth_.passes_info[i].name == backend_info.passes_info[i].name) {
            back_info_smooth_.passes_info[i].duration_ms *= alpha;
            back_info_smooth_.passes_info[i].duration_ms += k * us_to_ms(backend_info.passes_info[i].duration_us);
        } else {
            back_info_smooth_.passes_info[i].name = backend_info.passes_info[i].name;
            back_info_smooth_.passes_info[i].duration_ms = us_to_ms(backend_info.passes_info[i].duration_us);
        }
        back_info_smooth_.passes_info[i].input = backend_info.passes_info[i].input;
        back_info_smooth_.passes_info[i].output = backend_info.passes_info[i].output;
        back_info_smooth_.passes_info[i].position = {};
    }
    back_info_smooth_.resources_info.assign(std::begin(backend_info.resources_info),
                                            std::end(backend_info.resources_info));

    back_info_smooth_.cpu_total_ms *= alpha;
    back_info_smooth_.cpu_total_ms +=
        k * us_to_ms(backend_info.cpu_end_timepoint_us - backend_info.cpu_start_timepoint_us);

    back_info_smooth_.gpu_total_ms *= alpha;
    back_info_smooth_.gpu_total_ms += k * us_to_ms(backend_info.gpu_total_duration);

    items_info_smooth_.lights_count *= alpha;
    items_info_smooth_.lights_count += k * items_info.lights_count;
    items_info_smooth_.decals_count *= alpha;
    items_info_smooth_.decals_count += k * items_info.decals_count;
    items_info_smooth_.probes_count *= alpha;
    items_info_smooth_.probes_count += k * items_info.probes_count;
    items_info_smooth_.items_total *= alpha;
    items_info_smooth_.items_total += k * items_info.items_total;

    prev_timing_info_ = cur_timing_info_;
    cur_timing_info_.front_start_timepoint_us = frontend_info.start_timepoint_us;
    cur_timing_info_.front_end_timepoint_us = frontend_info.end_timepoint_us;
    cur_timing_info_.back_cpu_start_timepoint_us = backend_info.cpu_start_timepoint_us;
    cur_timing_info_.back_cpu_end_timepoint_us = backend_info.cpu_end_timepoint_us;
    cur_timing_info_.back_gpu_duration = backend_info.gpu_total_duration;
    cur_timing_info_.gpu_cpu_time_diff_us = backend_info.gpu_cpu_time_diff_us;
}

bool Eng::DebugFrameUI::HandleInput(const Gui::input_event_t &ev, const std::vector<bool> &keys_state) {
    if (back_info_smooth_.resources_info.empty()) {
        return false;
    }

    bool handled = false;
    if (ev.type == eInputEvent::P1Down || ev.type == eInputEvent::P2Down) {
        if (Check(Gui::Vec2i(ev.point))) {
            p1_down_pos_ = ev.point;
            view_grabbed_ = true;
            handled = true;
        }
    } else if (ev.type == eInputEvent::P1Up || ev.type == eInputEvent::P2Up) {
        if (ev.type == eInputEvent::P1Up && Distance(p1_down_pos_, ev.point) < 0.5f) {
            deferred_select_pos_ = Gui::Vec2i(ev.point);
        }
        p1_down_pos_ = Gui::Vec2f{-1};
        handled = view_grabbed_;
        view_grabbed_ = false;
    } else if (ev.type == eInputEvent::P1Move) {
        if (view_grabbed_) {
            view_offset_ += 2 * ev.move / Gui::Vec2f(dims_px_[1]);
            handled = true;
        }
    } else if (ev.type == eInputEvent::MouseWheel) {
        if (Check(Gui::Vec2i(ev.point))) {
            const Gui::Vec2f pivot1 = (ToLocal(Gui::Vec2i(ev.point)) - view_offset_) / view_scale_;
            view_scale_ = std::min(std::max(std::round(20 * view_scale_ + ev.move[0]) / 20, 0.05f), 100.0f);
            const Gui::Vec2f pivot2 = (ToLocal(Gui::Vec2i(ev.point)) - view_offset_) / view_scale_;
            view_offset_ += (pivot2 - pivot1) * view_scale_;
            handled = true;
        }
    } else if (ev.type == eInputEvent::MButtonDown) {
        if (Check(Gui::Vec2i(ev.point))) {
            const Gui::Vec2f pivot1 = (ToLocal(Gui::Vec2i(ev.point)) - view_offset_) / view_scale_;
            view_scale_ = 1;
            const Gui::Vec2f pivot2 = (ToLocal(Gui::Vec2i(ev.point)) - view_offset_) / view_scale_;
            view_offset_ += (pivot2 - pivot1) * view_scale_;
        }
    }

    if (handled) {
        return true;
    }

    return BaseElement::HandleInput(ev, keys_state);
}

void Eng::DebugFrameUI::Draw(Gui::Renderer *r) {
    if (!back_info_smooth_.resources_info.empty()) {
        DrawDetailed(r);
    } else if (!back_info_smooth_.passes_info.empty()) {
        DrawCompact(r);
    }
}

void Eng::DebugFrameUI::DrawCompact(Gui::Renderer *r) {
    const float font_scale = 0.5f;
    const float font_height = font_small_->height(font_scale, parent_);

    const char delimiter[] = "-------------------------------";
    char text_buffer[256];

    float vertical_offset = 0.75f;
    static const uint8_t text_color[4] = {255, 255, 255, 100};

    { // fps counter
        const uint64_t cur_frame_time = Sys::GetTimeUs();

        const double last_frame_dur = double(cur_frame_time - last_frame_time_) * 0.001;

        last_frame_time_ = cur_frame_time;

        const double alpha = 0.025;
        cur_frame_dur_ = alpha * last_frame_dur + (1.0 - alpha) * cur_frame_dur_;

        snprintf(text_buffer, sizeof(text_buffer), "                FPS: %.1f", (1000.0 / cur_frame_dur_));
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);
    }

    { // renderer frontend performance
        vertical_offset -= font_height;
        font_small_->DrawText(r, delimiter, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "     OCCLUDERS RAST: %.3f ms",
                 front_info_smooth_.occluders_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "        MAIN GATHER: %.3f ms",
                 front_info_smooth_.main_gather_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      SHADOW GATHER: %.3f ms",
                 front_info_smooth_.shadow_gather_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "            SORTING: %.3f ms",
                 front_info_smooth_.drawables_sort_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "        ITEM ASSIGN: %.3f ms",
                 front_info_smooth_.items_assignment_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "        FRONT TOTAL: %.3f ms", front_info_smooth_.total_time_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);
    }

    { // renderer backend performance
        vertical_offset -= font_height;
        font_small_->DrawText(r, delimiter, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        for (int i = 0; i < int(back_info_smooth_.passes_info.size()); ++i) {
            vertical_offset -= font_height;
            snprintf(text_buffer, sizeof(text_buffer), " %18s: %.3f ms", back_info_smooth_.passes_info[i].name.c_str(),
                     back_info_smooth_.passes_info[i].duration_ms);
            font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);
        }

        vertical_offset -= font_height;
        font_small_->DrawText(r, delimiter, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "     BACK CPU TOTAL: %.3f ms", back_info_smooth_.cpu_total_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "     BACK GPU TOTAL: %.3f ms", back_info_smooth_.gpu_total_ms);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);
    }

    if (debug_items_) {
        vertical_offset -= font_height;
        font_small_->DrawText(r, delimiter, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "       LIGHTS COUNT: %.3f", items_info_smooth_.lights_count);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "        LIGHTS DATA: %.3f kb",
                 items_info_smooth_.lights_count * sizeof(LightItem) / 1024.0f);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "       DECALS COUNT: %.3f", items_info_smooth_.decals_count);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "        DECALS DATA: %.3f kb",
                 items_info_smooth_.decals_count * sizeof(DecalItem) / 1024.0f);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "         CELLS DATA: %.3f kb",
                 ITEM_CELLS_COUNT * sizeof(CellData) / 1024.0f);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "         ITEMS DATA: %.3f kb",
                 items_info_smooth_.items_total * sizeof(ItemData) / 1024.0f);
        font_small_->DrawText(r, text_buffer, Gui::Vec2f{-1, vertical_offset}, text_color, font_scale, parent_);
    }
}

void Eng::DebugFrameUI::DrawDetailed(Gui::Renderer *r) {
    using namespace DebugFrameUIInternal;

    // background
    Gui::BaseElement::Draw(r);

    if (deferred_select_pos_) {
        selected_pass_index_ = -1;
        selected_res_index_ = -1;
    }

    { // passes
        r->PushClipArea(dims_);
        SCOPE_EXIT({ r->PopClipArea(); })

        Ren::SmallVector<int16_t, 128> pass_stack;
        for (int i = int(back_info_smooth_.passes_info.size()) - 1; i >= 0; --i) {
            pass_stack.push_back(int16_t(i));
        }

        if (pass_stack.empty()) {
            return;
        }

        std::vector<Ren::SmallVector<int16_t, 16>> chains;
        std::vector<bool> visited(back_info_smooth_.passes_info.size(), false);
        while (!pass_stack.empty()) {
            chains.emplace_back();
            chains.back().emplace_back(pass_stack.back());
            pass_stack.pop_back();

            for (int i = 0; i < int(chains.back().size()); ++i) {
                const auto &pass = back_info_smooth_.passes_info[chains.back()[i]];
                visited[chains.back()[i]] = true;

                int final_next = -1, final_score = 0;
                for (int j = chains.back()[i] + 1;
                     j < int(back_info_smooth_.passes_info.size()) /*&& final_next == -1*/; ++j) {
                    if (visited[j]) {
                        continue;
                    }
                    const auto &next = back_info_smooth_.passes_info[j];

                    int score = 0;
                    for (const auto &in_output : pass.output) {
                        for (const auto &input : next.input) {
                            if (input == in_output) {
                                score += (1 + j - i);
                                break;
                            }
                        }
                        for (const auto &output : next.output) {
                            if (output == in_output) {
                                score += (1 + j - i);
                                break;
                            }
                        }
                    }
                    if (score > final_score) {
                        final_next = j;
                        final_score = score;
                    }
                }

                if (final_next != -1) {
                    visited[final_next] = true;
                    insert_sorted(chains.back(), int16_t(final_next));
                    remove_sorted_rev(pass_stack, int16_t(final_next));
                }
            }
        }

        std::sort(begin(chains), end(chains),
                  [](const Ren::SmallVectorImpl<int16_t> &lhs, const Ren::SmallVectorImpl<int16_t> &rhs) {
                      return lhs.size() > rhs.size();
                  });

        int passes_total = 0;
        for (const auto &chain : chains) {
            passes_total += int(chain.size());
        }
        assert(passes_total == back_info_smooth_.passes_info.size());

        const Gui::Vec2f spacing = view_scale_ * 2 * ElementSpacingPx / Gui::Vec2f{size_px()};

        const float font_scale_small = 0.5f * view_scale_;
        const float font_height_small = font_small_->height(font_scale_small, this);
        const float font_scale_large = 8 * view_scale_;
        const float font_height_large = font_large_->height(font_scale_large, this);

        const Gui::Vec2f line_width_thick = Gui::Vec2f{1, aspect()} * 4 / Gui::Vec2f{size_px()};
        const Gui::Vec2f line_width_thin = Gui::Vec2f{1, aspect()} * 2 / Gui::Vec2f{size_px()};

        Gui::Vec2f bbox_min = {FLT_MAX}, bbox_max = {-FLT_MAX};

        std::vector<std::bitset<256>> is_occupied(chains.size());
        for (int ndx = 0; ndx < int(chains.size()); ++ndx) {
            const auto &chain = chains[ndx];

            int row = 0;
            while (row < int(chains.size())) {
                bool accept = true;
                for (int i = int(chain.front()); i <= int(chain.back()); ++i) {
                    accept &= !is_occupied[row][i];
                }
                if (accept) {
                    break;
                }
                ++row;
            }

            for (int i = int(chain.front()); i <= int(chain.back()); ++i) {
                is_occupied[ndx].set(i);
            }

            for (const int16_t i : chain) {
                const Gui::Vec2f elem_pos =
                    SnapToPixels(view_offset_ + Gui::Vec2f{spacing[0] * float(i), spacing[1] * float(-row)});
                bbox_min = Min(bbox_min, elem_pos);
                bbox_max = Max(bbox_max, elem_pos);
                DrawPassInfo(r, i, elem_pos, font_scale_small);
                back_info_smooth_.passes_info[i].position = elem_pos;
            }
        }

        for (int i = 0; i < int(back_info_smooth_.passes_info.size()); ++i) {
            DrawConnectionCurves(r, i, font_scale_small, i == selected_pass_index_);
        }

        const Gui::Vec2f bounds_min = bbox_min - 2 * view_scale_ * ElementSizePx / Gui::Vec2f(dims_px_[1]),
                         bounds_max = bbox_max + 4 * view_scale_ * ElementSizePx / Gui::Vec2f(dims_px_[1]);

        // prepare resources
        Ren::SmallVector<int16_t, 256> resources_per_heap[2];
        uint32_t heap_sizes[2] = {};
        for (int i = 0; i < int(back_info_smooth_.resources_info.size()); ++i) {
            const auto &res = back_info_smooth_.resources_info[i];
            resources_per_heap[res.heap].push_back(int16_t(i));
            heap_sizes[res.heap] = std::max(heap_sizes[res.heap], res.offset + res.size);
        }

        const uint32_t heap_total = heap_sizes[0] + heap_sizes[1];
        const float heap_size_y = view_scale_ * HeapScale * float(heap_total) / float(dims_px_[1][1]);

        // draw grid
        for (int i = 0; i < int(back_info_smooth_.passes_info.size()) + 1; ++i) {
            const float x_offset = view_scale_ *
                                   (2 * ElementSpacingPx[0] * float(i) - (ElementSpacingPx[0] - ElementSizePx[0])) /
                                   float(dims_px_[1][0]);
            static const uint8_t ColorBlack[4] = {0, 0, 0, 25};
            DrawLine(r, Gui::Vec2f{bbox_min[0] + x_offset, bounds_min[1] - heap_size_y},
                     Gui::Vec2f{bbox_min[0] + x_offset, bounds_max[1]}, line_width_thin, ColorBlack);
        }

        // draw resources
        Gui::Vec2f origin = Gui::Vec2f{bbox_min[0], bounds_min[1] - heap_size_y};
        for (int j = 0; j < 2; ++j) {
            const auto &resources = resources_per_heap[j];
            for (const int16_t i : resources) {
                DrawResourceInfo(r, i, origin, font_height_small);
            }
            origin[1] += view_scale_ * HeapScale * float(heap_sizes[j]) / float(dims_px_[1][1]);
            if (j != 1) {
                DrawLine(r, Gui::Vec2f{bounds_min[0], origin[1]}, Gui::Vec2f{bounds_max[0], origin[1]}, line_width_thin,
                         Gui::ColorBlack);
            }
        }

        { // draw bounds
            DrawLine(r, Gui::Vec2f{bounds_min[0], bounds_min[1] - heap_size_y},
                     Gui::Vec2f{bounds_min[0], bounds_max[1]}, line_width_thick, Gui::ColorBlack);
            DrawLine(r, bounds_min, Gui::Vec2f{bounds_max[0], bounds_min[1]}, line_width_thick, Gui::ColorBlack);
            DrawLine(r, Gui::Vec2f{bounds_min[0], bounds_max[1]}, bounds_max, line_width_thick, Gui::ColorBlack);
            DrawLine(r, Gui::Vec2f{bounds_max[0], bounds_min[1] - heap_size_y}, bounds_max, line_width_thick,
                     Gui::ColorBlack);
            DrawLine(r, Gui::Vec2f{bounds_min[0], bounds_min[1] - heap_size_y},
                     Gui::Vec2f{bounds_max[0], bounds_min[1] - heap_size_y}, line_width_thick, Gui::ColorBlack);
        }

        // draw labels
        font_large_->DrawText(r, "Passes", Gui::Vec2f{bounds_min[0], bounds_max[1] - 0.85f * font_height_large},
                              Gui::ColorWhite, font_scale_large, this);
        font_large_->DrawText(r, "Resources", Gui::Vec2f{bounds_min[0], bounds_min[1] - 0.85f * font_height_large},
                              Gui::ColorWhite, font_scale_large, this);
    }

    deferred_select_pos_ = {};
}

void Eng::DebugFrameUI::DrawPassInfo(Gui::Renderer *r, const int pass_index, const Gui::Vec2f elem_pos,
                                     const float font_scale) {
    using namespace DebugFrameUIInternal;

    const float font_height = font_small_->height(font_scale, this);

    Gui::Vec2f elem_size = view_scale_ * 2 * ElementSizePx / Gui::Vec2f{size_px()};
    const Gui::Vec2f elem_border = 8 / Gui::Vec2f{size_px()};

    const auto &pass = back_info_smooth_.passes_info[pass_index];
    int resource_count = 2 + int(pass.input.size());
    if (!pass.input.empty() && !pass.output.empty()) {
        resource_count += 2;
    }
    if (!pass.output.empty()) {
        resource_count += 1 + int(pass.output.size());
    }
    elem_size[1] = float(resource_count) * font_height;

    Gui::Image9Patch *el = (selected_pass_index_ == pass_index) ? &element_highlighted_ : &element_;
    if (selected_res_index_ != -1) {
        // highlight if pass uses selected resource
        const auto &res = back_info_smooth_.resources_info[selected_res_index_];
        for (int i = 0; i < int(pass.input.size()) && el != &element_highlighted_; ++i) {
            if (pass.input[i] == res.name) {
                el = &element_highlighted_;
            }
        }
        for (int i = 0; i < int(pass.output.size()) && el != &element_highlighted_; ++i) {
            if (pass.output[i] == res.name) {
                el = &element_highlighted_;
            }
        }
    }

    // main element
    el->set_parent(this);
    el->Resize(Gui::Vec2f{elem_pos[0], elem_pos[1] - elem_size[1]},
               Gui::Vec2f{elem_size[0], elem_size[1] - 2 * font_height});
    el->Draw(r);

    if (deferred_select_pos_ && el->Check(*deferred_select_pos_)) {
        selected_pass_index_ = pass_index;
    }

    el->Resize(Gui::Vec2f{elem_pos[0], elem_pos[1] - 2 * font_height}, Gui::Vec2f{elem_size[0], 2 * font_height});
    el->Draw(r);

    if (deferred_select_pos_ && el->Check(*deferred_select_pos_)) {
        selected_pass_index_ = pass_index;
    }

    if (font_scale > 0.125f) { // resources
        el->Resize(Gui::Vec2f{elem_pos[0], elem_pos[1] - elem_size[1]}, elem_size);
        r->PushClipArea(el->dims());
        SCOPE_EXIT({ r->PopClipArea(); })

        Gui::Vec2f text_pos = elem_pos + Gui::Vec2f{elem_border[0], -elem_border[1] - font_height};

        char text_buffer[256];
        snprintf(text_buffer, sizeof(text_buffer), "%s (%.3f ms)", pass.name.c_str(), pass.duration_ms);

        font_small_->DrawText(r, text_buffer, text_pos, Gui::ColorWhite, font_scale, this);

        if (!pass.input.empty()) {
            text_pos[1] -= 2 * font_height;
            for (int i = 0; i < int(pass.input.size()); ++i) {
                font_small_->DrawText(r, pass.input[i], text_pos, Gui::ColorCyan, font_scale, this);
                text_pos[1] -= font_height;
            }
        }

        if (!pass.output.empty()) {
            text_pos[1] -= 2 * font_height;
            for (int i = 0; i < int(pass.output.size()); ++i) {
                const std::string full_name = "------------------------------- " + pass.output[i];
                const float width = font_small_->GetWidth(full_name, font_scale, this);

                font_small_->DrawText(r, full_name,
                                      Gui::Vec2f{text_pos[0] + elem_size[0] - 2 * elem_border[0] - width, text_pos[1]},
                                      Gui::ColorRed, font_scale, this);
                text_pos[1] -= font_height;
            }
        }
    }
}

void Eng::DebugFrameUI::DrawResourceInfo(Gui::Renderer *r, const int res_index, const Gui::Vec2f origin,
                                         const float font_scale) {
    using namespace DebugFrameUIInternal;

    const auto &res = back_info_smooth_.resources_info[res_index];

    Gui::Image9Patch *el = (selected_res_index_ == res_index) ? &element_highlighted_ : &element_;

    const int first_use = res.first_use != -1 ? res.first_use : 0,
              last_use = res.last_use != -1 ? res.last_use + 1 : int(back_info_smooth_.passes_info.size());

    Gui::Vec2f elem_min = origin, elem_max = origin;
    elem_min[0] += view_scale_ *
                   (2 * ElementSpacingPx[0] * float(first_use) - (ElementSpacingPx[0] - ElementSizePx[0])) /
                   float(dims_px_[1][0]);
    elem_min[1] += view_scale_ * HeapScale * float(res.offset) / float(dims_px_[1][1]);

    elem_max[0] += view_scale_ *
                   (2 * ElementSpacingPx[0] * float(last_use) - (ElementSpacingPx[0] - ElementSizePx[0])) /
                   float(dims_px_[1][0]);
    elem_max[1] += view_scale_ * HeapScale * float(res.offset + res.size) / float(dims_px_[1][1]);

    // main element
    el->set_parent(this);
    el->Resize(elem_min, elem_max - elem_min);
    el->Draw(r);

    if (deferred_select_pos_ && el->Check(*deferred_select_pos_)) {
        selected_res_index_ = res_index;
    }

    r->PushClipArea(el->dims());
    SCOPE_EXIT({ r->PopClipArea(); })

    const float height = font_small_->height(0.5f, this);

    const std::string description = res.name + " (" + std::to_string(float(res.size) / (1024.0f * 1024.0f)) + " MB)";
    const float width = font_small_->GetWidth(description, 0.5f, this);
    font_small_->DrawText(r, description, 0.5f * (elem_min + elem_max - Gui::Vec2f{width, height}), Gui::ColorWhite,
                          0.5f, this);
}

void Eng::DebugFrameUI::DrawConnectionCurves(Gui::Renderer *r, const int pass_index, const float font_scale,
                                             const bool detailed_outputs) {
    using namespace DebugFrameUIInternal;

    const float font_height = font_small_->height(font_scale, this);
    const Gui::Vec2f line_thin = Gui::Vec2f{1, aspect()} * 2 / Gui::Vec2f{size_px()};
    const Gui::Vec2f line_thick = Gui::Vec2f{1, aspect()} * 4 / Gui::Vec2f{size_px()};

    Gui::Vec2f elem_size = view_scale_ * 2 * ElementSizePx / Gui::Vec2f{size_px()};
    const Gui::Vec2f elem_border = 8 / Gui::Vec2f{size_px()};

    const auto &pass = back_info_smooth_.passes_info[pass_index];
    if (pass.output.empty()) {
        return;
    }

    int resource_count = 2 + int(pass.input.size());
    if (!pass.input.empty() && !pass.output.empty()) {
        resource_count += 2;
    }
    if (!pass.output.empty()) {
        resource_count += 1 + int(pass.output.size());
    }
    elem_size[1] = float(resource_count) * font_height;

    Gui::Vec2f src_pos = pass.position + Gui::Vec2f{elem_size[0], -elem_border[1] - font_height};
    if (!pass.input.empty()) {
        src_pos[1] -= 2 * font_height;
        src_pos[1] -= font_height * pass.input.size();
    }
    src_pos[1] -= 2 * font_height;
    src_pos[1] += 0.25f * font_height;

    static const uint8_t ColorBlack1[4] = {0, 0, 0, 100};
    static const uint8_t ColorBlack2[4] = {0, 0, 0, 25};
    static const uint8_t ColorRed[4] = {255, 0, 0, 200};
    static const uint8_t ColorCyan[4] = {0, 255, 255, 200};

    Ren::SmallVector<int16_t, 64> next_indices;
    for (int i = 0; i < int(pass.output.size()); ++i) {
        for (int next_index = pass_index + 1; next_index < int(back_info_smooth_.passes_info.size()); ++next_index) {
            const auto &next = back_info_smooth_.passes_info[next_index];
            bool found = false;
            for (int j = 0; j < int(next.input.size()) && !found; ++j) {
                if (next.input[j] == pass.output[i]) {
                    if (detailed_outputs || next_index == selected_pass_index_) {
                        Gui::Vec2f dst_pos = next.position + Gui::Vec2f{0, -elem_border[1] - font_height};
                        dst_pos[1] -= 2 * font_height;
                        dst_pos[1] -= font_height * float(j);
                        dst_pos[1] += 0.25f * font_height;

                        const bool backward = (next_index == selected_pass_index_);
                        DrawCurve(r, src_pos, src_pos + Gui::Vec2f{0.15f * font_scale, 0},
                                  dst_pos - Gui::Vec2f{0.15f * font_scale, 0}, dst_pos, line_thick,
                                  backward ? ColorCyan : ColorRed, backward);
                    }
                    insert_sorted(next_indices, int16_t(next_index));
                    found = true;
                }
            }
            for (int j = 0; j < int(next.output.size()) && !found; ++j) {
                if (next.output[j] == pass.output[i]) {
                    if (detailed_outputs || next_index == selected_pass_index_) {
                        Gui::Vec2f dst_pos = next.position + Gui::Vec2f{0, -elem_border[1] - font_height};
                        if (!next.input.empty()) {
                            dst_pos[1] -= 2 * font_height;
                            dst_pos[1] -= font_height * next.input.size();
                        }
                        dst_pos[1] -= 2 * font_height;
                        dst_pos[1] -= font_height * float(j);
                        dst_pos[1] += 0.25f * font_height;

                        const bool backward = (next_index == selected_pass_index_);
                        DrawCurve(r, src_pos, src_pos + Gui::Vec2f{0.15f * font_scale, 0},
                                  dst_pos - Gui::Vec2f{0.15f * font_scale, 0}, dst_pos, line_thick, ColorRed, backward);
                    }
                    insert_sorted(next_indices, int16_t(next_index));
                    next_index = int(back_info_smooth_.passes_info.size());
                    found = true;
                }
            }
        }
        src_pos[1] -= font_height;
    }

    for (const int16_t j : next_indices) {
        const auto &next = back_info_smooth_.passes_info[j];

        const Gui::Vec2f src_pos = pass.position + Gui::Vec2f{elem_size[0], -elem_border[1] - font_height};
        const Gui::Vec2f dst_pos = next.position + Gui::Vec2f{0, -elem_border[1] - font_height};

        DrawCurve(r, src_pos, src_pos + Gui::Vec2f{0.15f * font_scale, 0}, dst_pos - Gui::Vec2f{0.15f * font_scale, 0},
                  dst_pos, line_thin, (selected_pass_index_ != -1) ? ColorBlack2 : ColorBlack1);
    }
}

void Eng::DebugFrameUI::DrawLine(Gui::Renderer *r, const Gui::Vec2f &_p0, const Gui::Vec2f &_p1,
                                 const Gui::Vec2f &width, const uint8_t color[4]) const {
    const Ren::TextureRegionRef &line_tex = line_.tex();
    const Gui::Vec2f *_uvs = line_.uvs_px();

    const auto p0 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p0[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.5f};
    const auto p1 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p1[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};
    const auto dp = Normalize(Gui::Vec2f{p1 - p0});

    r->PushLine(Gui::eDrawMode::Passthrough, line_tex->pos(2), color, p0, p1, dp, dp,
                Gui::Vec4f{width[0], width[1], 2, 0});
}

void Eng::DebugFrameUI::DrawCurve(Gui::Renderer *r, const Gui::Vec2f &_p0, const Gui::Vec2f &_p1, const Gui::Vec2f &_p2,
                                  const Gui::Vec2f &_p3, const Gui::Vec2f &width, const uint8_t color[4],
                                  const bool backward) const {
    const Ren::TextureRegionRef &line_tex = line_.tex();
    const Gui::Vec2f *_uvs = line_.uvs_px();

    auto p0 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1) * dims_[1][0],
                         dims_[0][1] + 0.5f * (_p0[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.5f};
    auto p1 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1) * dims_[1][0],
                         dims_[0][1] + 0.5f * (_p1[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.0f};
    auto p2 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p2[0] + 1) * dims_[1][0],
                         dims_[0][1] + 0.5f * (_p2[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};
    auto p3 = Gui::Vec4f{dims_[0][0] + 0.5f * (_p3[0] + 1) * dims_[1][0],
                         dims_[0][1] + 0.5f * (_p3[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};

    if (backward) {
        std::swap(p0[3], p2[3]);
        std::swap(p1[3], p3[3]);
    }

    r->PushCurve(Gui::eDrawMode::Passthrough, line_tex->pos(2), color, p0, p1, p2, p3,
                 Gui::Vec4f{width[0], width[1], 2, 0});
}