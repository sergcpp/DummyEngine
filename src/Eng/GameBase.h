#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "FrameInfo.h"

using CommandHandler = std::function<bool(const std::vector<std::string> &)>;

class GameBase {
protected:
    std::map<std::string, std::shared_ptr<void>> components_;
    std::map<std::string, CommandHandler> command_handers_;
    FrameInfo fr_info_;
public:
    GameBase(int w, int h, const char *local_dir);
    virtual ~GameBase();

    virtual void Resize(int w, int h);

    virtual void RegisterCommand(const std::string &cmd, const CommandHandler &handler);
    virtual bool ExecuteCommand(const std::string &cmd, const std::vector<std::string> &args);

    void Start();
    void Frame();
    void Quit();

    template <class T>
    void AddComponent(const std::string &name, const std::shared_ptr<T> &p) {
        components_[name] = p;
    }

    template <class T>
    std::shared_ptr<T> GetComponent(const std::string &name) {
        auto it = components_.find(name);
        if (it != components_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    std::atomic_bool terminated;
    int width, height;
};

