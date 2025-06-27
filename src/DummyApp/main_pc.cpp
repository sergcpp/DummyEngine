#include "DummyApp.h"

#include <fstream>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <Sys/ScopeExit.h>

int main(int argc, char *argv[]) {

    std::vector<uint8_t> all_data;

    for (int i = 0; i < 64; ++i) {
        std::string file_name = "stbn/stbn_scalar_2Dx1Dx1D_128x128x64x1_";
        file_name += std::to_string(i);
        file_name += ".png";

        int w, h, channels;
        unsigned char *data = stbi_load(file_name.c_str(), &w, &h, &channels, 0);
        SCOPE_EXIT({ stbi_image_free(data); })

        const int off_x = 24, off_y = 56;

        for (int y = off_y; y < off_y + 64; ++y) {
            for (int x = off_x; x < off_x + 64; ++x) {
                all_data.push_back(data[y * w + x]);
            }
        }

        //all_data.insert(all_data.end(), data, data + w * h * channels);
    }

    { // dump C arrays
        std::ofstream out_file("__bn_sampler_new.inl", std::ios::binary);
        out_file << "const int w = " << 64 << ";\n";
        out_file << "const int h = " << 64 << ";\n";
        out_file << "const int d = " << 64 << ";\n";
        out_file << "const uint8_t bn_samples[" << 64 * 64 * 64 << "] = {\n    ";
        for (int i = 0; i < int(all_data.size()); ++i) {
            out_file << int(all_data[i]);
            if (i != all_data.size() - 1) {
                out_file << "u, ";
            } else {
                out_file << "u\n";
            }
        }
        out_file << "};\n";
    }

    return DummyApp().Run(argc, argv);
}
