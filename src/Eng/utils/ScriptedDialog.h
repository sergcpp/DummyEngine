#pragma once

#include <vector>

#include <Sys/Json.h>

namespace Ren {
class Context;
}

namespace Snd {
class Context;
}

namespace Eng {
class SceneManager;
class ScriptedSequence;
class ScriptedDialog {
    Ren::Context &ren_ctx_;
    Snd::Context &snd_ctx_;
    SceneManager &scene_manager_;

    std::vector<ScriptedSequence> sequences_;

  public:
    ScriptedDialog(Ren::Context &ren_ctx, Snd::Context &snd_ctx, SceneManager &scene_manager);

    bool empty() const { return sequences_.empty(); }

    int GetSequencesCount() const { return (int)sequences_.size(); }

    ScriptedSequence *GetSequence(int i) { return &sequences_[i]; }

    void Clear();

    bool Load(const char *lookup_name, const JsObject &js_seq,
              bool (*read_sequence)(const char *name, JsObject &js_seq));
};
} // namespace Eng