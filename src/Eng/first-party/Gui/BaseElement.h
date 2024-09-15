#pragma once

#include <vector>

#include "Bitmask.h"
#include "Input.h"
#include "MVec.h"

namespace Gui {
// forward declare everything
class BitmapFont;
class ButtonBase;
class ButtonImage;
class ButtonText;
class Cursor;
class EditBox;
class Image;
class Image9Patch;
class LinearLayout;
class Renderer;
class TypeMesh;

enum class eFlags { Visible, Resizable };

class BaseElement {
  protected:
    Vec2f rel_dims_[2];

    Vec2f dims_[2];
    Vec2i dims_px_[2];
    Bitmask<eFlags> flags_;

    const BaseElement *parent_ = nullptr;
    mutable std::vector<BaseElement *> children_;

  public:
    BaseElement(const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    ~BaseElement();

    [[nodiscard]] Bitmask<eFlags> flags() const { return flags_; }
    void set_flags(const Bitmask<eFlags> flags) { flags_ = flags; }

    void set_parent(const BaseElement *parent) { parent_ = parent; }

    [[nodiscard]] const Vec2f *dims() const { return dims_; }
    [[nodiscard]] const Vec2f *rel_dims() const { return rel_dims_; }

    [[nodiscard]] const Vec2f &rel_pos() const { return rel_dims_[0]; }
    [[nodiscard]] const Vec2f &rel_size() const { return rel_dims_[1]; }

    [[nodiscard]] const Vec2f &pos() const { return dims_[0]; }
    [[nodiscard]] const Vec2f &size() const { return dims_[1]; }

    [[nodiscard]] float aspect() const { return dims_[1][1] / dims_[1][0]; }

    [[nodiscard]] const Vec2i &pos_px() const { return dims_px_[0]; }
    [[nodiscard]] const Vec2i &size_px() const { return dims_px_[1]; }

    [[nodiscard]] Vec2f ToLocal(const Vec2f &p) const { return 2.0f * (p - dims_[0]) / dims_[1] - 1.0f; }
    [[nodiscard]] Vec2f ToLocal(const Vec2i &p) const {
        return 2.0f * Vec2f(p - dims_px_[0]) / Vec2f(dims_px_[1]) - 1.0f;
    }

    virtual void Resize();
    virtual void Resize(const Vec2f &pos, const Vec2f &size);

    [[nodiscard]] virtual bool Check(const Vec2i &p) const;
    [[nodiscard]] virtual bool Check(const Vec2f &p) const;

    virtual bool HandleInput(const input_event_t &ev, const std::vector<bool> &keys_state);

    virtual void Draw(Renderer *r);

    void AddChild(BaseElement *el) const;
    void RemoveChild(BaseElement *el) const;

    enum class eSnapMode { Closest, Down, Up };

    Vec2f SnapToPixels(const Vec2f &p, eSnapMode mode = eSnapMode::Closest) const;
};

class RootElement : public BaseElement {
  public:
    explicit RootElement(const Vec2i &zone_size) : BaseElement(Vec2f{-1}, Vec2f{2}, nullptr) {
        set_zone(zone_size);
        RootElement::Resize(Vec2f{-1}, Vec2f{2});
    }

    void set_zone(const Vec2i &zone_size) { dims_px_[1] = zone_size; }

    void Resize() override { Resize(dims_[0], dims_[1]); }
    using BaseElement::Resize;

    void Resize(const Vec2f &pos, const Vec2f &size) override {
        dims_[0] = pos;
        dims_[1] = size;

        BaseElement::Resize();
    }
};
} // namespace Gui
