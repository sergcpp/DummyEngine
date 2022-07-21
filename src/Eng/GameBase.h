#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "FrameInfo.h"

struct TimeInterval {
    uint64_t start_timepoint_us = 0, end_timepoint_us = 0;
};

class GameBase {
  protected:
    std::map<std::string, std::shared_ptr<void>> components_;
    FrameInfo fr_info_;

    void InitOptickGPUProfiler();

  public:
    GameBase(int w, int h, const char *device_name);
    virtual ~GameBase();

    virtual void Resize(int w, int h);

    virtual void Start();
    virtual void Frame();
    virtual void Quit();

    template <class T> void AddComponent(const std::string &name, const std::shared_ptr<T> p) {
        components_[name] = std::move(p);
    }

    template <class T> std::shared_ptr<T> GetComponent(const std::string &name) {
        auto it = components_.find(name);
        if (it != components_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    std::atomic_bool terminated;
    int width, height;
};
