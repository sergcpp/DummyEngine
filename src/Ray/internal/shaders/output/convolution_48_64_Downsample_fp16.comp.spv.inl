/* Contents of file internal/shaders/output/convolution_48_64_Downsample_fp16.comp.spv */
const int internal_shaders_output_convolution_48_64_Downsample_fp16_comp_spv_size = 3147;
const unsigned char internal_shaders_output_convolution_48_64_Downsample_fp16_comp_spv[3147] = {
    0x78, 0xDA, 0x6D, 0x5A, 0x09, 0x70, 0x55, 0xD5, 0x19, 0x7E, 0xE7, 0xBE, 0x90, 0xA5, 0x20, 0x84, 0x9D, 0x17, 0x16, 0x49, 0x58, 0x04, 0x82, 0x04, 0x0A, 0xC4, 0x90, 0x28, 0x85, 0x22, 0x8B, 0xEC, 0x61, 0x99, 0xA2, 0x25, 0x2C, 0x35, 0xA0, 0x89, 0xD2, 0xB2, 0x97, 0x35, 0xEC, 0x04, 0x7C, 0x40, 0x22, 0x19, 0x76, 0x64, 0x47, 0x1C, 0x0B, 0x25, 0x32, 0x15, 0x2C, 0x8E, 0xA5, 0xE3, 0x74, 0x1C, 0x2D, 0x42, 0x15, 0xA6, 0x1B, 0x23, 0x42, 0x71, 0xA6, 0x5A, 0xB5, 0xCA, 0x52, 0x90, 0x2D, 0xDA, 0xF3, 0xBF, 0xF3, 0xFD, 0xE4, 0x7B, 0x67, 0xC2, 0xCC, 0xC9, 0xBD, 0xFF, 0xF6, 0xFD, 0xCB, 0xF9, 0xCF, 0xB9, 0xE7, 0xDD, 0x4B, 0x38, 0x68, 0x93, 0x14, 0x0A, 0x9B, 0x50, 0xED, 0x50, 0x72, 0xA8, 0x61, 0x38, 0x14, 0xFB, 0x57, 0x3F, 0x14, 0x84, 0x0C, 0xAE, 0x29, 0xB8, 0xE6, 0xD7, 0x0F, 0x59, 0x9D, 0xC4, 0x18, 0x7F, 0xE0, 0xD0, 0xD1, 0x43, 0xBB, 0xCC, 0x9A, 0x5D, 0xD8, 0xA5, 0x47, 0xCF, 0xAE, 0xA2, 0x5F, 0x37, 0xE4, 0x0C, 0x45, 0x56, 0x2F, 0x94, 0x14, 0xAA, 0x65, 0xAF, 0x09, 0x76, 0x4C, 0x9B, 0x5C, 0xF4, 0x4B, 0xE1, 0x67, 0xDA, 0x91, 0x65, 0x47, 0xAA, 0xB5, 0x4F, 0x88, 0xE1, 0xC9, 0xBD, 0xD3, 0x4F, 0xB6, 0x63, 0x90, 0xB5, 0x68, 0xEC, 0x5C, 0x87, 0xDA, 0xE0, 0xAA, 0x3C, 0x03, 0x5E, 0x2A, 0xF1, 0x02, 0xF0, 0x9A, 0x11, 0x2F, 0x0C, 0x5E, 0x0B, 0xE2, 0x25, 0x80, 0xD7, 0x9A, 0x78, 0xB5, 0xC0, 0x6B, 0x4B, 0xBC, 0x44, 0xF0, 0x3A, 0x10, 0x2F, 0x09, 0xBC, 0xCE, 0xC4, 0x4B, 0x06, 0xAF, 0x2B, 0xF1, 0x52, 0xC0, 0xCB, 0x01, 0xAF, 0x89, 0x97, 0xC7, 0x40, 0x1B, 0x59, 0x13, 0xC4, 0x3C, 0xD0, 0x46, 0x24, 0xB5, 0xA8, 0x6D, 0x47, 0x1A, 0xE8, 0x2C, 0xD0, 0xCD, 0x41, 0x2F, 0x46, 0x3C, 0x41, 0x0C, 0x2F, 0x21, 0x54, 0x02, 0x9C, 0x86, 0x1E, 0xAD, 0xB9, 0x97, 0xD4, 0xE0, 0xAF, 0x84, 0xFC, 0x2D, 0xB5, 0xD7, 0x8C, 0x07, 0x32, 0x47, 0xA7, 0x23, 0x17, 0xA1, 0x0F, 0x78, 0xFE, 0x0E, 0x7A, 0xFE, 0x0E, 0x7A, 0xFE, 0x0E, 0xD6, 0xE0, 0xEF, 0x20, 0xF9, 0x3B, 0xE4, 0xF9, 0x3B, 0x04, 0x7F, 0x2A, 0x0F, 0x4C, 0xBC, 0xBF, 0xB0, 0x89, 0xF7, 0xA7, 0xB4, 0xFA, 0x53, 0x9A, 0xFD, 0x09, 0x4F, 0xF1, 0x6A, 0x99, 0x78, 0x7F, 0x42, 0xA7, 0x63, 0xFE, 0x84, 0x3E, 0xEA, 0xF9, 0xAB, 0xF4, 0xFC, 0x29, 0x1D, 0x81, 0xBF, 0xCA, 0x1A, 0xFC, 0x55, 0x92, 0xBF, 0x63, 0x9E, 0xBF, 0x63, 0xF0, 0xE7, 0x30, 0x83, 0x98, 0x5E, 0xBA, 0xC4, 0x08, 0x9F, 0x8D, 0xAD, 0x4E, 0x22, 0x7A, 0xD0, 0xC4, 0xAE, 0x09, 0xB1, 0xD8, 0x92, 0x50, 0xF7, 0x74, 0xFB, 0x37, 0x19, 0xF7, 0x49, 0x34, 0x32, 0x61, 0x57, 0xD7, 0x8E, 0x3E, 0xC0, 0x49, 0x05, 0x8E, 0xFC, 0x6B, 0x6A, 0x69, 0x5D, 0x47, 0xD2, 0xE7, 0x4D, 0xAC, 0xC7, 0x06, 0x90, 0x8B, 0x4C, 0xF2, 0x6B, 0x80, 0x18, 0x84, 0x6E, 0x04, 0x5D, 0xA1, 0x5B, 0x85, 0xEA, 0xC4, 0x7A, 0xB7, 0x3E, 0xEA, 0x90, 0xE8, 0x8D, 0x06, 0xB8, 0x36, 0xC2, 0x68, 0x85, 0x1E, 0x6E, 0x8C, 0xF8, 0x9B, 0xA2, 0xEF, 0x85, 0x97, 0x07, 0xBA, 0x19, 0x78, 0x1A, 0x77, 0x04, 0x71, 0x2A, 0x9D, 0x86, 0x38, 0xC5, 0xBE, 0x39, 0x74, 0x13, 0x11, 0x5B, 0x7B, 0xDC, 0x87, 0x41, 0x77, 0x44, 0xAC, 0x61, 0xE8, 0x77, 0x42, 0xED, 0x3A, 0xC2, 0x5F, 0x27, 0xEC, 0x29, 0x86, 0xE8, 0x2C, 0xD0, 0xEA, 0xAF, 0x3B, 0x6A, 0x91, 0x89, 0xBA, 0xF5, 0xF0, 0xE2, 0xC9, 0x41, 0x2D, 0x54, 0xDE, 0x9B, 0xEC, 0x85, 0xEE, 0x07, 0x79, 0x23, 0xFB, 0x77, 0x08, 0xD9, 0x0D, 0x43, 0xEC, 0x12, 0xD7, 0x70, 0xDC, 0xA7, 0xA2, 0xFE, 0x33, 0x71, 0xAF, 0x18, 0xB3, 0x90, 0x73, 0x0B, 0xFB, 0x77, 0xB6, 0xBD, 0xCE, 0x04, 0x4F, 0xE8, 0x39, 0xF6, 0x3A, 0x1B, 0x7E, 0x04, 0x6B, 0x2E, 0xE6, 0x5D, 0xF8, 0x2D, 0x2D, 0xD6, 0x62, 0xE8, 0xB7, 0xC2, 0x7A, 0x5E, 0x0C, 0xBD, 0x25, 0xF6, 0x5A, 0xC7, 0x8E, 0x12, 0xE4, 0x2E, 0xF4, 0x52, 0xF0, 0x44, 0xBE, 0x12, 0xF7, 0x33, 0x41, 0x97, 0x02, 0x77, 0x26, 0xE5, 0x10, 0xF5, 0x6A, 0xB5, 0xD9, 0x8E, 0x72, 0xC4, 0xB5, 0x07, 0xBA, 0x1A, 0xD7, 0x5E, 0xD8, 0xEF, 0x41, 0x5C, 0x07, 0x28, 0x2E, 0x59, 0xF7, 0x07, 0xA0, 0xF7, 0x2A, 0xFC, 0x1E, 0x44, 0x5C, 0x42, 0x1F, 0x02, 0x4F, 0xFD, 0xBC, 0x86, 0xBD, 0x53, 0x70, 0x64, 0x1F, 0x50, 0x9C, 0xD8, 0x7A, 0x36, 0x0E, 0x27, 0xC1, 0x38, 0x1B, 0xE1, 0xE5, 0x81, 0xAE, 0x65, 0xE2, 0x71, 0x52, 0x8C, 0xEB, 0x0D, 0xA5, 0x33, 0xC4, 0xD6, 0x38, 0x5A, 0x30, 0x0F, 0x63, 0x0D, 0x8B, 0x9F, 0xA3, 0xE4, 0x47, 0xD6, 0xF1, 0x51, 0xF8, 0x79, 0x03, 0x98, 0x95, 0xF0, 0x23, 0xF4, 0x31, 0xCF, 0xCF, 0x17, 0xC6, 0xED, 0x93, 0x4A, 0x37, 0x08, 0x3B, 0xFD, 0x6C, 0xBB, 0x4F, 0x04, 0x98, 0xDB, 0x10, 0xFA, 0xF4, 0xB6, 0xE5, 0xD4, 0x42, 0xEE, 0x52, 0xAF, 0x5F, 0xA0, 0x6E, 0x4A, 0x17, 0x13, 0x2D, 0x73, 0xFD, 0x6B, 0x8F, 0x5E, 0xE0, 0xE9, 0x97, 0x79, 0xF4, 0x56, 0xA2, 0x65, 0x4E, 0xF6, 0x79, 0xF4, 0xDB, 0x1E, 0xFD, 0x17, 0x8F, 0xFE, 0xCC, 0xC3, 0xBB, 0xE3, 0xD1, 0x1D, 0x4C, 0x3C, 0x5D, 0xE2, 0xD1, 0xAB, 0x3D, 0xFA, 0xAC, 0x47, 0x5F, 0xF0, 0xE8, 0xFF, 0x7A, 0x74, 0x6A, 0x10, 0x4F, 0xD7, 0xF7, 0xE8, 0x06, 0x1E, 0x9D, 0xEE, 0xD1, 0x19, 0x1E, 0xDD, 0xC6, 0xA3, 0xB3, 0x3D, 0xFA, 0x31, 0x8F, 0xCE, 0xF1, 0xE8, 0x21, 0x1E, 0x3D, 0xD4, 0xA3, 0x87, 0x79, 0xF4, 0x78, 0x8F, 0x2E, 0xF0, 0xE8, 0x09, 0x1E, 0x3D, 0xC3, 0xA3, 0x67, 0x7A, 0xF4, 0x2C, 0x8F, 0x5E, 0xEE, 0xD1, 0x2B, 0x3C, 0x7A, 0xA5, 0x47, 0x6F, 0xF1, 0xE8, 0xAD, 0x1E, 0xBD, 0x0D, 0xF4, 0x77, 0xB6, 0x3B, 0xAF, 0x06, 0xAE, 0x4F, 0xEF, 0xD9, 0x7B, 0xD9, 0x07, 0xAF, 0x05, 0xAE, 0x5F, 0xE5, 0xFA, 0x04, 0xF6, 0xDB, 0xCE, 0xD8, 0x53, 0x17, 0x61, 0x3F, 0x7E, 0x14, 0x3C, 0x95, 0x77, 0xC5, 0x1E, 0xAB, 0xF2, 0x6E, 0xE0, 0xE5, 0xDB, 0xAE, 0x97, 0xB5, 0x91, 0x0D, 0x1B, 0xF9, 0xB7, 0x12, 0xBC, 0xC7, 0xB0, 0x0F, 0x67, 0x93, 0x5E, 0x2E, 0x6C, 0x59, 0x2F, 0x0F, 0xFB, 0x71, 0x6E, 0x6C, 0x2F, 0x73, 0xBC, 0xC7, 0x61, 0x9F, 0x47, 0xB6, 0x7D, 0xE0, 0xC3, 0x90, 0x6D, 0x5F, 0xF0, 0x73, 0x48, 0xAF, 0x3F, 0xF4, 0x02, 0xD2, 0x1B, 0x00, 0x7E, 0x77, 0xD2, 0x1B, 0x84, 0x58, 0x58, 0xEF, 0x29, 0x3C, 0xAB, 0x06, 0x51, 0x2C, 0x83, 0x61, 0xFF, 0x54, 0xCC, 0x5F, 0x52, 0x6C, 0xDF, 0x1F, 0x81, 0x67, 0x5E, 0x04, 0xCF, 0x84, 0x1E, 0xA8, 0x95, 0xEC, 0xFB, 0x23, 0x21, 0x5F, 0x84, 0x7D, 0x23, 0x1F, 0xBC, 0xA3, 0x16, 0x4F, 0x9E, 0x25, 0xA3, 0x90, 0x5F, 0x7E, 0x6C, 0x4F, 0x4C, 0x88, 0xF1, 0x46, 0x83, 0x2F, 0xF3, 0x35, 0x16, 0xF5, 0xB9, 0x6B, 0x65, 0xC2, 0x1F, 0x63, 0xC7, 0x58, 0xEC, 0x31, 0x63, 0x28, 0x86, 0x9F, 0x79, 0x31, 0xF4, 0xA6, 0x18, 0xC6, 0x41, 0xAE, 0x31, 0x3C, 0x0D, 0x9E, 0xC6, 0xF0, 0x0C, 0x6A, 0xF7, 0x74, 0x6C, 0x2F, 0x08, 0x1E, 0xE0, 0xCB, 0xF5, 0xA6, 0xC5, 0x17, 0x9D, 0x9F, 0x23, 0x26, 0xE9, 0x93, 0x67, 0x10, 0x87, 0xDA, 0x17, 0xA0, 0x2E, 0x72, 0x3E, 0x79, 0x15, 0xBC, 0x09, 0xB0, 0x29, 0x40, 0x1E, 0x93, 0x28, 0x0F, 0x91, 0x4D, 0xB4, 0x63, 0x12, 0xFC, 0x4C, 0x84, 0xDF, 0xAB, 0xE8, 0xC5, 0x49, 0xB1, 0xF8, 0xC3, 0xB1, 0x7D, 0x33, 0x02, 0xD9, 0xB3, 0xD0, 0x7D, 0x16, 0x79, 0x49, 0x1E, 0xCF, 0x63, 0x6F, 0x7D, 0x03, 0x3E, 0x8B, 0xC0, 0x93, 0xF9, 0xBF, 0x65, 0x75, 0xA6, 0xD8, 0xEB, 0x54, 0xF2, 0x2B, 0xF2, 0x42, 0x3B, 0xA6, 0x00, 0xAB, 0x10, 0x7E, 0x8A, 0xC9, 0xCF, 0x0B, 0x90, 0xBD, 0x40, 0x7E, 0xA6, 0x63, 0xCF, 0x56, 0x3F, 0x33, 0xC0, 0x4B, 0x83, 0x9F, 0x69, 0xF6, 0xFA, 0x22, 0xF9, 0x99, 0x01, 0x7A, 0x1A, 0xB0, 0x5E, 0x24, 0xAC, 0x79, 0x88, 0x59, 0xE9, 0xF9, 0xC0, 0xD6, 0xFE, 0x5A, 0x81, 0x5A, 0xCE, 0x8F, 0xCD, 0x49, 0x62, 0xEC, 0x19, 0xBE, 0x0A, 0xCF, 0xF5, 0x08, 0xE4, 0x4F, 0xE0, 0xD9, 0xB6, 0x1A, 0x32, 0xD1, 0x93, 0x67, 0xFB, 0x1A, 0x3C, 0x3B, 0xE6, 0xC1, 0x5E, 0x72, 0x5B, 0x03, 0x3D, 0xD5, 0x59, 0x8B, 0xE7, 0x0D, 0xEB, 0xAC, 0x85, 0x8E, 0xC6, 0xF4, 0x92, 0x17, 0xD3, 0x3A, 0xF0, 0xA2, 0x54, 0xAF, 0x75, 0x5E, 0xBD, 0xA6, 0x81, 0x9E, 0x0A, 0x7A, 0x2A, 0xE1, 0xAD, 0x47, 0xCE, 0x8A, 0xB7, 0x01, 0xBC, 0x28, 0xCD, 0xF3, 0x06, 0x6F, 0x9E, 0xA7, 0x40, 0x56, 0x46, 0x73, 0x53, 0x0E, 0x59, 0x39, 0x61, 0x6F, 0xC2, 0xF3, 0x51, 0xE7, 0x66, 0x0B, 0x78, 0x9B, 0x31, 0x37, 0x1B, 0xED, 0xB5, 0x82, 0xE6, 0x46, 0xE4, 0x2F, 0xDB, 0xB1, 0x11, 0x58, 0x2F, 0xC3, 0xCF, 0x56, 0xF2, 0xB3, 0x0D, 0xB2, 0x6D, 0xE4, 0x67, 0x17, 0x9E, 0xBB, 0xEA, 0x67, 0x37, 0x78, 0xDA, 0x6B, 0x3B, 0xEC, 0x75, 0x3B, 0xF9, 0xD9, 0x0D, 0x7A, 0x07, 0xB0, 0xB6, 0x13, 0xD6, 0x7E, 0x60, 0xC9, 0x9C, 0xC8, 0xF9, 0xF7, 0x30, 0xAD, 0xDD, 0x1C, 0xD2, 0x3B, 0x02, 0x99, 0xEE, 0x47, 0xBF, 0xC5, 0x3A, 0x3D, 0x42, 0xB5, 0xAC, 0x04, 0xFF, 0x71, 0xE2, 0x1D, 0x03, 0x7F, 0x3F, 0xD9, 0xBE, 0x89, 0x33, 0xD7, 0x31, 0xC2, 0x3F, 0x8E, 0xDA, 0xA9, 0xDD, 0x09, 0xE8, 0x1D, 0xA7, 0xDE, 0x7B, 0x0B, 0x67, 0xB7, 0x08, 0xE4, 0xDA, 0x7B, 0xBF, 0x87, 0xAC, 0xAF, 0xB5, 0x95, 0xBE, 0x3A, 0x89, 0x73, 0xC8, 0x7E, 0xD4, 0xF3, 0x24, 0x74, 0x14, 0xFB, 0x14, 0x62, 0x8F, 0x52, 0x4C, 0xEF, 0x82, 0xCF, 0xF9, 0xFC, 0x09, 0x7C, 0xCE, 0xE7, 0x3D, 0xF0, 0x39, 0x9F, 0x0F, 0x90, 0xCF, 0x7B, 0xA4, 0x77, 0x1A, 0x7C, 0x8E, 0xFF, 0x43, 0x8A, 0xFF, 0x34, 0xC5, 0x7F, 0x06, 0x32, 0x8D, 0xFF, 0x2C, 0xCE, 0x4D, 0x1A, 0xFF, 0x59, 0xE8, 0x28, 0xF6, 0x79, 0xC4, 0x9F, 0x43, 0x31, 0xFC, 0x0D, 0x7C, 0x8E, 0xFF, 0x1F, 0xE0, 0x73, 0xFC, 0x17, 0xC0, 0xE7, 0xF8, 0x2F, 0x22, 0xFE, 0x0B, 0xA4, 0x77, 0x09, 0x7C, 0x8E, 0xFF, 0x32, 0xC5, 0x7F, 0x89, 0xE2, 0xFF, 0x17, 0x64, 0x1A, 0xFF, 0x15, 0x9C, 0xF3, 0x34, 0xFE, 0x2B, 0xD0, 0x51, 0xEC, 0xCF, 0x11, 0xBF, 0x9C, 0x97, 0x35, 0x86, 0x2F, 0xC1, 0xE7, 0xF8, 0xBF, 0x06, 0x9F, 0xE3, 0xFF, 0x06, 0x7C, 0x8E, 0xFF, 0x1A, 0xE2, 0xFF, 0x86, 0xF4, 0x6E, 0x80, 0xCF, 0xF1, 0xFF, 0x8F, 0xE2, 0xBF, 0x41, 0xF1, 0xDF, 0x84, 0x4C, 0xE3, 0xBF, 0x85, 0x73, 0xA9, 0xC6, 0x7F, 0x0B, 0x3A, 0xDA, 0xAB, 0xDF, 0x61, 0xCD, 0xA8, 0xAF, 0xDB, 0xE0, 0x45, 0x69, 0xFD, 0xDE, 0xF6, 0xD6, 0xEF, 0x0E, 0xC8, 0xEE, 0xD0, 0xDA, 0xBE, 0x0B, 0xD9, 0x5D, 0xC2, 0xFE, 0x01, 0x67, 0x60, 0x5D, 0xDB, 0x72, 0xA8, 0xF8, 0x81, 0xF6, 0xF7, 0xFB, 0xF6, 0x5A, 0x45, 0x6B, 0x5B, 0xE4, 0xF7, 0xEC, 0xFD, 0x7D, 0x60, 0xDD, 0x23, 0xAC, 0x24, 0xE3, 0xB0, 0x34, 0xCE, 0x64, 0xE3, 0xF6, 0xF3, 0x24, 0xAA, 0x7B, 0x6D, 0xE3, 0xF8, 0xF2, 0x1B, 0x42, 0xED, 0x1E, 0x32, 0xF1, 0x6B, 0xB1, 0xAE, 0x71, 0x7A, 0xC2, 0xD7, 0x5A, 0xD6, 0xC3, 0xEF, 0x9F, 0x08, 0xE4, 0x5A, 0xCB, 0x54, 0xE3, 0x64, 0x6A, 0xDB, 0x14, 0xB6, 0x9B, 0x09, 0x2F, 0x62, 0x1C, 0x9F, 0xF1, 0xD2, 0x08, 0x2F, 0x42, 0x78, 0xCD, 0x8D, 0x93, 0xA9, 0x6D, 0x1B, 0xE0, 0x65, 0x10, 0xAF, 0x9D, 0x71, 0x7C, 0xC6, 0x6B, 0x4F, 0x78, 0xED, 0x08, 0xEF, 0x11, 0xE3, 0x64, 0x32, 0x17, 0x1D, 0x4C, 0xF5, 0x5C, 0x74, 0x34, 0xAE, 0x7E, 0x1D, 0x4D, 0x75, 0x1D, 0xBA, 0x18, 0xF7, 0xFB, 0x43, 0xE7, 0x22, 0xCB, 0x38, 0x9E, 0xEE, 0xB3, 0x99, 0xF6, 0xBE, 0x93, 0xA9, 0x9E, 0x8B, 0x2C, 0xD0, 0x99, 0xC0, 0xEA, 0x44, 0x58, 0x5D, 0x81, 0xA5, 0x74, 0x37, 0xCC, 0x8D, 0xF6, 0x5C, 0x77, 0xE3, 0xF6, 0xAC, 0xAE, 0x14, 0x6B, 0x0F, 0xE3, 0xF8, 0xFA, 0xBC, 0xCC, 0x31, 0xEE, 0x79, 0x29, 0x3A, 0xDD, 0x48, 0xAF, 0x97, 0x71, 0xB2, 0x3A, 0xF6, 0x57, 0xA1, 0xD0, 0xB9, 0xC6, 0x9D, 0x45, 0x7F, 0x0C, 0x0C, 0x99, 0x93, 0x5E, 0xC8, 0x59, 0xF4, 0x72, 0x4D, 0xB5, 0xDF, 0x9F, 0x18, 0xB7, 0xD7, 0xB0, 0xDF, 0x3E, 0xC6, 0xF1, 0x95, 0xEE, 0xEF, 0xE1, 0x0F, 0x20, 0x7C, 0xD1, 0x95, 0x39, 0xEA, 0x4F, 0xF8, 0x03, 0x08, 0x7F, 0x88, 0x71, 0x7B, 0x01, 0xE3, 0x0F, 0x35, 0x8E, 0xAF, 0x74, 0xBE, 0x87, 0x3F, 0x8A, 0xF0, 0x45, 0x57, 0xE6, 0x2C, 0x9F, 0xF0, 0x47, 0x91, 0xED, 0x38, 0xC4, 0xAA, 0x35, 0x1A, 0x6F, 0xDC, 0xB9, 0xC3, 0xAF, 0x51, 0x81, 0x71, 0x32, 0xF5, 0x31, 0x81, 0x7C, 0x8C, 0x43, 0x8D, 0x0A, 0xE0, 0x43, 0xF4, 0x26, 0x90, 0x6D, 0xA1, 0x17, 0x6F, 0x91, 0x87, 0x55, 0x4C, 0x58, 0x85, 0xA8, 0x47, 0x11, 0x61, 0x15, 0x53, 0x3D, 0x7E, 0x65, 0xDC, 0xDE, 0xC2, 0xF5, 0x98, 0x6E, 0x1C, 0x5F, 0xE9, 0x39, 0x1E, 0xFE, 0x5C, 0xC2, 0x9F, 0x8E, 0x7A, 0xCC, 0x21, 0xFC, 0xB9, 0xD4, 0x57, 0xF3, 0xD0, 0x67, 0xBA, 0x36, 0xE6, 0x1B, 0xC7, 0xD3, 0xBD, 0x49, 0x64, 0xF3, 0x4D, 0x7C, 0xCF, 0x67, 0x82, 0xAE, 0xC2, 0x1E, 0x52, 0x45, 0x7B, 0xC8, 0x02, 0x6F, 0x0F, 0x59, 0x68, 0x1C, 0x2F, 0x4A, 0xFB, 0xD9, 0x42, 0x13, 0xBF, 0x9F, 0xDD, 0xC7, 0x9A, 0xAA, 0x00, 0x5D, 0x41, 0x78, 0x8B, 0xBC, 0xBD, 0x65, 0xB1, 0x71, 0xBC, 0x28, 0x9D, 0xB1, 0x16, 0x9B, 0xF8, 0x33, 0xD6, 0x46, 0xC8, 0x4A, 0x68, 0xBD, 0x2E, 0x41, 0xEC, 0x4B, 0x28, 0xF7, 0x95, 0xC6, 0xBD, 0x1F, 0xD0, 0xF5, 0xBA, 0xCA, 0x38, 0x9E, 0xAE, 0xD7, 0x65, 0xF6, 0x7E, 0x39, 0xAD, 0x57, 0x91, 0x2F, 0xB5, 0x63, 0x19, 0xB0, 0x96, 0xA2, 0xA6, 0xAB, 0xC9, 0x4F, 0x29, 0x64, 0xA5, 0xE4, 0x67, 0x9D, 0x71, 0xEF, 0x1D, 0xD4, 0xCF, 0x7A, 0xE3, 0x78, 0xBA, 0x47, 0xAF, 0xB5, 0xF7, 0x6B, 0xC8, 0xCF, 0x7A, 0xD0, 0x6B, 0x81, 0xB5, 0x86, 0xB0, 0x36, 0x20, 0x66, 0xA5, 0xCB, 0x80, 0xAD, 0xFD, 0xBC, 0x11, 0x6B, 0x5E, 0xF4, 0xCA, 0xA8, 0x47, 0x2A, 0x8C, 0x93, 0x69, 0x1D, 0x37, 0x19, 0xA7, 0x13, 0xA5, 0xF3, 0xF5, 0x16, 0xD8, 0x6E, 0xF2, 0x6C, 0xB7, 0x1A, 0x27, 0xAB, 0x63, 0x7F, 0x4F, 0x09, 0xBD, 0x0D, 0xFD, 0xD5, 0x01, 0xB8, 0x5B, 0xC9, 0xFF, 0x4E, 0xAC, 0x27, 0xDF, 0xFF, 0x2B, 0xC6, 0xC9, 0x54, 0x6F, 0x2F, 0xF4, 0x7C, 0x5F, 0xFB, 0x8C, 0x93, 0xA9, 0xAF, 0xFD, 0xE4, 0x4B, 0x30, 0xF6, 0x91, 0xEC, 0x00, 0xC9, 0x24, 0xA6, 0xFD, 0x98, 0x0F, 0xC9, 0xF3, 0x00, 0x61, 0xFE, 0x06, 0xB9, 0xAB, 0xDD, 0x11, 0xB2, 0x13, 0xD9, 0x61, 0xB2, 0x13, 0x59, 0x29, 0x6A, 0xF4, 0x3B, 0x53, 0x7D, 0x6E, 0xD2, 0xBA, 0xBD, 0x69, 0x1C, 0x3F, 0x4A, 0x67, 0xE0, 0xE3, 0xA6, 0xFA, 0x0C, 0x9C, 0x46, 0xBD, 0x7B, 0xC2, 0x38, 0x99, 0x3E, 0x3B, 0xDF, 0x32, 0xCE, 0xFE, 0x04, 0xF9, 0x38, 0x69, 0xDC, 0x79, 0x85, 0x7D, 0xBC, 0x6D, 0x9C, 0xEE, 0x49, 0xD2, 0x7B, 0x07, 0xF3, 0xC5, 0x7A, 0x7F, 0x30, 0x4E, 0xF7, 0x1D, 0x9A, 0xD7, 0x53, 0xC6, 0xF1, 0xF9, 0xAC, 0xFA, 0x47, 0xE3, 0x7E, 0xDF, 0x9E, 0x22, 0xBD, 0x77, 0x8D, 0xE3, 0x0F, 0xE6, 0xF3, 0xAB, 0x71, 0x7C, 0x9E, 0x8F, 0xF7, 0x51, 0x3B, 0x7D, 0x4E, 0x7E, 0x80, 0xF7, 0x8B, 0x11, 0xE8, 0x4B, 0xDD, 0x84, 0xF7, 0x3E, 0xF5, 0xE4, 0x9F, 0xD1, 0x93, 0x0F, 0xCE, 0xB6, 0xC6, 0xF1, 0x74, 0xCD, 0x8A, 0xEC, 0xB4, 0x89, 0x5F, 0x2F, 0x6B, 0x41, 0x2F, 0x07, 0xBD, 0x9C, 0xF0, 0x3E, 0x44, 0xCF, 0x2B, 0xDE, 0x19, 0xE3, 0x78, 0x39, 0xB4, 0xCE, 0xCF, 0x98, 0xF8, 0x75, 0xBE, 0x0C, 0xB1, 0x9D, 0x45, 0x8E, 0x22, 0xFB, 0x08, 0xB2, 0x8F, 0x08, 0xFB, 0xAF, 0xC6, 0xBD, 0x03, 0x54, 0xEC, 0xBF, 0x43, 0x3F, 0x2D, 0xE4, 0xFA, 0x25, 0x76, 0x2E, 0x46, 0xBF, 0x3C, 0x02, 0x79, 0x5D, 0x3A, 0x6F, 0xFD, 0xD3, 0x38, 0x0C, 0xD1, 0x91, 0xB5, 0x7C, 0xCE, 0x5E, 0xCF, 0xD3, 0x5A, 0x16, 0xF9, 0xC7, 0x76, 0x9C, 0x83, 0xEF, 0x8F, 0x69, 0x4E, 0x3F, 0xA1, 0xFE, 0x92, 0x58, 0xE5, 0xDD, 0xE3, 0x27, 0xC8, 0xE3, 0x22, 0xF4, 0x2F, 0x52, 0xAC, 0x57, 0x8C, 0x7B, 0x3F, 0xB9, 0x08, 0xEF, 0x46, 0x3E, 0x33, 0x8E, 0xA7, 0xB1, 0xFF, 0x1B, 0xF6, 0x39, 0xA1, 0x6A, 0x9D, 0xCF, 0x8D, 0xE3, 0xEB, 0xBB, 0x96, 0xFF, 0x50, 0xAF, 0x7E, 0x61, 0xE2, 0xDF, 0xB5, 0x7C, 0x69, 0x9C, 0x5C, 0xF2, 0x16, 0xFA, 0x2B, 0xE4, 0xDD, 0x1E, 0x38, 0x22, 0xAF, 0x44, 0xDE, 0x5F, 0x1B, 0xE7, 0xFF, 0x2B, 0xE4, 0x7D, 0xC9, 0x5E, 0x2F, 0x53, 0xDE, 0x22, 0xFF, 0xD4, 0x8E, 0x4B, 0xC8, 0xE3, 0x53, 0xCA, 0xFB, 0x5B, 0xEA, 0x79, 0xC9, 0x5B, 0xDE, 0xB1, 0x7E, 0x8B, 0xBC, 0xAF, 0x42, 0xFF, 0x2A, 0xE5, 0x7D, 0xD3, 0xB8, 0xF7, 0xB0, 0x9A, 0xD3, 0x2D, 0xE3, 0x78, 0x9A, 0xF7, 0x1D, 0xD8, 0x73, 0xDE, 0x77, 0x8D, 0xE3, 0x6B, 0xDE, 0xF7, 0xBC, 0xBC, 0xF9, 0x3D, 0xD7, 0x7D, 0xE3, 0xE4, 0x9A, 0x77, 0x15, 0xE5, 0x2D, 0x38, 0xF7, 0x29, 0xEF, 0xEF, 0x8D, 0xF3, 0x5F, 0x85, 0xBC, 0xAF, 0xDB, 0xEB, 0x0D, 0xCA, 0x5B, 0xE4, 0xD7, 0xEC, 0xB8, 0x8E, 0x3C, 0xAE, 0x51, 0x1E, 0x41, 0xE0, 0xF2, 0x50, 0x3A, 0x1C, 0xB8, 0xF9, 0x7C, 0x1D, 0xD8, 0x49, 0x81, 0xD3, 0x89, 0xE0, 0xBD, 0x53, 0x4A, 0x50, 0x8D, 0x2B, 0xB2, 0x64, 0x3B, 0x52, 0xF0, 0x9E, 0x49, 0xEE, 0x35, 0xFF, 0x7A, 0x81, 0xC3, 0xD2, 0xF5, 0x95, 0x0A, 0x1C, 0xB9, 0x97, 0xF7, 0xD3, 0xF5, 0x82, 0x6A, 0x9F, 0x0D, 0x03, 0xD7, 0xEF, 0x22, 0x93, 0x77, 0xD5, 0x42, 0xF7, 0x42, 0x9F, 0x37, 0x0A, 0xDC, 0x77, 0x03, 0xB1, 0x17, 0x3B, 0x91, 0xEB, 0xBA, 0x6F, 0x1C, 0x54, 0xAF, 0xFB, 0x46, 0xC0, 0x16, 0xDE, 0x61, 0x8A, 0xBF, 0x19, 0xE2, 0xD0, 0xF8, 0xD3, 0x28, 0x7E, 0x91, 0x45, 0xEC, 0x48, 0x43, 0xFC, 0x11, 0x60, 0xA4, 0x53, 0xAC, 0x19, 0xB0, 0xD7, 0x58, 0xDB, 0x52, 0xAC, 0xF2, 0x9E, 0xBC, 0x2D, 0xC5, 0xDA, 0x0E, 0xB1, 0x8A, 0xBD, 0xD8, 0xB5, 0xA1, 0x58, 0xDB, 0x53, 0xAC, 0xED, 0x80, 0xDD, 0x1E, 0xB1, 0x4A, 0x8F, 0x69, 0x0C, 0x72, 0xD5, 0x7E, 0xE9, 0x08, 0xDF, 0xCB, 0x6D, 0x2E, 0x42, 0x77, 0x0E, 0x5C, 0xBF, 0xCB, 0xFA, 0xD0, 0xFC, 0x1E, 0x0D, 0x9C, 0x9E, 0xC8, 0x24, 0xBF, 0x2C, 0xCA, 0x4F, 0x64, 0x5D, 0xEC, 0xC8, 0x02, 0x76, 0x17, 0x9A, 0x9F, 0x9E, 0xC0, 0xD6, 0x7E, 0xCF, 0xA6, 0x9C, 0xE5, 0xFD, 0x7E, 0x4F, 0xCA, 0xB9, 0x17, 0xE5, 0x2C, 0xEF, 0xFA, 0x7B, 0x51, 0xCE, 0xB9, 0xC8, 0x59, 0xEC, 0xC5, 0x2E, 0x87, 0x72, 0xCE, 0xA3, 0x9C, 0x73, 0x81, 0x9D, 0x47, 0x39, 0x6B, 0x5C, 0x72, 0x15, 0x5A, 0xFB, 0x28, 0x85, 0x6A, 0xD0, 0x1B, 0x71, 0x69, 0x0D, 0xFA, 0x06, 0xAE, 0xF7, 0xB9, 0x06, 0x3F, 0x0D, 0x9C, 0x5E, 0x5F, 0xD4, 0xE0, 0x49, 0xAA, 0x81, 0xC8, 0xFA, 0xD9, 0xF1, 0x24, 0xB0, 0xFB, 0x51, 0x0D, 0x06, 0x02, 0x9B, 0x9F, 0x5F, 0x83, 0xBD, 0xBE, 0x95, 0x6F, 0x19, 0x03, 0x11, 0xBB, 0x7C, 0xC7, 0x18, 0x4C, 0x75, 0x19, 0x4E, 0x75, 0x91, 0x6F, 0x1A, 0xC3, 0xA9, 0x2E, 0x23, 0x50, 0x17, 0xB1, 0x17, 0xBB, 0x61, 0x54, 0x97, 0x91, 0x54, 0x97, 0x11, 0xC0, 0x1E, 0xE9, 0xF5, 0xED, 0x68, 0xAF, 0x6F, 0xC7, 0x52, 0x4E, 0x22, 0x1B, 0x63, 0xC7, 0x58, 0xE4, 0x34, 0x06, 0x18, 0xE3, 0x29, 0xD6, 0x02, 0xAF, 0x6F, 0x27, 0x52, 0xAC, 0xF2, 0x3D, 0x65, 0x22, 0xC5, 0x3A, 0x09, 0xB1, 0x8A, 0xBD, 0xD8, 0x4D, 0xA0, 0x58, 0x27, 0x53, 0xAC, 0x93, 0x80, 0x3D, 0x99, 0xE6, 0x50, 0x63, 0x18, 0x4B, 0x73, 0x56, 0xE8, 0xF5, 0xED, 0x73, 0x35, 0xF4, 0xED, 0xF3, 0x81, 0xD3, 0x7B, 0x0E, 0x73, 0x56, 0x4C, 0xF9, 0x89, 0xAC, 0xC8, 0x8E, 0x62, 0x60, 0x17, 0xD1, 0x9C, 0x4D, 0xF7, 0xFA, 0x76, 0x06, 0xE5, 0x2C, 0xDF, 0x81, 0xA6, 0x53, 0xCE, 0xB3, 0x29, 0x67, 0xF9, 0x26, 0x34, 0x9B, 0x72, 0x9E, 0x83, 0x9C, 0xC5, 0x5E, 0xEC, 0x66, 0x51, 0xCE, 0x73, 0x29, 0xE7, 0x39, 0xC0, 0x9E, 0x4B, 0x39, 0x6B, 0x5C, 0xC5, 0xE8, 0x5B, 0xED, 0x2D, 0xB9, 0x6A, 0x7E, 0xF3, 0xBD, 0xF9, 0x5B, 0x48, 0xF9, 0x89, 0x6C, 0x81, 0x1D, 0x0B, 0x61, 0xB7, 0x80, 0xF2, 0x2B, 0x41, 0x4F, 0x6A, 0xFF, 0xC9, 0xB7, 0xAB, 0x12, 0xC4, 0xB0, 0xC2, 0x9B, 0xD3, 0x55, 0x94, 0x9F, 0x7C, 0xC3, 0x5A, 0x45, 0xF9, 0xAD, 0x46, 0x7E, 0x62, 0x2F, 0x76, 0x2B, 0x29, 0xBF, 0x52, 0xCA, 0x6F, 0x35, 0xB0, 0x4B, 0x29, 0x3F, 0x8D, 0x6B, 0x21, 0xCD, 0xE9, 0x4B, 0xDE, 0x9C, 0xAE, 0xAF, 0x61, 0x4E, 0x37, 0x04, 0x4E, 0x6F, 0x3D, 0xE6, 0xB4, 0x9C, 0x72, 0x16, 0x59, 0x99, 0x1D, 0xE5, 0xC0, 0x2E, 0xA3, 0x9C, 0x2B, 0x28, 0x67, 0xE5, 0x6D, 0xF6, 0xE6, 0x59, 0xBE, 0xC9, 0x55, 0x20, 0x56, 0xF9, 0x1E, 0xB7, 0x99, 0xEA, 0xB0, 0x9D, 0xEA, 0x20, 0xDF, 0xE6, 0xB6, 0x53, 0x1D, 0x76, 0xA0, 0x0E, 0x62, 0x2F, 0x76, 0xDB, 0xA8, 0x0E, 0x3B, 0xA9, 0x0E, 0x3B, 0x80, 0xBD, 0x93, 0xEA, 0xA0, 0xB1, 0x96, 0x63, 0x9E, 0x6F, 0xE0, 0xF9, 0x79, 0x83, 0x9E, 0x97, 0xAF, 0xE0, 0xF9, 0xA9, 0x71, 0xEF, 0x0A, 0x1C, 0x2F, 0x4A, 0xE7, 0x88, 0x5D, 0x41, 0xFC, 0x39, 0xE2, 0x3A, 0xF0, 0x2F, 0x83, 0xBE, 0x4C, 0x78, 0xBB, 0xF1, 0xFC, 0x55, 0xBC, 0x3D, 0x81, 0xE3, 0x45, 0xE9, 0x3C, 0xB6, 0x27, 0x88, 0x3F, 0x8F, 0x5D, 0x02, 0xDE, 0x79, 0xD0, 0xE7, 0x09, 0x6F, 0x6F, 0x10, 0x7F, 0x96, 0xDC, 0x17, 0x38, 0x9E, 0xE2, 0x89, 0x6C, 0x5F, 0x10, 0x7F, 0x16, 0x3D, 0x67, 0xE2, 0xBF, 0x1F, 0xC9, 0xB5, 0xCA, 0x9E, 0x3E, 0x72, 0xEC, 0xC8, 0x06, 0x4E, 0x1D, 0xBC, 0xBF, 0x4B, 0x8E, 0x7D, 0x4B, 0x0C, 0xC7, 0xBE, 0x87, 0xA6, 0xD0, 0xFD, 0x8F, 0xE8, 0xBE, 0x36, 0x7E, 0x03, 0x3F, 0x44, 0xBF, 0x43, 0xEA, 0x41, 0x47, 0x7F, 0xAF, 0xB4, 0x08, 0xD5, 0xFC, 0x7B, 0xA5, 0x25, 0x64, 0xFA, 0xBB, 0xA1, 0x15, 0x6C, 0x5B, 0x92, 0xCE, 0xC3, 0xF0, 0xAD, 0x39, 0xB6, 0x86, 0xDE, 0xC3, 0x64, 0x97, 0x8E, 0xEF, 0x69, 0xAD, 0xC9, 0x2E, 0x03, 0xB1, 0x3D, 0x78, 0x2F, 0x07, 0x3D, 0xE1, 0x7F, 0x6F, 0xE3, 0x75, 0xFF, 0xF7, 0xCC, 0x84, 0xFE, 0x0F, 0xF8, 0x02, 0xB4, 0x34
};
