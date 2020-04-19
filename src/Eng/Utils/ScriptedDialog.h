#pragma once

#include <vector>

#include <Sys/Json.h>

namespace Ren {
class Context;
}

class SceneManager;
class ScriptedSequence;

class ScriptedDialog {
    Ren::Context &ctx_;
    SceneManager &scene_manager_;

    std::vector<ScriptedSequence> sequences_;

  public:
    ScriptedDialog(Ren::Context &ctx, SceneManager &scene_manager);

    int GetSequencesCount() const { return (int)sequences_.size(); }

    ScriptedSequence *GetSequence(int i) { return &sequences_[i]; }

    void Clear();

    bool Load(const JsObject &js_seq,
              bool (*read_sequence)(const char *name, JsObject &js_seq));
};