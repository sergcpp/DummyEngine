#pragma once

#include <utility>

namespace Sys {
typedef void(*onload_func)(void *arg, void *data, int size);
typedef void(*onerror_func)(void *arg);

void LoadAssetComplete(const char *url, void *arg, onload_func onload, onerror_func onerror);

template<typename T1, typename T2>
void LoadAssetComplete(const char *url, T1 onload, T2 onerror) {
    auto _arg = new std::pair<T1, T2>(onload, onerror);
    LoadAssetComplete(url, _arg, [](void *arg, void *data, int size) {
        auto funcs = (std::pair<T1, T2> *)arg;
        funcs->first(data, size);
        delete funcs;
    }, [](void *arg) {
        auto funcs = (std::pair<T1, T2> *)arg;
        funcs->second();
        delete funcs;
    });
}

void InitWorker();
void StopWorker();
}

