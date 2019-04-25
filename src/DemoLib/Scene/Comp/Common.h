#pragma once

#include <Ren/Storage.h>

struct JsObject;

struct ComponentBase : public Ren::RefCounter {
    virtual void Read(const JsObject &js_in) = 0;
    virtual void Write(JsObject &js_out) = 0;
};