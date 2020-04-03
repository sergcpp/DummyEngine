#include "Context.h"

#include "SW/SW.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::Context::~Context() {
    ReleaseAll();

    swDeleteContext(sw_ctx_);
}

void Ren::Context::Init(int w, int h) {
    w_ = w;
    h_ = h;

    sw_ctx_ = swCreateContext(w, h);

    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);

    printf("===========================================\n");
    printf("Device info:\n");

    // TODO: get cpu name and memory
    // print device info
    printf("\tVendor\t\t: %s\n", swGetString(SW_CPU_VENDOR));
    printf("\tRenderer\t: %s\n", "Software");

    printf("Capabilities:\n");

    printf("\tCPU Model\t\t: %s\n", swGetString(SW_CPU_MODEL));
    printf("\tNum CPUs\t\t: %i\n", (int)swGetInteger(SW_NUM_CPUS));
    printf("\tPhysical memory\t: %f GB\n", (float)swGetFloat(SW_PHYSICAL_MEMORY));

    // how many uniform vec4 vectors can be used
    max_uniform_vec4 = swGetInteger(SW_MAX_VERTEX_UNIFORM_VECTORS);
    printf("\tMax uniforms\t: %i\n", max_uniform_vec4);

    // how many bones(mat4) can be used at time
    Mesh::max_gpu_bones = max_uniform_vec4 / 2;
    printf("\tBones per pass\t: %i\n", Mesh::max_gpu_bones);

    printf("===========================================\n\n");

    default_vertex_buf_ = buffers_.Add(32 * 1024 * 1024);
    default_indices_buf_ = buffers_.Add(32 * 1024 * 1024);
}

void Ren::Context::Resize(int w, int h) {
    w_ = w;
    h_ = h;

    SWint cur = swGetCurFramebuffer();
    assert(cur == 0);
    swDeleteFramebuffer(cur);
    swCreateFramebuffer(SW_BGRA8888, w, h, true);
}

Ren::ProgramRef Ren::Context::LoadProgramSW(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
        const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *load_status) {
    ProgramRef ref;
    for (auto it = programs_.begin(); it != programs_.end(); ++it) {
        if (strcmp(it->name(), name) == 0) {
            ref = { &programs_, it.index() };
            break;
        }
    }

    if (!ref) {
        ref = programs_.Add(name, vs_shader, fs_shader, num_fvars, attrs, unifs, load_status);
    } else {
        if (ref->ready()) {
            if (load_status) *load_status = Found;
        } else if (!ref->ready() && vs_shader && fs_shader) {
            ref->Init(name, vs_shader, fs_shader, num_fvars, attrs, unifs, load_status);
        }
    }

    return ref;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif