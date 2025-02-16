DECORATE(Undefined,     0 /* channel count */, 0 /* PP data len*/, 0 /* block_x */, 0 /* block_y */, VK_FORMAT_UNDEFINED, 0xffffffff, 0xffffffff, 0xffffffff)
DECORATE(RGB8,          3, 3, 0, 0,   VK_FORMAT_R8G8B8_UNORM,             GL_RGB,             GL_RGB8,                            GL_UNSIGNED_BYTE)
DECORATE(RGBA8,         4, 4, 0, 0,   VK_FORMAT_R8G8B8A8_UNORM,           GL_RGBA,            GL_RGBA8,                           GL_UNSIGNED_BYTE)
DECORATE(RGBA8_snorm,   4, 4, 0, 0,   VK_FORMAT_R8G8B8A8_SNORM,           GL_RGBA,            GL_RGBA8_SNORM,                     GL_BYTE)
DECORATE(BGRA8,         4, 4, 0, 0,   VK_FORMAT_B8G8R8A8_UNORM,           0xffffffff,         0xffffffff,                         GL_UNSIGNED_BYTE)
DECORATE(R32F,          1, 4, 0, 0,   VK_FORMAT_R32_SFLOAT,               GL_RED,             GL_R32F,                            GL_FLOAT)
DECORATE(R16F,          1, 2, 0, 0,   VK_FORMAT_R16_SFLOAT,               GL_RED,             GL_R16F,                            GL_HALF_FLOAT)
DECORATE(R8,            1, 1, 0, 0,   VK_FORMAT_R8_UNORM,                 GL_RED,             GL_R8,                              GL_UNSIGNED_BYTE)
DECORATE(R32UI,         1, 4, 0, 0,   VK_FORMAT_R32_UINT,                 GL_RED_INTEGER,     GL_R32UI,                           GL_UNSIGNED_INT)
DECORATE(RG8,           2, 2, 0, 0,   VK_FORMAT_R8G8_UNORM,               GL_RG,              GL_RG8,                             GL_UNSIGNED_BYTE)
DECORATE(RGB32F,        3, 12, 0, 0,  VK_FORMAT_R32G32B32_SFLOAT,         GL_RGB,             GL_RGB32F,                          GL_FLOAT)
DECORATE(RGBA32F,       4, 16, 0, 0,  VK_FORMAT_R32G32B32A32_SFLOAT,      GL_RGBA,            GL_RGBA32F,                         GL_FLOAT)
DECORATE(RGBA32UI,      4, 16, 0, 0,  VK_FORMAT_R32G32B32A32_UINT,        GL_RGBA,            GL_RGBA32UI,                        GL_UNSIGNED_INT)
DECORATE(RGB16F,        3, 6, 0, 0,   VK_FORMAT_R16G16B16_SFLOAT,         GL_RGB,             GL_RGB16F,                          GL_HALF_FLOAT)
DECORATE(RGBA16F,       4, 8, 0, 0,   VK_FORMAT_R16G16B16A16_SFLOAT,      GL_RGBA,            GL_RGBA16F,                         GL_HALF_FLOAT)
DECORATE(RG16_snorm,    2, 4, 0, 0,   VK_FORMAT_R16G16_SNORM,             GL_RG,              GL_RG16_SNORM_EXT,                  GL_SHORT)
DECORATE(RG16,          2, 4, 0, 0,   VK_FORMAT_R16G16_UNORM,             GL_RG,              GL_RG16_EXT,                        GL_UNSIGNED_SHORT)
DECORATE(RG16F,         2, 4, 0, 0,   VK_FORMAT_R16G16_SFLOAT,            GL_RG,              GL_RG16F,                           GL_HALF_FLOAT)
DECORATE(RG32F,         2, 8, 0, 0,   VK_FORMAT_R32G32_SFLOAT,            GL_RG,              GL_RG32F,                           GL_FLOAT)
DECORATE(RG32UI,        2, 8, 0, 0,   VK_FORMAT_R32G32_UINT,              GL_RG_INTEGER,      GL_RG32UI,                          GL_UNSIGNED_INT)
DECORATE(RGB10_A2,      4, 4, 0, 0,   VK_FORMAT_A2B10G10R10_UNORM_PACK32, GL_RGBA,            GL_RGB10_A2,                        GL_UNSIGNED_INT_2_10_10_10_REV)
DECORATE(RG11F_B10F,    3, 4, 0, 0,   VK_FORMAT_B10G11R11_UFLOAT_PACK32,  GL_RGB,             GL_R11F_G11F_B10F,                  GL_FLOAT)
DECORATE(RGB9_E5,       4, 4, 0, 0,   VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,   GL_RGB,             GL_RGB9_E5,                         GL_UNSIGNED_INT_5_9_9_9_REV)
DECORATE(D16,           1, 2, 0, 0,   VK_FORMAT_D16_UNORM,                GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16,               GL_UNSIGNED_SHORT)
DECORATE(D24_S8,        2, 4, 0, 0,   VK_FORMAT_D24_UNORM_S8_UINT,        GL_DEPTH_STENCIL,   GL_DEPTH24_STENCIL8,                GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
DECORATE(D32_S8,        2, 8, 0, 0,   VK_FORMAT_D32_SFLOAT_S8_UINT,       GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8,               GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
DECORATE(D32,           1, 4, 0, 0,   VK_FORMAT_D32_SFLOAT,               GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT32,               GL_UNSIGNED_INT)
DECORATE(BC1,           3, 0, 4, 4,   VK_FORMAT_BC1_RGBA_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   0xffffffff)
DECORATE(BC2,           4, 0, 4, 4,   VK_FORMAT_BC2_UNORM_BLOCK,          0xffffffff,         GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,   0xffffffff)
DECORATE(BC3,           4, 0, 4, 4,   VK_FORMAT_BC3_UNORM_BLOCK,          0xffffffff,         GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,   0xffffffff)
DECORATE(BC4,           1, 0, 4, 4,   VK_FORMAT_BC4_UNORM_BLOCK,          0xffffffff,         GL_COMPRESSED_RED_RGTC1_EXT,        0xffffffff)
DECORATE(BC5,           2, 0, 4, 4,   VK_FORMAT_BC5_UNORM_BLOCK,          0xffffffff,         GL_COMPRESSED_RED_GREEN_RGTC2_EXT,  0xffffffff)
DECORATE(ASTC_4x4,      0, 0, 4, 4,   VK_FORMAT_ASTC_4x4_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_4x4_KHR,    0xffffffff)
DECORATE(ASTC_5x4,      0, 0, 5, 4,   VK_FORMAT_ASTC_5x4_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_5x4_KHR,    0xffffffff)
DECORATE(ASTC_5x5,      0, 0, 5, 5,   VK_FORMAT_ASTC_5x5_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_5x5_KHR,    0xffffffff)
DECORATE(ASTC_6x5,      0, 0, 6, 5,   VK_FORMAT_ASTC_6x5_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_6x5_KHR,    0xffffffff)
DECORATE(ASTC_6x6,      0, 0, 6, 5,   VK_FORMAT_ASTC_6x6_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_6x6_KHR,    0xffffffff)
DECORATE(ASTC_8x5,      0, 0, 8, 5,   VK_FORMAT_ASTC_8x5_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_8x5_KHR,    0xffffffff)
DECORATE(ASTC_8x6,      0, 0, 8, 6,   VK_FORMAT_ASTC_8x6_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_8x6_KHR,    0xffffffff)
DECORATE(ASTC_8x8,      0, 0, 8, 8,   VK_FORMAT_ASTC_8x8_UNORM_BLOCK,     0xffffffff,         GL_COMPRESSED_RGBA_ASTC_8x8_KHR,    0xffffffff)
DECORATE(ASTC_10x5,     0, 0, 10, 5,  VK_FORMAT_ASTC_10x5_UNORM_BLOCK,    0xffffffff,         GL_COMPRESSED_RGBA_ASTC_10x5_KHR,   0xffffffff)
DECORATE(ASTC_10x6,     0, 0, 10, 6,  VK_FORMAT_ASTC_10x6_UNORM_BLOCK,    0xffffffff,         GL_COMPRESSED_RGBA_ASTC_10x6_KHR,   0xffffffff)
DECORATE(ASTC_10x8,     0, 0, 10, 8,  VK_FORMAT_ASTC_10x8_UNORM_BLOCK,    0xffffffff,         GL_COMPRESSED_RGBA_ASTC_10x8_KHR,   0xffffffff)
DECORATE(ASTC_10x10,    0, 0, 10, 10, VK_FORMAT_ASTC_10x10_UNORM_BLOCK,   0xffffffff,         GL_COMPRESSED_RGBA_ASTC_10x10_KHR,  0xffffffff)
DECORATE(ASTC_12x10,    0, 0, 12, 10, VK_FORMAT_ASTC_12x10_UNORM_BLOCK,   0xffffffff,         GL_COMPRESSED_RGBA_ASTC_12x10_KHR,  0xffffffff)
DECORATE(ASTC_12x12,    0, 0, 12, 12, VK_FORMAT_ASTC_12x12_UNORM_BLOCK,   0xffffffff,         GL_COMPRESSED_RGBA_ASTC_12x12_KHR,  0xffffffff)