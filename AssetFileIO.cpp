#include "AssetFileIO.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>

void Sys::LoadAssetComplete(const char *url, void *arg, onload_func onload, onerror_func onerror) {
    emscripten_async_wget_data(url, arg, onload, onerror);
}
#else

#include <memory>

#include "AssetFile.h"
#include "AsyncFileReader.h"
#include "ThreadWorker.h"

//#define IMITATE_LONG_LOAD

namespace Sys {
std::unique_ptr<Sys::ThreadWorker> g_worker;
std::unique_ptr<char[]> g_file_read_buffer;
size_t g_file_read_buffer_size;
Sys::AsyncFileReader g_file_reader;
}

void Sys::LoadAssetComplete(const char *url, void *arg, onload_func onload, onerror_func onerror) {
    std::string url_str(url);
    g_worker->AddTask([url_str, arg, onload, onerror] {
#if defined(IMITATE_LONG_LOAD)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif

        size_t file_size = 0;
        bool success = false;
#if !defined(__ANDROID__)
        success = g_file_reader.ReadFile(url_str.c_str(), g_file_read_buffer_size, &g_file_read_buffer[0], file_size);
#else
        AssetFile in_file(url_str.c_str(), AssetFile::FileIn);
        if (in_file) {
            file_size = in_file.size();
            if (file_size <= g_file_read_buffer_size)  {
                success = in_file.Read(&g_file_read_buffer[0], file_size);
            } else {
                success = false;
            }
        } else {
            success = false;
        }
#endif

        if (!success && file_size) {
            while (file_size > g_file_read_buffer_size) {
                g_file_read_buffer_size *= 2;
            }
            g_file_read_buffer.reset(new char[g_file_read_buffer_size]);
#if !defined(__ANDROID__)
            success = g_file_reader.ReadFile(url_str.c_str(), g_file_read_buffer_size, &g_file_read_buffer[0], file_size);
#else
            success = in_file.Read(&g_file_read_buffer[0], file_size);
#endif
        }

        if (success) {
            if (onload) {
                onload(arg, &g_file_read_buffer[0], (int)file_size);
            }
        } else {
            if (onerror) {
                onerror(arg);
            }
        }
    });
}

void Sys::InitWorker() {
    g_worker.reset(new Sys::ThreadWorker);
    g_file_read_buffer_size = 16 * 1024 * 1024;
    g_file_read_buffer.reset(new char[g_file_read_buffer_size]);
}

void Sys::StopWorker() {
    g_worker.reset();
    g_file_read_buffer.reset();
}

#endif