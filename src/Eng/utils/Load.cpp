#include "Load.h"

#include <memory>
#include <sstream>

#include <Sys/AssetFile.h>
#include <Sys/MemBuf.h>

std::vector<uint8_t> Eng::LoadHDR(std::string_view name, int &out_w, int &out_h) {
    Sys::AssetFile in_file(name);
    const size_t in_file_size = in_file.size();
    std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);

    in_file.Read((char *)&in_file_data[0], in_file_size);

    Sys::MemBuf mem_buf(&in_file_data[0], in_file_size);
    std::istream in_stream(&mem_buf);

    std::string line;
    if (!std::getline(in_stream, line) || line != "#?RADIANCE") {
        throw std::runtime_error("Is not HDR file!");
    }

    float exposure = 1.0f;
    std::string format;

    while (std::getline(in_stream, line)) {
        if (line.empty()) break;

        if (!line.compare(0, 6, "FORMAT")) {
            format = line.substr(7);
        } else if (!line.compare(0, 8, "EXPOSURE")) {
            exposure = float(atof(line.substr(9).c_str()));
        }
    }

    if (format != "32-bit_rle_rgbe") {
        throw std::runtime_error("Wrong format!");
    }

    int res_x = 0, res_y = 0;

    std::string resolution;
    if (!std::getline(in_stream, resolution)) {
        throw std::runtime_error("Cannot read resolution!");
    }

    {
        std::stringstream ss(resolution);
        std::string tok;

        ss >> tok;
        if (tok != "-Y") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_y = atoi(tok.c_str());

        ss >> tok;
        if (tok != "+X") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_x = atoi(tok.c_str());
    }

    if (!res_x || !res_y) {
        throw std::runtime_error("Unsupported format!");
    }

    out_w = res_x;
    out_h = res_y;

    std::vector<uint8_t> data(res_x * res_y * 4);
    int data_offset = 0;

    int scanlines_left = res_y;
    std::vector<uint8_t> _scanline(res_x * 4);
    uint8_t *scanline = _scanline.data();

    while (scanlines_left) {
        {
            uint8_t rgbe[4];

            if (!in_stream.read((char *)&rgbe[0], 4)) {
                throw std::runtime_error("Cannot read file!");
            }

            if ((rgbe[0] != 2) || (rgbe[1] != 2) || ((rgbe[2] & 0x80) != 0)) {
                data[0] = rgbe[0];
                data[1] = rgbe[1];
                data[2] = rgbe[2];
                data[3] = rgbe[3];

                size_t read_size = (res_x * scanlines_left - 1) * 4;
                if (read_size && !in_stream.read((char *)&data[4], read_size)) {
                    throw std::runtime_error("Cannot read file!");
                }
                return data;
            }

            if ((((rgbe[2] & 0xFF) << 8) | (rgbe[3] & 0xFF)) != res_x) {
                throw std::runtime_error("Wrong scanline width!");
            }
        }

        int index = 0;
        for (int i = 0; i < 4; i++) {
            int index_end = (i + 1) * res_x;
            while (index < index_end) {
                uint8_t buf[2];
                if (!in_stream.read((char *)&buf[0], 2)) {
                    throw std::runtime_error("Cannot read file!");
                }

                if (buf[0] > 128) {
                    int count = buf[0] - 128;
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    while (count-- > 0) {
                        scanline[index++] = buf[1];
                    }
                } else {
                    int count = buf[0];
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    scanline[index++] = buf[1];
                    if (--count > 0) {
                        if (!in_stream.read((char *)&scanline[index], count)) {
                            throw std::runtime_error("Cannot read file!");
                        }
                        index += count;
                    }
                }
            }
        }

        for (int i = 0; i < res_x; i++) {
            data[data_offset++] = scanline[i + 0 * res_x];
            data[data_offset++] = scanline[i + 1 * res_x];
            data[data_offset++] = scanline[i + 2 * res_x];
            data[data_offset++] = scanline[i + 3 * res_x];
        }

        scanlines_left--;
    }

    return data;
}
