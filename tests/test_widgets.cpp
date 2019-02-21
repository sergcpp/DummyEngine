#include "test_common.h"

#include "../BaseElement.h"

void test_widgets() {
    using Ren::Vec2f;
    using Ren::Vec2i;

    {
        // BaseElement tests
        {
            // Simple element
            Gui::RootElement root({ 1000, 1000 });
            Gui::BaseElement el({ -0.5f, -0.5f }, { 1, 1 }, &root);

            assert(el.pos() == Vec2f(-0.5f, -0.5f));
            assert(el.size() == Vec2f(1, 1));
            assert(el.pos_px() == Vec2i(250, 250));
            assert(el.size_px() == Vec2i(500, 500));

            root.set_zone({ 2000, 2000 });
            el.Resize(&root);
            el.Resize(&root);

            assert(el.pos() == Vec2f(-0.5f, -0.5f));
            assert(el.size() == Vec2f(1, 1));
            assert(el.pos_px() == Vec2i(500, 500));
            assert(el.size_px() == Vec2i(1000, 1000));

            root.set_zone({ 1000, 1000 });
            el.Resize({ 0, 0 }, { 0.5f, 0.5f }, &root);

            assert(el.pos() == Vec2f(0, 0));
            assert(el.size() == Vec2f(0.5f, 0.5f));
            assert(el.pos_px() == Vec2i(500, 500));
            assert(el.size_px() == Vec2i(250, 250));

            assert(el.Check(Vec2f(0.25f, 0.25f)));
            assert_false(el.Check(Vec2f(-0.25f, 0.25f)));
            assert_false(el.Check(Vec2f(-0.25f, -0.25f)));
            assert_false(el.Check(Vec2f(0.25f, -0.25f)));

            assert(el.Check(Vec2i(600, 600)));
            assert_false(el.Check(Vec2i(-600, 600)));
            assert_false(el.Check(Vec2i(-600, -600)));
            assert_false(el.Check(Vec2i(600, -600)));
        }

        {
            // Parenting
            Gui::RootElement root({ 1000, 1000 });
            Gui::BaseElement par_el({ 0, 0 }, { 1, 1 }, &root);
            Gui::BaseElement child_el({ 0, 0 }, { 1, 1 }, &par_el);

            assert(child_el.pos() == Vec2f(0.5f, 0.5f));
            assert(child_el.size() == Vec2f(0.5f, 0.5f));
            assert(child_el.pos_px() == Vec2i(750, 750));
            assert(child_el.size_px() == Vec2i(250, 250));

            par_el.Resize(&root);
            child_el.Resize(&par_el);

            assert(child_el.pos() == Vec2f(0.5f, 0.5f));
            assert(child_el.size() == Vec2f(0.5f, 0.5f));
            assert(child_el.pos_px() == Vec2i(750, 750));
            assert(child_el.size_px() == Vec2i(250, 250));

            par_el.Resize({ 0.5f, 0.5f }, { 0.5f, 0.5f }, &root);
            child_el.Resize(&par_el);

            assert(child_el.pos() == Vec2f(0.75f, 0.75f));
            assert(child_el.size() == Vec2f(0.25f, 0.25f));
            assert(child_el.pos_px() == Vec2i(875, 875));
            assert(child_el.size_px() == Vec2i(125, 125));
        }
    }

    {
        // LinearLayout tests
        // TODO
    }

}
