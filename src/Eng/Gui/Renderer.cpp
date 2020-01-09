#include "Renderer.h"

Gui::TextureDataRef Gui::Renderer::LoadTexture(const char *name, const void *data, const int res[2], Ren::eTexColorFormat format) {
    TextureDataRef ref = tex_storage_.FindByName(name);
    if (!ref) {
        int pos[3];
        int node = tex_atlas_.Allocate(data, format, res, pos, 1);
        if (node != -1) {
            ref = tex_storage_.Add(name, &tex_atlas_, pos);
        }
    }
    return ref;
}