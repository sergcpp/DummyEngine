
#if !defined(VULKAN)
layout(location = REN_U_BASE_INSTANCE_LOC) uniform uint _base_instance;
#define gl_InstanceIndex (_base_instance + gl_InstanceID)
#endif