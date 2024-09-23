#pragma once

#include <memory>
#include <optional>
#include <string>

#include <Gui/Image9Patch.h>

namespace Ren {
class Context;
}

namespace Eng {
struct BackendInfo;
struct FrontendInfo;
struct ItemsInfo;

class DebugFrameUI final : public Gui::BaseElement {
  public:
    DebugFrameUI(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                 const Gui::BitmapFont *font_small);

    void UpdateInfo(const FrontendInfo &frontend_info, const BackendInfo &backend_info, const ItemsInfo &items_info,
                    bool debug_items);

    enum class eViewMode { Off, Compact, Detailed };

    eViewMode view_mode() const { return view_mode_; }
    void set_view_mode(const eViewMode mode) { view_mode_ = mode; }

    bool HandleInput(const Gui::input_event_t &ev, const std::vector<bool> &keys_state) override;

    void Draw(Gui::Renderer *r) override;

  private:
    const Gui::BitmapFont *font_small_;

    Gui::Image9Patch back_, element_, element_highlighted_;
    Gui::Image line_;

#if !defined(NDEBUG)
    eViewMode view_mode_ = eViewMode::Detailed;
#else
    eViewMode view_mode_ = eViewMode::Off;
#endif

    Gui::Vec2f view_offset_ = Gui::Vec2f{-0.9f, 0.5f};
    float view_scale_ = 1.0f;
    bool view_grabbed_ = false;
    Gui::Vec2f p1_down_pos_ = Gui::Vec2f{-1};
    std::optional<Gui::Vec2i> deferred_select_pos_;
    int selected_index_ = -1;

    uint64_t last_frame_time_ = 0;
    double cur_frame_dur_ = 0.0;

    bool debug_items_ = false;

    struct {
        float occluders_time_ms = 0.0f, main_gather_time_ms = 0.0f, shadow_gather_time_ms = 0.0f,
              drawables_sort_time_ms = 0.0f, items_assignment_time_ms = 0.0f;
        float total_time_ms = 0.0f;
    } front_info_smooth_;

    struct pass_info_smooth_t {
        std::string name;
        float duration_ms;
        Ren::SmallVector<std::string, 16> input;
        Ren::SmallVector<std::string, 16> output;
        Gui::Vec2f position;
    };

    struct {
        pass_info_smooth_t passes[256];
        int pass_count = 0;

        float cpu_total_ms = 0.0f, gpu_total_ms = 0.0f;

        float shadow_draw_calls_count = 0.0f, depth_fill_draw_calls_count = 0.0f, opaque_draw_calls_count = 0.0f;

        float tris_rendered = 0.0f;
    } back_info_smooth_;

    struct {
        float lights_count = 0.0f, decals_count = 0.0f, probes_count = 0.0f;
        float items_total = 0.0f;
    } items_info_smooth_;

    struct {
        uint64_t front_start_timepoint_us = 0, front_end_timepoint_us = 0;
        uint64_t back_cpu_start_timepoint_us = 0, back_cpu_end_timepoint_us = 0;
        uint64_t back_gpu_duration = 0;
        int64_t gpu_cpu_time_diff_us = 0;
    } prev_timing_info_, cur_timing_info_;

    void DrawCompact(Gui::Renderer *r);
    void DrawDetailed(Gui::Renderer *r);

    void DrawPassInfo(Gui::Renderer *r, int pass_index, Gui::Vec2f elem_pos, float font_scale);
    void DrawConnectionCurves(Gui::Renderer *r, int pass_index, float font_scale, bool detailed_outputs);

    void DrawLine(Gui::Renderer *r, const Gui::Vec2f &p0, const Gui::Vec2f &p1, const Gui::Vec2f &width) const;
    void DrawCurve(Gui::Renderer *r, const Gui::Vec2f &p0, const Gui::Vec2f &p1, const Gui::Vec2f &p2,
                   const Gui::Vec2f &p3, const Gui::Vec2f &width, const uint8_t color[4], bool backward = false) const;
};
}; // namespace Eng